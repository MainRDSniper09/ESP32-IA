#include "JobManager.h"

#include <WiFi.h>

#include "../core/Agent.h"
#include "../core/PromptBuilder.h"
#include "../core/TimeUtils.h"
#include "../llm/LlmClient.h"
#include "Notifiers.h"

namespace agent {

namespace {

const char* kWeekdays[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};

class MutexGuard {
 public:
  explicit MutexGuard(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
  ~MutexGuard() { xSemaphoreGive(m_); }
 private:
  SemaphoreHandle_t m_;
};

const char* statusName(uint8_t s) {
  switch (s) {
    case 1: return "queued";
    case 2: return "running";
    case 3: return "done";
    default: return "unknown";
  }
}

}  // namespace

void JobManager::begin() {
  mutex_ = xSemaphoreCreateMutex();
  queue_ = xQueueCreate(AGENT_JOB_HISTORY, sizeof(uint32_t));
  // Núcleo 0 (el de la pila Wi-Fi), prioridad baja: puede bloquearse en HTTPS
  // sin afectar a la tarea de E/S del núcleo 1. Pila amplia para TLS + JSON.
  xTaskCreatePinnedToCore(taskEntry, "net", 16384, this, 1, nullptr, 0);
}

uint32_t JobManager::submit(const std::string& text, const std::string& prevRequest, const std::string& prevQuestion) {
  MutexGuard g(mutex_);
  // Buscar un hueco: libre, o el job terminado más antiguo.
  Job* slot = nullptr;
  for (size_t i = 0; i < AGENT_JOB_HISTORY; ++i) {
    Job& j = jobs_[i];
    if (j.status == Status::Free) {
      slot = &j;
      break;
    }
    if (j.status == Status::Done && (!slot || j.id < slot->id)) slot = &j;
  }
  if (!slot) return 0;  // todos en cola/ejecución

  slot->id = nextId_++;
  slot->status = Status::Queued;
  slot->text = text;
  slot->prevRequest = prevRequest;
  slot->prevQuestion = prevQuestion;
  slot->result.clear();
  slot->createdMs = millis();
  slot->finishedMs = 0;
  uint32_t id = slot->id;
  if (xQueueSend(queue_, &id, 0) != pdTRUE) {
    slot->status = Status::Free;
    return 0;
  }
  return id;
}

uint8_t JobManager::pending() {
  MutexGuard g(mutex_);
  uint8_t n = 0;
  for (size_t i = 0; i < AGENT_JOB_HISTORY; ++i) {
    if (jobs_[i].status == Status::Queued || jobs_[i].status == Status::Running) ++n;
  }
  return n;
}

bool JobManager::writeJob(uint32_t id, JsonObject out) {
  MutexGuard g(mutex_);
  for (size_t i = 0; i < AGENT_JOB_HISTORY; ++i) {
    const Job& j = jobs_[i];
    if (j.id != id || j.status == Status::Free) continue;
    out["job_id"] = j.id;
    out["status"] = statusName(static_cast<uint8_t>(j.status));
    out["input"] = j.text;
    out["elapsed_ms"] = (j.finishedMs ? j.finishedMs : millis()) - j.createdMs;
    if (j.status == Status::Done) {
      JsonDocument tmp;
      if (!deserializeJson(tmp, j.result)) out["result"] = tmp.as<JsonObjectConst>();
    }
    return true;
  }
  return false;
}

void JobManager::taskEntry(void* self) { static_cast<JobManager*>(self)->run(); }

void JobManager::run() {
  for (;;) {
    uint32_t id = 0;
    if (xQueueReceive(queue_, &id, pdMS_TO_TICKS(500)) == pdTRUE) process(id);
    // Entre trabajos, despachar notificaciones externas pendientes.
    telegram_.process(WiFi.status() == WL_CONNECTED);
  }
}

void JobManager::process(uint32_t id) {
  std::string text, prevRequest, prevQuestion;
  {
    MutexGuard g(mutex_);
    Job* job = nullptr;
    for (size_t i = 0; i < AGENT_JOB_HISTORY; ++i) {
      if (jobs_[i].id == id) job = &jobs_[i];
    }
    if (!job || job->status != Status::Queued) return;
    job->status = Status::Running;
    text = job->text;
    prevRequest = job->prevRequest;
    prevQuestion = job->prevQuestion;
  }

  JsonDocument doc;
  JsonObject r = doc.to<JsonObject>();
  const std::string combined = prevRequest.empty() ? text : prevRequest + " " + text;

  // 1) Barrera determinista: expresiones ambiguas -> aclaración sin gastar una llamada al LLM.
  if (!agent_.precheckText(combined, r)) {
    // 2) Consulta al LLM.
    std::string nowIso, weekday;
    if (clock_.isSynced()) {
      time_t now = clock_.now();
      nowIso = timeutil::toIso8601(now);
      struct tm tmv;
      timeutil::toLocal(now, tmv);
      weekday = kWeekdays[tmv.tm_wday % 7];
    }
    static const std::string system = prompt::systemPrompt();  // se construye una sola vez
    std::string user = prompt::userMessage(text, nowIso, weekday, prevRequest, prevQuestion);
    LlmResult llm = llm_.complete(system, user);

    JsonObject meta = r["llm"].to<JsonObject>();
    meta["provider"] = llm_.providerName();
    meta["model"] = llm.model.empty() ? std::string(llm_.model()) : llm.model;
    meta["latency_ms"] = llm.latencyMs;
    meta["http_status"] = llm.httpStatus;
    meta["attempts"] = llm.attempts;

    if (!llm.ok) {
      // 3a) Falla del LLM: no se ejecuta nada; el sistema sigue operando localmente.
      r["outcome"] = "error";
      r["message"] = llm.errorMessage + ". No se ejecutó ninguna acción.";
      JsonObject e = r["errors"].to<JsonArray>().add<JsonObject>();
      e["code"] = llm.errorCode;
      e["field"] = "";
      e["message"] = llm.errorMessage;
      agent_.logEvent(EventLevel::Warning, "llm", "Fallo al consultar el LLM: " + llm.errorMessage);
    } else {
      // 3b) Respuesta: validar y ejecutar (todo determinista en el firmware).
      r["llm_output"] = llm.text.size() > 1500 ? llm.text.substr(0, 1500) + "…" : llm.text;
      agent_.handleLlmText(llm.text, combined, r);
    }
  }

  std::string serialized;
  serializeJson(doc, serialized);
  Serial.printf("[job %lu] %s\n", static_cast<unsigned long>(id), serialized.c_str());

  MutexGuard g(mutex_);
  for (size_t i = 0; i < AGENT_JOB_HISTORY; ++i) {
    if (jobs_[i].id == id) {
      jobs_[i].result = serialized;
      jobs_[i].status = Status::Done;
      jobs_[i].finishedMs = millis();
    }
  }
}

}  // namespace agent

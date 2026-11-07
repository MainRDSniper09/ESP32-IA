#include "WebApi.h"

#include "../config/AppConfig.h"
#include "../config/Secrets.h"
#include "../core/ActionCatalog.h"
#include "../core/Agent.h"
#include "../core/DeviceCatalog.h"
#include "../core/TimeUtils.h"
#include "../llm/LlmClient.h"
#include "../platform/JobManager.h"
#include "../platform/NetworkManager.h"
#include "../platform/Notifiers.h"
#include "WebPage.h"

namespace agent {

namespace {

const size_t kMaxBody = 4096;

/** Comparación en tiempo constante (evita ataques de temporización sobre el token). */
bool safeEquals(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); ++i) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

int httpCodeFor(const char* outcome) {
  if (strcmp(outcome, "rejected") == 0) return 422;
  if (strcmp(outcome, "error") == 0) return 502;
  return 200;
}

}  // namespace

// ===========================================================================
// Infraestructura
// ===========================================================================

void WebApi::begin() {
  static const char* kHeaders[] = {"Authorization"};
  server_.collectHeaders(kHeaders, 1);

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/health", HTTP_GET, [this]() { handleHealth(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/catalog", HTTP_GET, [this]() { handleCatalog(); });
  server_.on("/api/devices", HTTP_GET, [this]() { handleDevices(); });
  server_.on("/api/devices/set", HTTP_POST, [this]() { handleDeviceSet(); });
  server_.on("/api/accesses", HTTP_GET, [this]() { handleAccesses(); });
  server_.on("/api/command", HTTP_POST, [this]() { handleCommand(); });
  server_.on("/api/jobs", HTTP_GET, [this]() { handleJob(); });
  server_.on("/api/plan", HTTP_POST, [this]() { handlePlan(false); });
  server_.on("/api/plan/validate", HTTP_POST, [this]() { handlePlan(true); });
  server_.on("/api/tasks", HTTP_GET, [this]() { handleTasks(); });
  server_.on("/api/tasks", HTTP_DELETE, [this]() { handleTasksDelete(); });
  server_.on("/api/tasks/cancel", HTTP_POST, [this]() { handleTasksCancel(); });
  server_.on("/api/events", HTTP_GET, [this]() { handleEvents(); });
  server_.on("/api/events/ack", HTTP_POST, [this]() { handleEventsAck(); });
  server_.onNotFound([this]() { handleNotFound(); });
  server_.begin();
  Serial.println("[http] Servidor web en el puerto 80");
}

void WebApi::addCors() {
  server_.sendHeader("Access-Control-Allow-Origin", "*");
  server_.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  server_.sendHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
}

bool WebApi::authorize() {
  if (strlen(SECRET_DEVICE_API_TOKEN) == 0) return true;
  String expected = String("Bearer ") + SECRET_DEVICE_API_TOKEN;
  if (safeEquals(server_.header("Authorization"), expected)) return true;
  sendError(401, "unauthorized", "Falta o es incorrecta la cabecera Authorization: Bearer <token>");
  return false;
}

bool WebApi::parseBody(JsonDocument& doc) {
  if (!server_.hasArg("plain") || server_.arg("plain").length() == 0) {
    sendError(400, "empty_body", "Se esperaba un cuerpo JSON");
    return false;
  }
  const String& body = server_.arg("plain");
  if (body.length() > kMaxBody) {
    sendError(413, "body_too_large", "El cuerpo supera " + String(kMaxBody) + " bytes");
    return false;
  }
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    sendError(400, "invalid_json", String("JSON mal formado: ") + err.c_str());
    return false;
  }
  return true;
}

void WebApi::sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  addCors();
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json; charset=utf-8", out);
}

void WebApi::sendError(int code, const char* errCode, const String& message) {
  JsonDocument doc;
  doc["error"]["code"] = errCode;
  doc["error"]["message"] = message;
  sendJson(code, doc);
}

// ===========================================================================
// Rutas
// ===========================================================================

void WebApi::handleRoot() {
  server_.sendHeader("Cache-Control", "no-cache");
  server_.send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void WebApi::handleHealth() {
  JsonDocument doc;
  doc["status"] = "ok";
  doc["firmware"] = AGENT_FIRMWARE_VERSION;
  doc["uptime_s"] = clock_.uptimeSeconds();
  doc["auth_required"] = strlen(SECRET_DEVICE_API_TOKEN) > 0;
  sendJson(200, doc);
}

void WebApi::handleStatus() {
  if (!authorize()) return;
  JsonDocument doc;
  doc["firmware"] = AGENT_FIRMWARE_VERSION;
  doc["uptime_s"] = clock_.uptimeSeconds();

  JsonObject t = doc["time"].to<JsonObject>();
  bool synced = clock_.isSynced();
  t["synced"] = synced;
  t["timezone"] = AGENT_TIMEZONE;
  if (synced) {
    t["epoch"] = static_cast<uint32_t>(clock_.now());
    t["local"] = timeutil::toIso8601(clock_.now());
  } else {
    t["epoch"] = nullptr;
    t["local"] = nullptr;
  }

  net_.writeStatus(doc["network"].to<JsonObject>());

  JsonObject l = doc["llm"].to<JsonObject>();
  l["provider"] = llm_.providerName();
  l["model"] = llm_.model();
  l["configured"] = llm_.configured();  // nunca se expone la clave
  l["jobs_pending"] = jobs_.pending();

  JsonObject n = doc["notifications"].to<JsonObject>();
  n["local_led"] = true;
  n["telegram"] = telegram_.enabled();
  n["telegram_sent"] = telegram_.sent();
  n["telegram_failed"] = telegram_.failed();

  agent_.writeSummary(doc["agent"].to<JsonObject>());

  JsonObject m = doc["memory"].to<JsonObject>();
  m["heap_free"] = ESP.getFreeHeap();
  m["heap_min"] = ESP.getMinFreeHeap();

  JsonObject s = doc["security"].to<JsonObject>();
  s["api_token_required"] = strlen(SECRET_DEVICE_API_TOKEN) > 0;
  s["secrets_file_missing"] = AGENT_USING_SECRET_TEMPLATE == 1;
  sendJson(200, doc);
}

void WebApi::handleCatalog() {
  if (!authorize()) return;
  JsonDocument doc;
  JsonArray acts = doc["actions"].to<JsonArray>();
  for (size_t i = 0; i < actions::count(); ++i) {
    const ActionSpec& a = actions::at(i);
    JsonObject o = acts.add<JsonObject>();
    o["name"] = a.name;
    o["description"] = a.description;
    o["schedulable"] = a.schedulable;
    JsonArray ps = o["params"].to<JsonArray>();
    for (uint8_t j = 0; j < a.paramCount; ++j) {
      const ParamSpec& p = a.params[j];
      JsonObject po = ps.add<JsonObject>();
      po["name"] = p.name;
      po["type"] = actions::paramTypeName(p.type);
      po["required"] = p.required;
      if (p.allowed) po["allowed"] = p.allowed;
      po["description"] = p.description;
    }
  }
  JsonArray devs = doc["devices"].to<JsonArray>();
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    JsonObject o = devs.add<JsonObject>();
    o["id"] = d.id;
    o["name"] = d.name;
    o["kind"] = devices::kindName(d.kind);
    o["io"] = isOutputKind(d.kind) ? "output" : "input";
    o["pin"] = d.pin;
  }
  JsonObject groups = doc["groups"].to<JsonObject>();
  JsonArray og = groups["outputs"].to<JsonArray>();
  og.add("lights");
  og.add("sprinklers");
  og.add("all");
  JsonArray ag = groups["accesses"].to<JsonArray>();
  ag.add("doors");
  ag.add("windows");
  ag.add("accesses");
  JsonArray conds = doc["conditions"].to<JsonArray>();
  conds.add("all_closed");
  conds.add("any_open");
  conds.add("is_open");
  conds.add("is_closed");
  JsonArray timing = doc["timing"].to<JsonArray>();
  timing.add("now");
  timing.add("at");
  timing.add("delay");
  timing.add("after_step");
  doc["limits"]["max_steps"] = AGENT_MAX_PLAN_STEPS;
  doc["limits"]["max_tasks"] = AGENT_MAX_TASKS;
  doc["limits"]["max_delay_s"] = AGENT_MAX_DELAY_S;
  doc["limits"]["max_command_chars"] = AGENT_MAX_COMMAND_CHARS;
  sendJson(200, doc);
}

void WebApi::handleDevices() {
  if (!authorize()) return;
  JsonDocument doc;
  agent_.writeDevices(doc["devices"].to<JsonArray>());
  sendJson(200, doc);
}

void WebApi::handleDeviceSet() {
  if (!authorize()) return;
  JsonDocument body;
  if (!parseBody(body)) return;
  const char* device = body["device"] | "";
  JsonVariant st = body["state"];
  bool on;
  if (st.is<bool>()) on = st.as<bool>();
  else if (st == "on") on = true;
  else if (st == "off") on = false;
  else {
    sendError(400, "invalid_state", "state debe ser \"on\", \"off\", true o false");
    return;
  }
  JsonDocument doc;
  agent_.manualSet(device, on, doc.to<JsonObject>());
  sendJson(httpCodeFor(doc["outcome"] | ""), doc);
}

void WebApi::handleAccesses() {
  if (!authorize()) return;
  String target = server_.hasArg("target") ? server_.arg("target") : String("accesses");
  String filter = server_.hasArg("filter") ? server_.arg("filter") : String("all");
  if (!devices::isValidAccessTarget(target.c_str())) {
    sendError(400, "unknown_target", "target debe ser doors, windows, accesses o el id de una puerta/ventana");
    return;
  }
  if (filter != "all" && filter != "open" && filter != "closed") {
    sendError(400, "invalid_filter", "filter debe ser all, open o closed");
    return;
  }
  JsonDocument doc;
  agent_.writeAccesses(doc["accesses"].to<JsonArray>(), target.c_str(), filter.c_str());
  sendJson(200, doc);
}

void WebApi::handleCommand() {
  if (!authorize()) return;
  JsonDocument body;
  if (!parseBody(body)) return;
  const char* text = body["text"] | "";
  size_t len = strlen(text);
  if (len == 0) {
    sendError(400, "missing_text", "Falta el campo 'text'");
    return;
  }
  if (len > AGENT_MAX_COMMAND_CHARS) {
    sendError(400, "text_too_long", "La instrucción supera " + String(AGENT_MAX_COMMAND_CHARS) + " caracteres");
    return;
  }
  const char* prevReq = body["context"]["request"] | "";
  const char* prevQ = body["context"]["question"] | "";
  uint32_t id = jobs_.submit(text, prevReq, prevQ);
  if (id == 0) {
    sendError(503, "busy", "Hay demasiadas instrucciones en proceso; intente en unos segundos");
    return;
  }
  JsonDocument doc;
  doc["job_id"] = id;
  doc["status"] = "queued";
  doc["poll"] = "/api/jobs?id=" + String(id);
  sendJson(202, doc);
}

void WebApi::handleJob() {
  if (!authorize()) return;
  long id = server_.hasArg("id") ? server_.arg("id").toInt() : 0;
  JsonDocument doc;
  if (id <= 0 || !jobs_.writeJob(static_cast<uint32_t>(id), doc.to<JsonObject>())) {
    sendError(404, "job_not_found", "No existe el job indicado (solo se conservan los últimos " +
                                        String(AGENT_JOB_HISTORY) + ")");
    return;
  }
  sendJson(200, doc);
}

void WebApi::handlePlan(bool dryRun) {
  if (!authorize()) return;
  JsonDocument body;
  if (!parseBody(body)) return;
  JsonDocument doc;
  JsonObject out = doc.to<JsonObject>();
  agent_.handlePlanJson(body.as<JsonVariantConst>(), "api /plan", dryRun, out);
  sendJson(httpCodeFor(doc["outcome"] | ""), doc);
}

void WebApi::handleTasks() {
  if (!authorize()) return;
  JsonDocument doc;
  agent_.writeTasks(doc["tasks"].to<JsonArray>());
  doc["count"] = doc["tasks"].size();
  doc["capacity"] = AGENT_MAX_TASKS;
  sendJson(200, doc);
}

void WebApi::cancelFromArgs(bool all, long id) {
  JsonDocument doc;
  if (all) {
    doc["cancelled"] = agent_.cancelAllTasks();
    sendJson(200, doc);
    return;
  }
  if (id <= 0) {
    sendError(400, "missing_id", "Indique id=<n> o all=1");
    return;
  }
  if (!agent_.cancelTask(static_cast<uint32_t>(id))) {
    sendError(404, "task_not_found", "No existe la tarea #" + String(id));
    return;
  }
  doc["cancelled"] = 1;
  doc["id"] = id;
  sendJson(200, doc);
}

void WebApi::handleTasksDelete() {
  if (!authorize()) return;
  bool all = server_.hasArg("all") && (server_.arg("all") == "1" || server_.arg("all") == "true");
  long id = server_.hasArg("id") ? server_.arg("id").toInt() : 0;
  cancelFromArgs(all, id);
}

void WebApi::handleTasksCancel() {
  if (!authorize()) return;
  JsonDocument body;
  if (!parseBody(body)) return;
  cancelFromArgs(body["all"] | false, body["id"] | 0L);
}

void WebApi::handleEvents() {
  if (!authorize()) return;
  long since = server_.hasArg("since") ? server_.arg("since").toInt() : 0;
  long limit = server_.hasArg("limit") ? server_.arg("limit").toInt() : 20;
  if (since < 0) since = 0;
  if (limit <= 0 || limit > AGENT_EVENT_LOG_SIZE) limit = AGENT_EVENT_LOG_SIZE;
  JsonDocument doc;
  agent_.writeEvents(doc["events"].to<JsonArray>(), static_cast<uint32_t>(since), static_cast<size_t>(limit));
  doc["last_id"] = agent_.events().lastId();
  doc["alerts_unacknowledged"] = agent_.unacknowledgedAlerts();
  sendJson(200, doc);
}

void WebApi::handleEventsAck() {
  if (!authorize()) return;
  agent_.acknowledgeAlerts();
  JsonDocument doc;
  doc["alerts_unacknowledged"] = 0;
  sendJson(200, doc);
}

void WebApi::handleNotFound() {
  if (server_.method() == HTTP_OPTIONS) {  // preflight CORS
    addCors();
    server_.send(204);
    return;
  }
  sendError(404, "not_found", "Ruta no encontrada: " + server_.uri());
}

}  // namespace agent

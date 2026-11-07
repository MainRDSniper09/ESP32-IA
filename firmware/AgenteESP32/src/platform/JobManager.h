/**
 * @file JobManager.h
 * @brief Cola de trabajos de lenguaje natural + tarea de red (FreeRTOS, núcleo 0).
 *
 * POST /api/command no espera al LLM: crea un "job", responde 202 con su id y
 * la tarea de red lo procesa en segundo plano. El cliente consulta
 * GET /api/jobs?id=N hasta que `status` sea "done".
 *
 * Estados: queued -> running -> done
 *
 * Así, una llamada lenta al LLM (2-20 s) nunca bloquea el servidor web, el
 * monitoreo de puertas/ventanas ni el planificador.
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <string>

#include "../config/AppConfig.h"

namespace agent {

class Agent;
class LlmClient;
class IClock;
class TelegramNotifier;

class JobManager {
 public:
  JobManager(Agent& agent, LlmClient& llm, IClock& clock, TelegramNotifier& telegram)
      : agent_(agent), llm_(llm), clock_(clock), telegram_(telegram) {}

  /** Crea la cola y la tarea de red. */
  void begin();

  /**
   * Encola una instrucción. `prevRequest`/`prevQuestion` se usan cuando el
   * texto responde a una aclaración anterior.
   * @return id del job, o 0 si la cola está llena.
   */
  uint32_t submit(const std::string& text, const std::string& prevRequest, const std::string& prevQuestion);

  /** Escribe el job en `out`. false si el id no existe (o ya fue reemplazado). */
  bool writeJob(uint32_t id, JsonObject out);

  /** Jobs en cola o en ejecución. */
  uint8_t pending();

 private:
  enum class Status : uint8_t { Free, Queued, Running, Done };
  struct Job {
    uint32_t id = 0;
    Status status = Status::Free;
    std::string text, prevRequest, prevQuestion;
    std::string result;  ///< JSON serializado del resultado
    uint32_t createdMs = 0;
    uint32_t finishedMs = 0;
  };

  static void taskEntry(void* self);
  void run();
  void process(uint32_t id);

  Agent& agent_;
  LlmClient& llm_;
  IClock& clock_;
  TelegramNotifier& telegram_;
  Job jobs_[AGENT_JOB_HISTORY];
  uint32_t nextId_ = 1;
  QueueHandle_t queue_ = nullptr;
  SemaphoreHandle_t mutex_ = nullptr;
};

}  // namespace agent

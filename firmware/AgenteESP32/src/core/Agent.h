/**
 * @file Agent.h
 * @brief Orquestador determinista: valida, planifica y ejecuta.
 *
 *                 ┌──────────────┐
 *   texto  ──────►│ AmbiguityGuard│── ambiguo ──► "clarification"
 *                 └──────┬───────┘
 *                        ▼ (lo llama la capa de red)
 *                    [ LLM remoto ]
 *                        ▼ texto con JSON
 *                 ┌──────────────┐
 *                 │ extractJson  │── sin JSON ──► "rejected"
 *                 │ PlanValidator│── inválido ──► "rejected" (+ lista de errores)
 *                 └──────┬───────┘
 *                        ▼ Plan tipado
 *                 ┌──────────────┐
 *                 │ resolve()    │  horas -> instantes epoch, capacidad, reloj
 *                 └──────┬───────┘
 *            inmediato   │   programado
 *          ┌─────────────┴──────────────┐
 *          ▼                            ▼
 *   execute() + condición       TaskScheduler (+ NVS)
 *          │                            │ tick() cuando vence
 *          ▼                            ▼
 *   IHardware (LED)  ◄──────────  execute() + condición
 *
 * El agente es seguro entre hilos: cada método público toma el `ILock`.
 * Las condiciones se evalúan SIEMPRE con el estado real de los sensores en el
 * momento de la ejecución, nunca con lo que "crea" el LLM.
 */
#pragma once

#include <ArduinoJson.h>

#include <string>
#include <vector>

#include "EventLog.h"
#include "Ports.h"
#include "TaskScheduler.h"
#include "Types.h"

namespace agent {

class Agent {
 public:
  Agent(IHardware& hw, IClock& clock, ITaskStore& store, ILock* lock = nullptr);

  /** Recupera las tareas guardadas en memoria no volátil. Llamar una vez al arrancar. */
  void begin();

  // ------------------------------------------------------------ Entradas
  /**
   * Barrera determinista previa al LLM. Si el texto es ambiguo escribe un
   * resultado "clarification" en `out` y devuelve true (no hace falta llamar al LLM).
   */
  bool precheckText(const std::string& text, JsonObject out);

  /**
   * Procesa la respuesta textual del LLM: extrae el JSON, lo valida y, si es
   * correcto, lo ejecuta/programa. Escribe el resultado en `out`.
   */
  void handleLlmText(const std::string& llmText, const std::string& origin, JsonObject out);

  /**
   * Procesa un plan JSON ya parseado (API /api/plan o la respuesta del LLM).
   * Con `dryRun = true` solo valida y resuelve horarios, sin ejecutar nada.
   */
  void handlePlanJson(JsonVariantConst plan, const std::string& origin, bool dryRun, JsonObject out);

  /** Control manual (botones de la interfaz). Pasa por el mismo validador. */
  void manualSet(const std::string& deviceId, bool on, JsonObject out);

  // ---------------------------------------------------------- Periódicos
  /** Ejecuta las tareas vencidas. Llamar cada AGENT_SCHEDULER_TICK_MS. */
  void tick();

  /** Notificación de cambio (con antirrebote) en una puerta/ventana. */
  void onAccessChanged(const DeviceInfo& dev, bool open);

  /** Registra un evento del sistema (Wi-Fi, NTP, LLM...). */
  void logEvent(EventLevel level, const std::string& source, const std::string& message);

  // ----------------------------------------------------------- Consultas
  void writeDevices(JsonArray out);
  void writeAccesses(JsonArray out, const std::string& target, const std::string& filter);
  void writeTasks(JsonArray out);
  void writeEvents(JsonArray out, uint32_t sinceId, size_t limit);
  void writeSummary(JsonObject out);  ///< Resumen del núcleo para /api/status

  bool cancelTask(uint32_t id);
  size_t cancelAllTasks();
  void acknowledgeAlerts();
  uint32_t unacknowledgedAlerts();
  size_t taskCount();

  EventLog& events() { return events_; }

 private:
  /** Resultado de evaluar una condición localmente. */
  struct CondResult {
    bool met;
    std::string detail;  ///< p. ej. "Abiertas: Puerta principal"
  };

  void submitPlan(const Plan& plan, const std::string& origin, bool dryRun, JsonObject out);
  bool resolveTimes(const Plan& plan, std::vector<time_t>& due, std::vector<ValidationError>& errors);
  void executeStep(const PlanStep& step, JsonObject result);
  CondResult evaluate(const Condition& c) const;
  std::string accessList(const std::string& target, bool open) const;

  // Catálogo de funciones autorizadas (una por ActionId).
  void doSetDevice(const ActionParams& p, JsonObject r);
  void doSetGroup(const ActionParams& p, JsonObject r);
  void doReadAccess(const ActionParams& p, JsonObject r);
  void doNotify(const ActionParams& p, const std::string& extra, JsonObject r);
  void doListTasks(JsonObject r);
  void doCancelTask(const ActionParams& p, JsonObject r);
  void doSystemStatus(JsonObject r);

  void writeTasksUnlocked(JsonArray out);
  void writeSummaryUnlocked(JsonObject out);
  void persist();
  void log(EventLevel level, const std::string& source, const std::string& message);
  static void writeErrors(const std::vector<ValidationError>& errors, JsonObject out);

  IHardware& hw_;
  IClock& clock_;
  ITaskStore& store_;
  ILock* lock_;
  TaskScheduler scheduler_;
  EventLog events_;
};

}  // namespace agent

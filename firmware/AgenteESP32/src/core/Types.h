/**
 * @file Types.h
 * @brief Tipos de dominio del agente (independientes del hardware).
 *
 * Todo lo que está en `src/core/` es C++ estándar + ArduinoJson: no incluye
 * Arduino.h ni APIs del ESP32. Gracias a eso se puede compilar y probar en un
 * PC (`pio test -e native`). El hardware se conecta mediante las interfaces
 * de `Ports.h`.
 *
 * Flujo de datos:
 *   texto -> LLM -> JSON crudo -> PlanValidator -> Plan (tipado) ->
 *   Agent::submitPlan -> ejecución inmediata | TaskScheduler (persistente)
 */
#pragma once

#include <stdint.h>
#include <time.h>

#include <string>
#include <vector>

namespace agent {

// ======================================================================
// Dispositivos
// ======================================================================

/** Tipo físico de un dispositivo del catálogo. */
enum class DeviceKind : uint8_t {
  Light,      ///< Luz (salida digital, LED)
  Sprinkler,  ///< Aspersor (salida digital, LED)
  Door,       ///< Puerta (entrada digital, interruptor)
  Window,     ///< Ventana (entrada digital, interruptor)
};

/** Entrada del catálogo estático de dispositivos. */
struct DeviceInfo {
  const char* id;    ///< Identificador canónico usado en el JSON: "luz_entrada"
  const char* name;  ///< Nombre legible: "Luz de la entrada"
  DeviceKind kind;
  uint8_t pin;       ///< GPIO del ESP32-S3
};

inline bool isOutputKind(DeviceKind k) { return k == DeviceKind::Light || k == DeviceKind::Sprinkler; }
inline bool isAccessKind(DeviceKind k) { return k == DeviceKind::Door || k == DeviceKind::Window; }

// ======================================================================
// Acciones (catálogo explícito de funciones autorizadas)
// ======================================================================

/** Identificador de cada acción permitida. El LLM nunca puede invocar otra cosa. */
enum class ActionId : uint8_t {
  SetDevice,     ///< Encender/apagar UNA salida
  SetGroup,      ///< Encender/apagar un grupo de salidas
  ReadAccess,    ///< Consultar puertas/ventanas
  Notify,        ///< Emitir una notificación/alerta
  ListTasks,     ///< Listar tareas programadas
  CancelTask,    ///< Cancelar una tarea (o todas)
  SystemStatus,  ///< Estado general del sistema
};

/** Parámetros ya validados de una acción. Solo se usan los campos de la acción. */
struct ActionParams {
  std::string device;     ///< SetDevice: id de salida
  std::string group;      ///< SetGroup: lights|sprinklers|all ; ReadAccess: doors|windows|accesses|<id>
  bool state = false;     ///< SetDevice/SetGroup: true = encender
  std::string filter;     ///< ReadAccess: all|open|closed
  std::string message;    ///< Notify
  uint8_t level = 1;      ///< Notify: 0 info, 1 warning, 2 alert (ver EventLevel)
  uint32_t taskId = 0;    ///< CancelTask: id concreto (0 = no indicado)
  bool all = false;       ///< CancelTask: cancelar todas
};

struct Action {
  ActionId id = ActionId::SystemStatus;
  ActionParams params;
};

// ======================================================================
// Condiciones (se evalúan LOCALMENTE con los sensores, nunca con el LLM)
// ======================================================================

enum class ConditionType : uint8_t {
  None,       ///< Sin condición: se ejecuta siempre
  AllClosed,  ///< Todos los accesos del objetivo están cerrados
  AnyOpen,    ///< Al menos un acceso del objetivo está abierto
  IsOpen,     ///< El acceso indicado está abierto
  IsClosed,   ///< El acceso indicado está cerrado
};

struct Condition {
  ConditionType type = ConditionType::None;
  std::string target;  ///< doors|windows|accesses|<id de puerta/ventana>
};

// ======================================================================
// Temporización
// ======================================================================

enum class TimingType : uint8_t {
  Now,        ///< Inmediato
  At,         ///< A una hora local HH:MM (hoy, o mañana si ya pasó)
  Delay,      ///< N segundos desde ahora
  AfterStep,  ///< N segundos después de otro paso del mismo plan
};

enum class Repeat : uint8_t { None, Daily };

struct Timing {
  TimingType type = TimingType::Now;
  int hour = -1;          ///< At
  int minute = -1;        ///< At
  std::string date;       ///< At (opcional): "YYYY-MM-DD"
  uint32_t seconds = 0;   ///< Delay / AfterStep
  int step = -1;          ///< AfterStep: índice del paso de referencia
  Repeat repeat = Repeat::None;
};

// ======================================================================
// Plan
// ======================================================================

struct PlanStep {
  Action action;
  Timing when;
  Condition condition;
};

enum class PlanType : uint8_t {
  Plan,           ///< Lista de pasos ejecutables
  Clarification,  ///< El LLM necesita más información antes de actuar
  Unsupported,    ///< La solicitud está fuera del catálogo
};

struct Plan {
  PlanType type = PlanType::Plan;
  std::string summary;   ///< Resumen legible de lo que se hará
  std::string question;  ///< Clarification: pregunta para el usuario
  std::vector<PlanStep> steps;
};

// ======================================================================
// Errores de validación
// ======================================================================

struct ValidationError {
  std::string code;     ///< Código estable para máquinas: "unknown_device"
  std::string field;    ///< Ruta JSON: "steps[1].params.device"
  std::string message;  ///< Explicación en español
};

// ======================================================================
// Tareas programadas
// ======================================================================

struct ScheduledTask {
  uint32_t id = 0;
  time_t due = 0;          ///< Instante de ejecución (epoch UTC)
  time_t createdAt = 0;
  PlanStep step;           ///< Acción + condición + repetición
  std::string origin;      ///< Texto original del usuario (recortado)
};

// ======================================================================
// Eventos / alertas
// ======================================================================

enum class EventLevel : uint8_t { Info = 0, Warning = 1, Alert = 2 };

struct Event {
  uint32_t id = 0;
  time_t ts = 0;          ///< 0 si la hora aún no está sincronizada
  uint32_t uptimeS = 0;
  EventLevel level = EventLevel::Info;
  std::string source;     ///< "access", "task", "llm", "system", "user"...
  std::string message;
};

}  // namespace agent

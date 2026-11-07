#include "ActionCatalog.h"

#include <string.h>

namespace agent {
namespace actions {

namespace {

const ParamSpec kSetDevice[] = {
    {"device", ParamType::String, true, nullptr, "id de una luz o aspersor del catálogo"},
    {"state", ParamType::Enum, true, "on|off", "on = encender, off = apagar"},
};

const ParamSpec kSetGroup[] = {
    {"group", ParamType::Enum, true, "lights|sprinklers|all", "grupo de salidas"},
    {"state", ParamType::Enum, true, "on|off", "on = encender, off = apagar"},
};

const ParamSpec kReadAccess[] = {
    {"target", ParamType::String, true, nullptr, "doors | windows | accesses | id de una puerta o ventana"},
    {"filter", ParamType::Enum, false, "all|open|closed", "qué estados reportar (por defecto all)"},
};

const ParamSpec kNotify[] = {
    {"message", ParamType::String, true, nullptr, "texto de la notificación (máx. 160 caracteres)"},
    {"level", ParamType::Enum, false, "info|warning|alert", "severidad (por defecto warning)"},
};

const ParamSpec kCancelTask[] = {
    {"task_id", ParamType::Integer, false, nullptr, "id de la tarea a cancelar"},
    {"all", ParamType::Boolean, false, nullptr, "true para cancelar todas las tareas"},
};

#define ARRAY_LEN(a) static_cast<uint8_t>(sizeof(a) / sizeof((a)[0]))

const ActionSpec kActions[] = {
    {"set_device", ActionId::SetDevice, "Enciende o apaga UNA luz o aspersor.",
     kSetDevice, ARRAY_LEN(kSetDevice), true},
    {"set_group", ActionId::SetGroup, "Enciende o apaga un grupo completo de salidas.",
     kSetGroup, ARRAY_LEN(kSetGroup), true},
    {"read_access", ActionId::ReadAccess, "Consulta el estado abierto/cerrado de puertas y ventanas.",
     kReadAccess, ARRAY_LEN(kReadAccess), false},
    {"notify", ActionId::Notify, "Genera una notificación visible (interfaz, LED de alerta y Telegram si está configurado).",
     kNotify, ARRAY_LEN(kNotify), true},
    {"list_tasks", ActionId::ListTasks, "Lista las tareas programadas pendientes.",
     nullptr, 0, false},
    {"cancel_task", ActionId::CancelTask, "Cancela una tarea programada por su id, o todas con all=true.",
     kCancelTask, ARRAY_LEN(kCancelTask), false},
    {"system_status", ActionId::SystemStatus, "Devuelve el estado general (salidas, accesos, tareas, red, hora).",
     nullptr, 0, false},
};

const size_t kCount = sizeof(kActions) / sizeof(kActions[0]);

}  // namespace

size_t count() { return kCount; }

const ActionSpec& at(size_t i) { return kActions[i < kCount ? i : 0]; }

const ActionSpec* find(const std::string& name) {
  for (size_t i = 0; i < kCount; ++i) {
    if (name == kActions[i].name) return &kActions[i];
  }
  return nullptr;
}

const ActionSpec& spec(ActionId id) {
  for (size_t i = 0; i < kCount; ++i) {
    if (kActions[i].id == id) return kActions[i];
  }
  return kActions[0];
}

const ParamSpec* findParam(const ActionSpec& a, const std::string& name) {
  for (uint8_t i = 0; i < a.paramCount; ++i) {
    if (name == a.params[i].name) return &a.params[i];
  }
  return nullptr;
}

bool enumContains(const char* allowed, const std::string& value) {
  if (!allowed || value.empty()) return false;
  const char* p = allowed;
  while (*p) {
    const char* end = strchr(p, '|');
    size_t len = end ? static_cast<size_t>(end - p) : strlen(p);
    if (len == value.size() && strncmp(p, value.c_str(), len) == 0) return true;
    if (!end) break;
    p = end + 1;
  }
  return false;
}

const char* paramTypeName(ParamType t) {
  switch (t) {
    case ParamType::String: return "string";
    case ParamType::Integer: return "integer";
    case ParamType::Boolean: return "boolean";
    case ParamType::Enum: return "enum";
  }
  return "unknown";
}

}  // namespace actions
}  // namespace agent

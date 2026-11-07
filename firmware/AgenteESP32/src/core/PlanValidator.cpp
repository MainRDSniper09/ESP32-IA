#include "PlanValidator.h"

#include <string.h>

#include "../config/AppConfig.h"
#include "ActionCatalog.h"
#include "DeviceCatalog.h"
#include "TextUtils.h"
#include "TimeUtils.h"

namespace agent {

namespace {

const size_t kMaxErrors = 12;
const size_t kMaxSummary = 200;
const size_t kMaxQuestion = 240;
const size_t kMaxMessage = 160;

void addError(std::vector<ValidationError>& errors, const char* code, const std::string& field,
              const std::string& message) {
  if (errors.size() >= kMaxErrors) return;
  ValidationError e;
  e.code = code;
  e.field = field;
  e.message = message;
  errors.push_back(e);
}

std::string join(const std::string& path, const char* key) {
  return path.empty() ? std::string(key) : path + "." + key;
}

/** Rechaza claves que no estén en `allowed` (lista terminada en nullptr). */
void checkKeys(JsonObjectConst obj, const char* const* allowed, const std::string& path,
               std::vector<ValidationError>& errors) {
  for (JsonPairConst kv : obj) {
    bool ok = false;
    for (const char* const* a = allowed; *a; ++a) {
      if (strcmp(kv.key().c_str(), *a) == 0) {
        ok = true;
        break;
      }
    }
    if (!ok) {
      addError(errors, "unknown_field", join(path, kv.key().c_str()),
               std::string("Campo no permitido: '") + kv.key().c_str() + "'");
    }
  }
}

bool isNonNegativeInt(JsonVariantConst v) { return v.is<long>() && v.as<long>() >= 0; }

std::string str(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  return s ? std::string(s) : std::string();
}

}  // namespace

// ---------------------------------------------------------------------------
bool PlanValidator::validatePlan(JsonVariantConst root, Plan& out, std::vector<ValidationError>& errors) {
  errors.clear();
  out = Plan();

  if (!root.is<JsonObjectConst>()) {
    addError(errors, "invalid_root", "", "La respuesta debe ser un objeto JSON");
    return false;
  }
  JsonObjectConst obj = root.as<JsonObjectConst>();
  static const char* const kTopKeys[] = {"version", "type", "summary", "question", "steps", nullptr};
  checkKeys(obj, kTopKeys, "", errors);

  // version (opcional, debe ser 1)
  JsonVariantConst version = obj["version"];
  if (!version.isNull() && !(version.is<long>() && version.as<long>() == 1)) {
    addError(errors, "unsupported_version", "version", "Solo se admite version = 1");
  }

  // summary (opcional, se recorta)
  JsonVariantConst summary = obj["summary"];
  if (!summary.isNull()) {
    if (!summary.is<const char*>()) addError(errors, "invalid_type", "summary", "summary debe ser texto");
    else out.summary = text::truncateUtf8(str(summary), kMaxSummary);
  }

  // type (obligatorio)
  JsonVariantConst type = obj["type"];
  if (!type.is<const char*>()) {
    addError(errors, "missing_field", "type", "Falta 'type' (plan | clarification | unsupported)");
    return false;
  }
  std::string t = str(type);
  JsonVariantConst steps = obj["steps"];

  if (t == "clarification") {
    out.type = PlanType::Clarification;
    JsonVariantConst q = obj["question"];
    if (!q.is<const char*>() || str(q).empty()) {
      addError(errors, "missing_field", "question", "Una aclaración requiere 'question' no vacía");
    } else {
      out.question = text::truncateUtf8(str(q), kMaxQuestion);
    }
    if (!steps.isNull() && !(steps.is<JsonArrayConst>() && steps.size() == 0)) {
      addError(errors, "unexpected_steps", "steps", "Una aclaración no puede incluir pasos");
    }
    return errors.empty();
  }

  if (t == "unsupported") {
    out.type = PlanType::Unsupported;
    if (!steps.isNull() && !(steps.is<JsonArrayConst>() && steps.size() == 0)) {
      addError(errors, "unexpected_steps", "steps", "Una respuesta 'unsupported' no puede incluir pasos");
    }
    return errors.empty();
  }

  if (t != "plan") {
    addError(errors, "invalid_value", "type", "type debe ser plan | clarification | unsupported");
    return false;
  }

  out.type = PlanType::Plan;
  if (!obj["question"].isNull()) {
    addError(errors, "unexpected_field", "question", "'question' solo se usa con type = clarification");
  }
  if (!steps.is<JsonArrayConst>()) {
    addError(errors, "missing_field", "steps", "Un plan requiere el arreglo 'steps'");
    return false;
  }
  JsonArrayConst arr = steps.as<JsonArrayConst>();
  if (arr.size() == 0) {
    addError(errors, "empty_plan", "steps", "El plan no contiene pasos");
    return false;
  }
  if (arr.size() > AGENT_MAX_PLAN_STEPS) {
    addError(errors, "too_many_steps", "steps",
             "Máximo " + std::to_string(AGENT_MAX_PLAN_STEPS) + " pasos por plan");
    return false;
  }

  size_t i = 0;
  for (JsonVariantConst s : arr) {
    PlanStep step;
    validateStep(s, i, step, errors, true);
    out.steps.push_back(step);
    ++i;
  }
  return errors.empty();
}

// ---------------------------------------------------------------------------
bool PlanValidator::validateStep(JsonVariantConst v, size_t index, PlanStep& out,
                                 std::vector<ValidationError>& errors, bool allowTiming) {
  const size_t before = errors.size();
  const std::string path = "steps[" + std::to_string(index) + "]";

  if (!v.is<JsonObjectConst>()) {
    addError(errors, "invalid_type", path, "Cada paso debe ser un objeto");
    return false;
  }
  JsonObjectConst obj = v.as<JsonObjectConst>();
  static const char* const kStepKeys[] = {"action", "params", "when", "condition", nullptr};
  checkKeys(obj, kStepKeys, path, errors);

  // ---- action
  JsonVariantConst actionV = obj["action"];
  if (!actionV.is<const char*>()) {
    addError(errors, "missing_field", join(path, "action"), "Falta 'action'");
    return false;
  }
  const ActionSpec* spec = actions::find(str(actionV));
  if (!spec) {
    addError(errors, "unknown_action", join(path, "action"),
             "Acción no autorizada: '" + str(actionV) + "'");
    return false;
  }
  out.action.id = spec->id;

  // ---- params
  validateParams(obj["params"], join(path, "params"), out.action, errors);

  // ---- when
  JsonVariantConst when = obj["when"];
  if (!when.isNull()) {
    if (!allowTiming) {
      addError(errors, "unexpected_field", join(path, "when"), "'when' no se admite en este contexto");
    } else {
      validateWhen(when, join(path, "when"), index, out.when, errors);
    }
  }
  if (out.when.type != TimingType::Now && !spec->schedulable) {
    addError(errors, "not_schedulable", join(path, "when"),
             std::string("La acción '") + spec->name + "' es una consulta inmediata y no puede programarse");
  }

  // ---- condition
  JsonVariantConst cond = obj["condition"];
  if (!cond.isNull()) {
    validateCondition(cond, join(path, "condition"), out.condition, errors);
    if (!spec->schedulable && out.condition.type != ConditionType::None) {
      addError(errors, "unexpected_field", join(path, "condition"),
               std::string("La acción '") + spec->name + "' no admite condición");
    }
  }

  return errors.size() == before;
}

// ---------------------------------------------------------------------------
void PlanValidator::validateParams(JsonVariantConst params, const std::string& path, Action& action,
                                   std::vector<ValidationError>& errors) {
  const ActionSpec& spec = actions::spec(action.id);
  ActionParams& p = action.params;

  if (!params.isNull() && !params.is<JsonObjectConst>()) {
    addError(errors, "invalid_type", path, "'params' debe ser un objeto");
    return;
  }
  JsonObjectConst obj = params.as<JsonObjectConst>();  // nulo si no hay params

  // 1) Claves desconocidas y tipos genéricos.
  for (JsonPairConst kv : obj) {
    const ParamSpec* ps = actions::findParam(spec, kv.key().c_str());
    const std::string field = join(path, kv.key().c_str());
    if (!ps) {
      addError(errors, "unknown_param", field,
               std::string("Parámetro no permitido para '") + spec.name + "': '" + kv.key().c_str() + "'");
      continue;
    }
    JsonVariantConst val = kv.value();
    switch (ps->type) {
      case ParamType::String:
        if (!val.is<const char*>() || str(val).empty())
          addError(errors, "invalid_type", field, "Debe ser texto no vacío");
        break;
      case ParamType::Enum:
        if (!val.is<const char*>()) addError(errors, "invalid_type", field, "Debe ser texto");
        else if (!actions::enumContains(ps->allowed, str(val)))
          addError(errors, "invalid_value", field,
                   "Valor '" + str(val) + "' no permitido. Opciones: " + ps->allowed);
        break;
      case ParamType::Integer:
        if (!isNonNegativeInt(val)) addError(errors, "invalid_type", field, "Debe ser un entero >= 0");
        break;
      case ParamType::Boolean:
        if (!val.is<bool>()) addError(errors, "invalid_type", field, "Debe ser true o false");
        break;
    }
  }

  // 2) Obligatorios.
  for (uint8_t i = 0; i < spec.paramCount; ++i) {
    const ParamSpec& ps = spec.params[i];
    if (ps.required && obj[ps.name].isNull()) {
      addError(errors, "missing_param", join(path, ps.name), std::string("Falta el parámetro '") + ps.name + "'");
    }
  }

  // 3) Semántica específica de cada acción.
  switch (action.id) {
    case ActionId::SetDevice: {
      p.device = str(obj["device"]);
      p.state = str(obj["state"]) == "on";
      if (!p.device.empty()) {
        const DeviceInfo* d = devices::find(p.device);
        if (!d) addError(errors, "unknown_device", join(path, "device"), "Dispositivo desconocido: '" + p.device + "'");
        else if (!isOutputKind(d->kind))
          addError(errors, "device_not_output", join(path, "device"),
                   "'" + p.device + "' es una entrada (puerta/ventana) y no puede encenderse/apagarse");
      }
      break;
    }
    case ActionId::SetGroup:
      p.group = str(obj["group"]);
      p.state = str(obj["state"]) == "on";
      break;
    case ActionId::ReadAccess: {
      p.group = str(obj["target"]);
      p.filter = obj["filter"].isNull() ? std::string("all") : str(obj["filter"]);
      if (!p.group.empty() && !devices::isValidAccessTarget(p.group))
        addError(errors, "unknown_target", join(path, "target"),
                 "Objetivo inválido: '" + p.group + "' (use doors, windows, accesses o el id de una puerta/ventana)");
      break;
    }
    case ActionId::Notify: {
      p.message = str(obj["message"]);
      std::string level = obj["level"].isNull() ? std::string("warning") : str(obj["level"]);
      p.level = level == "info" ? 0 : (level == "alert" ? 2 : 1);
      if (p.message.size() > kMaxMessage)
        addError(errors, "too_long", join(path, "message"),
                 "El mensaje supera " + std::to_string(kMaxMessage) + " caracteres");
      break;
    }
    case ActionId::CancelTask: {
      bool hasId = !obj["task_id"].isNull();
      p.all = obj["all"].is<bool>() && obj["all"].as<bool>();
      if (hasId && isNonNegativeInt(obj["task_id"])) p.taskId = static_cast<uint32_t>(obj["task_id"].as<long>());
      if (hasId == p.all) {
        addError(errors, "invalid_params", path, "Indique 'task_id' o 'all': true (exactamente uno)");
      } else if (hasId && p.taskId == 0) {
        addError(errors, "invalid_value", join(path, "task_id"), "task_id debe ser mayor que 0");
      }
      break;
    }
    case ActionId::ListTasks:
    case ActionId::SystemStatus:
      break;
  }
}

// ---------------------------------------------------------------------------
void PlanValidator::validateWhen(JsonVariantConst when, const std::string& path, size_t index, Timing& out,
                                 std::vector<ValidationError>& errors) {
  if (!when.is<JsonObjectConst>()) {
    addError(errors, "invalid_type", path, "'when' debe ser un objeto");
    return;
  }
  JsonObjectConst obj = when.as<JsonObjectConst>();
  std::string type = str(obj["type"]);
  JsonVariantConst repeat = obj["repeat"];

  if (type == "now") {
    static const char* const k[] = {"type", nullptr};
    checkKeys(obj, k, path, errors);
    out.type = TimingType::Now;
  } else if (type == "at") {
    static const char* const k[] = {"type", "time", "date", "repeat", nullptr};
    checkKeys(obj, k, path, errors);
    out.type = TimingType::At;
    std::string timeStr = str(obj["time"]);
    if (!obj["time"].is<const char*>()) {
      addError(errors, "missing_field", join(path, "time"), "Falta 'time' con formato HH:MM (24 h)");
    } else if (!timeutil::parseHHMM(timeStr, out.hour, out.minute)) {
      addError(errors, "invalid_time", join(path, "time"), "Hora inválida: '" + timeStr + "' (formato HH:MM, 00:00-23:59)");
    }
    if (!obj["date"].isNull()) {
      int y, m, d;
      out.date = str(obj["date"]);
      if (!obj["date"].is<const char*>() || !timeutil::parseDate(out.date, y, m, d))
        addError(errors, "invalid_date", join(path, "date"), "Fecha inválida (formato YYYY-MM-DD)");
    }
    if (!repeat.isNull()) {
      std::string r = str(repeat);
      if (r == "daily") out.repeat = Repeat::Daily;
      else if (r != "none") addError(errors, "invalid_value", join(path, "repeat"), "repeat debe ser none | daily");
      if (out.repeat == Repeat::Daily && !out.date.empty())
        addError(errors, "invalid_params", join(path, "repeat"), "Una tarea diaria no puede tener fecha fija");
    }
  } else if (type == "delay") {
    static const char* const k[] = {"type", "seconds", nullptr};
    checkKeys(obj, k, path, errors);
    out.type = TimingType::Delay;
    JsonVariantConst s = obj["seconds"];
    if (!isNonNegativeInt(s) || s.as<long>() < 1 || static_cast<unsigned long>(s.as<long>()) > AGENT_MAX_DELAY_S) {
      addError(errors, "invalid_delay", join(path, "seconds"),
               "seconds debe ser un entero entre 1 y " + std::to_string(AGENT_MAX_DELAY_S));
    } else {
      out.seconds = static_cast<uint32_t>(s.as<long>());
    }
  } else if (type == "after_step") {
    static const char* const k[] = {"type", "step", "seconds", nullptr};
    checkKeys(obj, k, path, errors);
    out.type = TimingType::AfterStep;
    JsonVariantConst st = obj["step"];
    JsonVariantConst s = obj["seconds"];
    if (!isNonNegativeInt(st) || static_cast<size_t>(st.as<long>()) >= index) {
      addError(errors, "invalid_step_ref", join(path, "step"),
               "step debe ser el índice (desde 0) de un paso ANTERIOR a este");
    } else {
      out.step = static_cast<int>(st.as<long>());
    }
    if (!isNonNegativeInt(s) || static_cast<unsigned long>(s.as<long>()) > AGENT_MAX_DELAY_S) {
      addError(errors, "invalid_delay", join(path, "seconds"),
               "seconds debe ser un entero entre 0 y " + std::to_string(AGENT_MAX_DELAY_S));
    } else {
      out.seconds = static_cast<uint32_t>(s.as<long>());
    }
  } else {
    addError(errors, "invalid_value", join(path, "type"), "when.type debe ser now | at | delay | after_step");
  }

  if (out.type != TimingType::At && !repeat.isNull()) {
    addError(errors, "invalid_params", join(path, "repeat"), "repeat solo se admite con when.type = at");
  }
}

// ---------------------------------------------------------------------------
void PlanValidator::validateCondition(JsonVariantConst cond, const std::string& path, Condition& out,
                                      std::vector<ValidationError>& errors) {
  if (!cond.is<JsonObjectConst>()) {
    addError(errors, "invalid_type", path, "'condition' debe ser un objeto");
    return;
  }
  JsonObjectConst obj = cond.as<JsonObjectConst>();
  static const char* const k[] = {"type", "target", nullptr};
  checkKeys(obj, k, path, errors);

  std::string type = str(obj["type"]);
  out.target = str(obj["target"]);

  if (type == "all_closed") out.type = ConditionType::AllClosed;
  else if (type == "any_open") out.type = ConditionType::AnyOpen;
  else if (type == "is_open") out.type = ConditionType::IsOpen;
  else if (type == "is_closed") out.type = ConditionType::IsClosed;
  else {
    addError(errors, "invalid_value", join(path, "type"),
             "condition.type debe ser all_closed | any_open | is_open | is_closed");
    return;
  }

  if (out.target.empty()) {
    addError(errors, "missing_field", join(path, "target"), "Falta 'target' en la condición");
    return;
  }
  if (out.type == ConditionType::IsOpen || out.type == ConditionType::IsClosed) {
    const DeviceInfo* d = devices::find(out.target);
    if (!d || !isAccessKind(d->kind))
      addError(errors, "unknown_target", join(path, "target"),
               "is_open/is_closed requieren el id de UNA puerta o ventana");
  } else if (!devices::isValidAccessTarget(out.target)) {
    addError(errors, "unknown_target", join(path, "target"),
             "Objetivo inválido: '" + out.target + "' (doors, windows, accesses o id de puerta/ventana)");
  }
}

}  // namespace agent

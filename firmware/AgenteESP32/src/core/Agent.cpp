#include "Agent.h"

#include <string.h>

#include "../config/AppConfig.h"
#include "ActionCatalog.h"
#include "DeviceCatalog.h"
#include "PlanSerializer.h"
#include "PlanValidator.h"
#include "TextUtils.h"
#include "TimeUtils.h"

namespace agent {

namespace {

const size_t kMaxOrigin = 80;

/** "encendida"/"apagada" (luces) o "encendido"/"apagado" (aspersores). */
std::string stateWord(const DeviceInfo& d, bool on) {
  bool fem = d.kind != DeviceKind::Sprinkler;
  if (on) return fem ? "encendida" : "encendido";
  return fem ? "apagada" : "apagado";
}

std::string joinNames(const std::vector<std::string>& v) {
  std::string s;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) s += ", ";
    s += v[i];
  }
  return s;
}

}  // namespace

Agent::Agent(IHardware& hw, IClock& clock, ITaskStore& store, ILock* lock)
    : hw_(hw), clock_(clock), store_(store), lock_(lock), scheduler_(AGENT_MAX_TASKS),
      events_(AGENT_EVENT_LOG_SIZE) {}

// ===========================================================================
// Arranque y persistencia
// ===========================================================================

void Agent::begin() {
  LockGuard g(lock_);
  std::string blob;
  if (!store_.load(blob) || blob.empty()) {
    log(EventLevel::Info, "system", "Sin tareas guardadas en memoria no volátil");
    return;
  }
  size_t discarded = 0;
  if (!scheduler_.deserialize(blob, discarded)) {
    log(EventLevel::Warning, "system", "Las tareas guardadas estaban dañadas y se descartaron");
    persist();
    return;
  }
  log(EventLevel::Info, "system",
      "Se recuperaron " + std::to_string(scheduler_.size()) + " tarea(s) de la memoria no volátil");
  if (discarded) {
    log(EventLevel::Warning, "system", std::to_string(discarded) + " tarea(s) inválida(s) descartada(s) al recuperar");
    persist();
  }
}

void Agent::persist() {
  if (!store_.save(scheduler_.serialize())) {
    log(EventLevel::Alert, "system", "No se pudieron guardar las tareas en memoria no volátil");
  }
}

void Agent::log(EventLevel level, const std::string& source, const std::string& message) {
  time_t ts = clock_.isSynced() ? clock_.now() : 0;
  events_.add(level, source, message, ts, clock_.uptimeSeconds());
}

void Agent::logEvent(EventLevel level, const std::string& source, const std::string& message) {
  LockGuard g(lock_);
  log(level, source, message);
}

// ===========================================================================
// Entradas: texto, respuesta del LLM, plan JSON, control manual
// ===========================================================================

bool Agent::precheckText(const std::string& textIn, JsonObject out) {
  std::string phrase;
  if (!text::findAmbiguousTime(textIn, phrase)) return false;
  std::string q = "Mencionaste \"" + phrase +
                  "\", que no indica un momento exacto. ¿A qué hora (por ejemplo 20:30) o en cuántos "
                  "minutos quieres que lo haga?";
  out["outcome"] = "clarification";
  out["question"] = q;
  out["message"] = q;
  out["source"] = "local_guard";
  logEvent(EventLevel::Info, "agent", "Instrucción ambigua (\"" + phrase + "\"): se pidió aclaración");
  return true;
}

void Agent::handleLlmText(const std::string& llmText, const std::string& origin, JsonObject out) {
  std::string json;
  if (!text::extractJsonObject(llmText, json)) {
    out["outcome"] = "rejected";
    out["message"] = "La respuesta del LLM no contiene un objeto JSON. No se ejecutó nada.";
    JsonObject e = out["errors"].to<JsonArray>().add<JsonObject>();
    e["code"] = "no_json";
    e["field"] = "";
    e["message"] = "No se encontró un objeto JSON en la respuesta";
    logEvent(EventLevel::Warning, "llm", "Respuesta del LLM sin JSON: rechazada");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    out["outcome"] = "rejected";
    out["message"] = "La respuesta del LLM tiene JSON mal formado. No se ejecutó nada.";
    JsonObject e = out["errors"].to<JsonArray>().add<JsonObject>();
    e["code"] = "invalid_json";
    e["field"] = "";
    e["message"] = std::string("JSON inválido: ") + err.c_str();
    logEvent(EventLevel::Warning, "llm", "Respuesta del LLM con JSON inválido: rechazada");
    return;
  }
  handlePlanJson(doc.as<JsonVariantConst>(), origin, false, out);
}

void Agent::handlePlanJson(JsonVariantConst planJson, const std::string& origin, bool dryRun, JsonObject out) {
  LockGuard g(lock_);
  Plan plan;
  std::vector<ValidationError> errors;
  if (!PlanValidator::validatePlan(planJson, plan, errors)) {
    out["outcome"] = "rejected";
    out["message"] = "El plan no pasó la validación. No se ejecutó nada.";
    writeErrors(errors, out);
    if (!dryRun) {
      log(EventLevel::Warning, "agent",
          "Plan rechazado (" + errors[0].code + "): " + errors[0].message);
    }
    return;
  }
  submitPlan(plan, origin, dryRun, out);
}

void Agent::manualSet(const std::string& deviceId, bool on, JsonObject out) {
  JsonDocument doc;
  doc["version"] = 1;
  doc["type"] = "plan";
  doc["summary"] = "Control manual";
  JsonObject step = doc["steps"].to<JsonArray>().add<JsonObject>();
  step["action"] = "set_device";
  step["params"]["device"] = deviceId;
  step["params"]["state"] = on ? "on" : "off";
  handlePlanJson(doc.as<JsonVariantConst>(), "control manual", false, out);
}

// ===========================================================================
// Planificación
// ===========================================================================

bool Agent::resolveTimes(const Plan& plan, std::vector<time_t>& due, std::vector<ValidationError>& errors) {
  const time_t now = clock_.now();
  const bool synced = clock_.isSynced();
  bool needsClock = false;
  due.assign(plan.steps.size(), 0);  // 0 = inmediato

  for (size_t i = 0; i < plan.steps.size(); ++i) {
    const Timing& t = plan.steps[i].when;
    const std::string field = "steps[" + std::to_string(i) + "].when";
    switch (t.type) {
      case TimingType::Now:
        break;
      case TimingType::At: {
        needsClock = true;
        if (!synced) break;
        time_t at = 0;
        if (!timeutil::resolveLocalTime(now, t.hour, t.minute, t.date, at)) {
          ValidationError e;
          e.code = "time_in_past";
          e.field = field;
          e.message = "La fecha y hora indicadas ya pasaron";
          errors.push_back(e);
        }
        due[i] = at;
        break;
      }
      case TimingType::Delay:
        needsClock = true;
        due[i] = now + static_cast<time_t>(t.seconds);
        break;
      case TimingType::AfterStep: {
        time_t base = due[static_cast<size_t>(t.step)];
        if (base == 0 && t.seconds == 0) break;  // inmediatamente después de un paso inmediato
        needsClock = true;
        due[i] = (base == 0 ? now : base) + static_cast<time_t>(t.seconds);
        break;
      }
    }
  }
  if (needsClock && !synced) {
    ValidationError e;
    e.code = "clock_not_synced";
    e.field = "steps";
    e.message = "La hora aún no está sincronizada por NTP: solo se aceptan órdenes inmediatas";
    errors.push_back(e);
  }
  return errors.empty();
}

void Agent::submitPlan(const Plan& plan, const std::string& originIn, bool dryRun, JsonObject out) {
  const std::string origin = text::truncateUtf8(originIn, kMaxOrigin);
  serializer::writePlan(plan, out["plan"].to<JsonObject>());

  if (plan.type == PlanType::Clarification) {
    out["outcome"] = "clarification";
    out["question"] = plan.question;
    out["message"] = plan.question;
    out["source"] = "llm";
    return;
  }
  if (plan.type == PlanType::Unsupported) {
    out["outcome"] = "unsupported";
    out["message"] = plan.summary.empty()
                         ? std::string("La solicitud no corresponde a ninguna acción disponible.")
                         : plan.summary;
    return;
  }

  std::vector<time_t> due;
  std::vector<ValidationError> errors;
  if (!resolveTimes(plan, due, errors)) {
    out["outcome"] = "rejected";
    out["message"] = "No se puede programar el plan. No se ejecutó nada.";
    writeErrors(errors, out);
    if (!dryRun) log(EventLevel::Warning, "agent", "Plan rechazado: " + errors[0].message);
    return;
  }
  size_t toSchedule = 0;
  for (size_t i = 0; i < due.size(); ++i) toSchedule += due[i] ? 1 : 0;
  if (toSchedule > scheduler_.freeSlots()) {
    std::vector<ValidationError> e(1);
    e[0].code = "scheduler_full";
    e[0].field = "steps";
    e[0].message = "No hay espacio para " + std::to_string(toSchedule) + " tarea(s) nuevas (máximo " +
                   std::to_string(scheduler_.capacity()) + ")";
    out["outcome"] = "rejected";
    out["message"] = e[0].message;
    writeErrors(e, out);
    return;
  }

  // ---- Ejecución / programación (el plan ya es válido en su totalidad)
  JsonArray results = out["results"].to<JsonArray>();
  size_t executed = 0, scheduled = 0;
  std::string message;
  for (size_t i = 0; i < plan.steps.size(); ++i) {
    const PlanStep& step = plan.steps[i];
    JsonObject r = results.add<JsonObject>();
    r["step"] = i;
    r["action"] = actions::spec(step.action.id).name;
    r["description"] = serializer::describeStep(step);

    if (due[i] == 0) {
      if (dryRun) {
        r["status"] = "would_execute";
      } else {
        executeStep(step, r);
        ++executed;
      }
    } else {
      r["due"] = static_cast<uint32_t>(due[i]);
      r["due_local"] = timeutil::toIso8601(due[i]);
      if (step.when.repeat == Repeat::Daily) r["repeat"] = "daily";
      if (dryRun) {
        r["status"] = "would_schedule";
      } else {
        uint32_t id = scheduler_.add(step, due[i], clock_.now(), origin);
        r["status"] = "scheduled";
        r["task_id"] = id;
        std::string m = "Programado para " + timeutil::toShort(due[i]) + " (tarea #" + std::to_string(id) + ")";
        if (step.when.repeat == Repeat::Daily) m += ", todos los días";
        r["message"] = m;
        log(EventLevel::Info, "task", "Nueva tarea #" + std::to_string(id) + ": " + serializer::describeStep(step) +
                                          " — " + timeutil::toShort(due[i]));
        ++scheduled;
      }
    }
    const char* m = r["message"] | "";
    if (*m) {
      if (!message.empty()) message += " | ";
      message += m;
    }
  }

  if (dryRun) {
    out["outcome"] = "valid";
    out["message"] = "Plan válido (simulación: no se ejecutó nada).";
    return;
  }
  if (scheduled) persist();
  out["outcome"] = scheduled == 0 ? "executed" : (executed == 0 ? "scheduled" : "mixed");
  out["message"] = message.empty() ? std::string("Plan ejecutado.") : message;
}

// ===========================================================================
// Ejecución
// ===========================================================================

void Agent::executeStep(const PlanStep& step, JsonObject r) {
  CondResult c = evaluate(step.condition);
  if (!c.met) {
    r["status"] = "skipped";
    r["condition_met"] = false;
    std::string cond = serializer::describeCondition(step.condition);  // " si ..."
    if (!cond.empty() && cond[0] == ' ') cond.erase(0, 1);
    r["message"] = "Condición no cumplida (" + cond + "): " + c.detail +
                   ". No se ejecutó: " + serializer::describeAction(step.action);
    return;
  }
  if (step.condition.type != ConditionType::None) r["condition_met"] = true;
  r["status"] = "executed";

  const ActionParams& p = step.action.params;
  switch (step.action.id) {
    case ActionId::SetDevice: doSetDevice(p, r); break;
    case ActionId::SetGroup: doSetGroup(p, r); break;
    case ActionId::ReadAccess: doReadAccess(p, r); break;
    case ActionId::Notify: doNotify(p, c.detail, r); break;
    case ActionId::ListTasks: doListTasks(r); break;
    case ActionId::CancelTask: doCancelTask(p, r); break;
    case ActionId::SystemStatus: doSystemStatus(r); break;
  }
}

std::string Agent::accessList(const std::string& target, bool open) const {
  std::vector<std::string> names;
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    if (devices::inAccessTarget(d, target) && hw_.isOpen(d) == open) names.push_back(d.name);
  }
  return joinNames(names);
}

Agent::CondResult Agent::evaluate(const Condition& c) const {
  CondResult r;
  r.met = true;
  switch (c.type) {
    case ConditionType::None:
      break;
    case ConditionType::AllClosed: {
      std::string open = accessList(c.target, true);
      r.met = open.empty();
      r.detail = r.met ? "todas cerradas" : "abiertas: " + open;
      break;
    }
    case ConditionType::AnyOpen: {
      std::string open = accessList(c.target, true);
      r.met = !open.empty();
      r.detail = r.met ? "abiertas: " + open : "todas cerradas";
      break;
    }
    case ConditionType::IsOpen:
    case ConditionType::IsClosed: {
      const DeviceInfo* d = devices::find(c.target);
      bool open = d && hw_.isOpen(*d);
      r.met = (c.type == ConditionType::IsOpen) == open;
      r.detail = std::string(d ? d->name : c.target.c_str()) + (open ? " está abierta" : " está cerrada");
      break;
    }
  }
  return r;
}

// ---------------------------------------------------------------------------
// Catálogo de funciones autorizadas
// ---------------------------------------------------------------------------

void Agent::doSetDevice(const ActionParams& p, JsonObject r) {
  const DeviceInfo* d = devices::find(p.device);
  if (!d || !isOutputKind(d->kind)) {  // ya validado; defensa adicional
    r["status"] = "failed";
    r["message"] = "Dispositivo inválido";
    return;
  }
  hw_.writeOutput(*d, p.state);
  std::string m = std::string(d->name) + " " + stateWord(*d, p.state);
  r["message"] = m;
  log(EventLevel::Info, "device", m);
}

void Agent::doSetGroup(const ActionParams& p, JsonObject r) {
  size_t n = 0;
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    if (devices::inOutputGroup(d, p.group)) {
      hw_.writeOutput(d, p.state);
      ++n;
    }
  }
  std::string m = std::string(p.state ? "Se encendieron " : "Se apagaron ") + devices::targetLabel(p.group) +
                  " (" + std::to_string(n) + ")";
  r["message"] = m;
  log(EventLevel::Info, "device", m);
}

void Agent::doReadAccess(const ActionParams& p, JsonObject r) {
  JsonObject data = r["data"].to<JsonObject>();
  JsonArray items = data["accesses"].to<JsonArray>();
  std::vector<std::string> open, closed;
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    if (!devices::inAccessTarget(d, p.group)) continue;
    bool isOpen = hw_.isOpen(d);
    (isOpen ? open : closed).push_back(d.name);
    if (p.filter == "open" && !isOpen) continue;
    if (p.filter == "closed" && isOpen) continue;
    JsonObject o = items.add<JsonObject>();
    o["id"] = d.id;
    o["name"] = d.name;
    o["kind"] = devices::kindName(d.kind);
    o["open"] = isOpen;
  }
  data["open_count"] = open.size();
  data["closed_count"] = closed.size();

  std::string label = devices::targetLabel(p.group);
  std::string m;
  if (p.filter == "closed") {
    m = closed.empty() ? "Ninguna de " + label + " está cerrada" : "Cerradas: " + joinNames(closed);
  } else if (open.empty()) {
    m = "Todas " + label + " están cerradas";
  } else {
    m = "Abiertas: " + joinNames(open);
    if (p.filter == "all" && !closed.empty()) m += ". Cerradas: " + joinNames(closed);
  }
  r["message"] = m;
}

void Agent::doNotify(const ActionParams& p, const std::string& extra, JsonObject r) {
  std::string m = p.message;
  if (!extra.empty() && extra != "todas cerradas") m += " (" + extra + ")";
  log(static_cast<EventLevel>(p.level), "notify", m);
  r["message"] = "Notificación: " + m;
}

void Agent::doListTasks(JsonObject r) {
  JsonObject data = r["data"].to<JsonObject>();
  writeTasksUnlocked(data["tasks"].to<JsonArray>());
  std::vector<ScheduledTask> list = scheduler_.list();
  if (list.empty()) {
    r["message"] = "No hay tareas programadas";
    return;
  }
  std::string m = std::to_string(list.size()) + " tarea(s): ";
  for (size_t i = 0; i < list.size(); ++i) {
    if (i) m += "; ";
    m += "#" + std::to_string(list[i].id) + " " + timeutil::toShort(list[i].due) + " " +
         serializer::describeStep(list[i].step);
  }
  r["message"] = m;
}

void Agent::doCancelTask(const ActionParams& p, JsonObject r) {
  if (p.all) {
    size_t n = scheduler_.cancelAll();
    persist();
    r["message"] = "Se cancelaron " + std::to_string(n) + " tarea(s)";
    log(EventLevel::Info, "task", r["message"].as<std::string>());
    return;
  }
  if (scheduler_.cancel(p.taskId)) {
    persist();
    r["message"] = "Tarea #" + std::to_string(p.taskId) + " cancelada";
    log(EventLevel::Info, "task", r["message"].as<std::string>());
  } else {
    r["status"] = "failed";
    r["message"] = "No existe la tarea #" + std::to_string(p.taskId);
  }
}

void Agent::doSystemStatus(JsonObject r) {
  JsonObject data = r["data"].to<JsonObject>();
  writeSummaryUnlocked(data);
  r["message"] = std::to_string(data["outputs_on"].as<unsigned>()) + " salida(s) encendida(s), " +
                 std::to_string(data["accesses_open"].as<unsigned>()) + " acceso(s) abierto(s), " +
                 std::to_string(data["tasks"].as<unsigned>()) + " tarea(s) programada(s)";
}

// ===========================================================================
// Periódicos
// ===========================================================================

void Agent::tick() {
  LockGuard g(lock_);
  if (!clock_.isSynced()) return;  // sin hora confiable no se ejecutan tareas horarias
  const time_t now = clock_.now();
  std::vector<ScheduledTask> due = scheduler_.takeDue(now);
  if (due.empty()) return;

  for (size_t i = 0; i < due.size(); ++i) {
    const ScheduledTask& t = due[i];
    const std::string tag = "Tarea #" + std::to_string(t.id);
    if (now - t.due > static_cast<time_t>(AGENT_MISSED_TASK_GRACE_S)) {
      log(EventLevel::Alert, "task",
          tag + " NO se ejecutó (vencía " + timeutil::toShort(t.due) + ", el equipo estaba apagado o sin hora): " +
              serializer::describeStep(t.step));
      continue;
    }
    JsonDocument tmp;
    JsonObject r = tmp.to<JsonObject>();
    executeStep(t.step, r);
    const char* status = r["status"] | "executed";
    const char* msg = r["message"] | "";
    if (strcmp(status, "skipped") == 0) log(EventLevel::Info, "task", tag + " omitida. " + msg);
    else log(EventLevel::Info, "task", tag + " ejecutada. " + msg);
  }
  persist();
}

void Agent::onAccessChanged(const DeviceInfo& dev, bool open) {
  LockGuard g(lock_);
  log(EventLevel::Info, "access", std::string(dev.name) + (open ? " abierta" : " cerrada"));
}

// ===========================================================================
// Consultas para la API
// ===========================================================================

void Agent::writeDevices(JsonArray out) {
  LockGuard g(lock_);
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    JsonObject o = out.add<JsonObject>();
    o["id"] = d.id;
    o["name"] = d.name;
    o["kind"] = devices::kindName(d.kind);
    o["pin"] = d.pin;
    if (isOutputKind(d.kind)) {
      o["io"] = "output";
      bool on = hw_.readOutput(d);
      o["on"] = on;
      o["state"] = on ? "on" : "off";
    } else {
      o["io"] = "input";
      bool open = hw_.isOpen(d);
      o["open"] = open;
      o["state"] = open ? "open" : "closed";
    }
  }
}

void Agent::writeAccesses(JsonArray out, const std::string& target, const std::string& filter) {
  LockGuard g(lock_);
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    if (!devices::inAccessTarget(d, target)) continue;
    bool open = hw_.isOpen(d);
    if ((filter == "open" && !open) || (filter == "closed" && open)) continue;
    JsonObject o = out.add<JsonObject>();
    o["id"] = d.id;
    o["name"] = d.name;
    o["kind"] = devices::kindName(d.kind);
    o["open"] = open;
  }
}

void Agent::writeTasks(JsonArray out) {
  LockGuard g(lock_);
  writeTasksUnlocked(out);
}

void Agent::writeTasksUnlocked(JsonArray out) {
  std::vector<ScheduledTask> list = scheduler_.list();
  for (size_t i = 0; i < list.size(); ++i) {
    const ScheduledTask& t = list[i];
    JsonObject o = out.add<JsonObject>();
    o["id"] = t.id;
    o["description"] = serializer::describeStep(t.step);
    o["due"] = static_cast<uint32_t>(t.due);
    o["due_local"] = timeutil::toIso8601(t.due);
    o["repeat"] = t.step.when.repeat == Repeat::Daily ? "daily" : "none";
    o["created"] = static_cast<uint32_t>(t.createdAt);
    o["origin"] = t.origin;
    serializer::writeStepCore(t.step, o["step"].to<JsonObject>());
  }
}

void Agent::writeEvents(JsonArray out, uint32_t sinceId, size_t limit) {
  LockGuard g(lock_);
  std::vector<Event> list = events_.since(sinceId, limit);
  for (size_t i = 0; i < list.size(); ++i) {
    const Event& e = list[i];
    JsonObject o = out.add<JsonObject>();
    o["id"] = e.id;
    if (e.ts) o["time"] = timeutil::toIso8601(e.ts);
    else o["time"] = nullptr;
    o["uptime_s"] = e.uptimeS;
    o["level"] = serializer::levelName(e.level);
    o["source"] = e.source;
    o["message"] = e.message;
  }
}

void Agent::writeSummary(JsonObject out) {
  LockGuard g(lock_);
  writeSummaryUnlocked(out);
}

void Agent::writeSummaryUnlocked(JsonObject out) {
  unsigned on = 0, open = 0;
  JsonArray openList = out["open_accesses"].to<JsonArray>();
  JsonArray onList = out["outputs_on_list"].to<JsonArray>();
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    if (isOutputKind(d.kind) && hw_.readOutput(d)) {
      ++on;
      onList.add(d.id);
    }
    if (isAccessKind(d.kind) && hw_.isOpen(d)) {
      ++open;
      openList.add(d.id);
    }
  }
  out["outputs_on"] = on;
  out["accesses_open"] = open;
  out["tasks"] = static_cast<unsigned>(scheduler_.size());
  out["tasks_capacity"] = static_cast<unsigned>(scheduler_.capacity());
  out["alerts_unacknowledged"] = events_.unacknowledged();
  out["last_event_id"] = events_.lastId();
}

bool Agent::cancelTask(uint32_t id) {
  LockGuard g(lock_);
  if (!scheduler_.cancel(id)) return false;
  persist();
  log(EventLevel::Info, "task", "Tarea #" + std::to_string(id) + " cancelada");
  return true;
}

size_t Agent::cancelAllTasks() {
  LockGuard g(lock_);
  size_t n = scheduler_.cancelAll();
  persist();
  log(EventLevel::Info, "task", "Se cancelaron " + std::to_string(n) + " tarea(s)");
  return n;
}

void Agent::acknowledgeAlerts() {
  LockGuard g(lock_);
  events_.acknowledgeAll();
}

uint32_t Agent::unacknowledgedAlerts() {
  LockGuard g(lock_);
  return events_.unacknowledged();
}

size_t Agent::taskCount() {
  LockGuard g(lock_);
  return scheduler_.size();
}

void Agent::writeErrors(const std::vector<ValidationError>& errors, JsonObject out) {
  JsonArray arr = out["errors"].to<JsonArray>();
  for (size_t i = 0; i < errors.size(); ++i) {
    JsonObject e = arr.add<JsonObject>();
    e["code"] = errors[i].code;
    e["field"] = errors[i].field;
    e["message"] = errors[i].message;
  }
}

}  // namespace agent

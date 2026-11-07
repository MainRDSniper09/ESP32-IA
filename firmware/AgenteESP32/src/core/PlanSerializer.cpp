#include "PlanSerializer.h"

#include <stdio.h>

#include "ActionCatalog.h"
#include "DeviceCatalog.h"

namespace agent {
namespace serializer {

const char* conditionTypeName(ConditionType t) {
  switch (t) {
    case ConditionType::AllClosed: return "all_closed";
    case ConditionType::AnyOpen: return "any_open";
    case ConditionType::IsOpen: return "is_open";
    case ConditionType::IsClosed: return "is_closed";
    case ConditionType::None: break;
  }
  return "none";
}

const char* levelName(EventLevel l) {
  switch (l) {
    case EventLevel::Info: return "info";
    case EventLevel::Warning: return "warning";
    case EventLevel::Alert: return "alert";
  }
  return "info";
}

void writeStepCore(const PlanStep& step, JsonObject out) {
  const Action& a = step.action;
  out["action"] = actions::spec(a.id).name;
  JsonObject p = out["params"].to<JsonObject>();
  switch (a.id) {
    case ActionId::SetDevice:
      p["device"] = a.params.device;
      p["state"] = a.params.state ? "on" : "off";
      break;
    case ActionId::SetGroup:
      p["group"] = a.params.group;
      p["state"] = a.params.state ? "on" : "off";
      break;
    case ActionId::ReadAccess:
      p["target"] = a.params.group;
      p["filter"] = a.params.filter;
      break;
    case ActionId::Notify:
      p["message"] = a.params.message;
      p["level"] = levelName(static_cast<EventLevel>(a.params.level));
      break;
    case ActionId::CancelTask:
      if (a.params.all) p["all"] = true;
      else p["task_id"] = a.params.taskId;
      break;
    case ActionId::ListTasks:
    case ActionId::SystemStatus:
      break;
  }
  if (step.condition.type != ConditionType::None) {
    JsonObject c = out["condition"].to<JsonObject>();
    c["type"] = conditionTypeName(step.condition.type);
    c["target"] = step.condition.target;
  }
}

void writeStep(const PlanStep& step, JsonObject out) {
  writeStepCore(step, out);
  JsonObject w = out["when"].to<JsonObject>();
  const Timing& t = step.when;
  switch (t.type) {
    case TimingType::Now:
      w["type"] = "now";
      break;
    case TimingType::At: {
      w["type"] = "at";
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", t.hour, t.minute);
      w["time"] = buf;
      if (!t.date.empty()) w["date"] = t.date;
      if (t.repeat == Repeat::Daily) w["repeat"] = "daily";
      break;
    }
    case TimingType::Delay:
      w["type"] = "delay";
      w["seconds"] = t.seconds;
      break;
    case TimingType::AfterStep:
      w["type"] = "after_step";
      w["step"] = t.step;
      w["seconds"] = t.seconds;
      break;
  }
}

void writePlan(const Plan& plan, JsonObject out) {
  out["version"] = 1;
  switch (plan.type) {
    case PlanType::Plan: out["type"] = "plan"; break;
    case PlanType::Clarification: out["type"] = "clarification"; break;
    case PlanType::Unsupported: out["type"] = "unsupported"; break;
  }
  if (!plan.summary.empty()) out["summary"] = plan.summary;
  if (plan.type == PlanType::Clarification) out["question"] = plan.question;
  if (plan.type == PlanType::Plan) {
    JsonArray steps = out["steps"].to<JsonArray>();
    for (size_t i = 0; i < plan.steps.size(); ++i) writeStep(plan.steps[i], steps.add<JsonObject>());
  }
}

std::string describeAction(const Action& a) {
  const ActionParams& p = a.params;
  switch (a.id) {
    case ActionId::SetDevice:
      return std::string(p.state ? "encender " : "apagar ") + devices::targetLabel(p.device);
    case ActionId::SetGroup:
      return std::string(p.state ? "encender " : "apagar ") + devices::targetLabel(p.group);
    case ActionId::ReadAccess:
      return "consultar " + devices::targetLabel(p.group);
    case ActionId::Notify:
      return "notificar: \"" + p.message + "\"";
    case ActionId::ListTasks:
      return "listar tareas programadas";
    case ActionId::CancelTask:
      return p.all ? std::string("cancelar todas las tareas") : "cancelar la tarea #" + std::to_string(p.taskId);
    case ActionId::SystemStatus:
      return "consultar estado del sistema";
  }
  return "acción";
}

std::string describeCondition(const Condition& c) {
  std::string label = devices::targetLabel(c.target);
  switch (c.type) {
    case ConditionType::AllClosed: return " si " + label + " están cerradas";
    case ConditionType::AnyOpen: return " si alguna de " + label + " está abierta";
    case ConditionType::IsOpen: return " si " + label + " está abierta";
    case ConditionType::IsClosed: return " si " + label + " está cerrada";
    case ConditionType::None: break;
  }
  return "";
}

std::string describeStep(const PlanStep& s) { return describeAction(s.action) + describeCondition(s.condition); }

}  // namespace serializer
}  // namespace agent

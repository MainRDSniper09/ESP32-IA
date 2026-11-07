#include "TaskScheduler.h"

#include <ArduinoJson.h>

#include <algorithm>

#include "PlanSerializer.h"
#include "PlanValidator.h"
#include "TimeUtils.h"

namespace agent {

namespace {
bool byDue(const ScheduledTask& a, const ScheduledTask& b) {
  return a.due != b.due ? a.due < b.due : a.id < b.id;
}
}  // namespace

uint32_t TaskScheduler::add(const PlanStep& step, time_t due, time_t createdAt, const std::string& origin) {
  if (tasks_.size() >= capacity_) return 0;
  ScheduledTask t;
  t.id = nextId_++;
  t.due = due;
  t.createdAt = createdAt;
  t.step = step;
  t.origin = origin;
  tasks_.push_back(t);
  return t.id;
}

bool TaskScheduler::cancel(uint32_t id) {
  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i].id == id) {
      tasks_.erase(tasks_.begin() + static_cast<long>(i));
      return true;
    }
  }
  return false;
}

size_t TaskScheduler::cancelAll() {
  size_t n = tasks_.size();
  tasks_.clear();
  return n;
}

const ScheduledTask* TaskScheduler::find(uint32_t id) const {
  for (size_t i = 0; i < tasks_.size(); ++i) {
    if (tasks_[i].id == id) return &tasks_[i];
  }
  return nullptr;
}

std::vector<ScheduledTask> TaskScheduler::list() const {
  std::vector<ScheduledTask> out = tasks_;
  std::sort(out.begin(), out.end(), byDue);
  return out;
}

std::vector<ScheduledTask> TaskScheduler::takeDue(time_t now) {
  std::vector<ScheduledTask> due;
  for (size_t i = 0; i < tasks_.size();) {
    ScheduledTask& t = tasks_[i];
    if (t.due > now) {
      ++i;
      continue;
    }
    due.push_back(t);
    if (t.step.when.repeat == Repeat::Daily) {
      // Avanzar al próximo día futuro (cubre también varios días apagado).
      time_t next = t.due;
      int guard = 0;
      while (next <= now && guard++ < 400) next = timeutil::addLocalDays(next, 1);
      t.due = next;
      ++i;
    } else {
      tasks_.erase(tasks_.begin() + static_cast<long>(i));
    }
  }
  std::sort(due.begin(), due.end(), byDue);
  return due;
}

std::string TaskScheduler::serialize() const {
  JsonDocument doc;
  doc["next_id"] = nextId_;
  JsonArray arr = doc["tasks"].to<JsonArray>();
  for (size_t i = 0; i < tasks_.size(); ++i) {
    const ScheduledTask& t = tasks_[i];
    JsonObject o = arr.add<JsonObject>();
    o["id"] = t.id;
    o["due"] = static_cast<uint32_t>(t.due);  // epoch de 32 bits: válido hasta 2106
    o["created"] = static_cast<uint32_t>(t.createdAt);
    if (t.step.when.repeat == Repeat::Daily) o["repeat"] = "daily";
    o["origin"] = t.origin;
    serializer::writeStepCore(t.step, o["step"].to<JsonObject>());
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool TaskScheduler::deserialize(const std::string& json, size_t& discarded) {
  discarded = 0;
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  if (!doc["tasks"].is<JsonArray>()) return false;

  tasks_.clear();
  uint32_t maxId = 0;
  for (JsonObject o : doc["tasks"].as<JsonArray>()) {
    ScheduledTask t;
    std::vector<ValidationError> errors;
    bool ok = o["id"].is<uint32_t>() && o["due"].is<uint32_t>() &&
              PlanValidator::validateStep(o["step"], 0, t.step, errors, false);
    if (!ok || tasks_.size() >= capacity_) {
      ++discarded;
      continue;
    }
    t.id = o["id"].as<uint32_t>();
    t.due = static_cast<time_t>(o["due"].as<uint32_t>());
    t.createdAt = static_cast<time_t>(o["created"] | 0u);
    const char* origin = o["origin"] | "";
    t.origin = origin;
    if (o["repeat"] == "daily") t.step.when.repeat = Repeat::Daily;
    if (t.id > maxId) maxId = t.id;
    tasks_.push_back(t);
  }
  uint32_t storedNext = doc["next_id"] | 1u;
  nextId_ = std::max(storedNext, maxId + 1);
  return true;
}

}  // namespace agent

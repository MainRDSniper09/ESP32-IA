#include "EventLog.h"

namespace agent {

EventLog::EventLog(size_t capacity) : capacity_(capacity > 0 ? capacity : 1) { buffer_.resize(capacity_); }

uint32_t EventLog::add(EventLevel level, const std::string& source, const std::string& message, time_t ts,
                       uint32_t uptimeS) {
  Event& e = buffer_[head_];
  e.id = nextId_++;
  e.ts = ts;
  e.uptimeS = uptimeS;
  e.level = level;
  e.source = source;
  e.message = message;
  head_ = (head_ + 1) % capacity_;
  if (size_ < capacity_) ++size_;
  if (level != EventLevel::Info) ++unacked_;
  if (listener_) listener_(e);
  return e.id;
}

std::vector<Event> EventLog::since(uint32_t sinceId, size_t limit) const {
  std::vector<Event> out;
  // Recorrer del más antiguo al más reciente.
  size_t start = (head_ + capacity_ - size_) % capacity_;
  for (size_t i = 0; i < size_; ++i) {
    const Event& e = buffer_[(start + i) % capacity_];
    if (e.id > sinceId) out.push_back(e);
  }
  if (limit > 0 && out.size() > limit) out.erase(out.begin(), out.end() - static_cast<long>(limit));
  return out;
}

}  // namespace agent

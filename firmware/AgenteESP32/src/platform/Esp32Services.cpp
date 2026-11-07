#include "Esp32Services.h"

#include <esp_timer.h>

#include "../core/TimeUtils.h"

namespace agent {

// ------------------------------------------------------------------ Reloj
void Esp32Clock::begin(const char* tz, const char* ntp1, const char* ntp2, const char* ntp3) {
  // configTzTime fija TZ y arranca SNTP en segundo plano (no bloquea).
  configTzTime(tz, ntp1, ntp2, ntp3);
}

time_t Esp32Clock::now() const { return time(nullptr); }

bool Esp32Clock::isSynced() const { return time(nullptr) >= timeutil::kMinValidEpoch; }

uint32_t Esp32Clock::uptimeSeconds() const { return static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL); }

// -------------------------------------------------------------------- NVS
static const char* kNamespace = "agente";
static const char* kKey = "tasks";

bool NvsTaskStore::begin() {
  ok_ = prefs_.begin(kNamespace, false);
  return ok_;
}

bool NvsTaskStore::load(std::string& out) {
  if (!ok_) return false;
  size_t len = prefs_.getBytesLength(kKey);
  if (len == 0) {
    out.clear();
    return true;
  }
  out.assign(len, '\0');
  size_t read = prefs_.getBytes(kKey, &out[0], len);
  if (read != len) return false;
  last_ = out;
  return true;
}

bool NvsTaskStore::save(const std::string& data) {
  if (!ok_) return false;
  if (data == last_) return true;
  size_t written = prefs_.putBytes(kKey, data.data(), data.size());
  if (written != data.size()) return false;
  last_ = data;
  return true;
}

void NvsTaskStore::clear() {
  if (!ok_) return;
  prefs_.remove(kKey);
  last_.clear();
}

}  // namespace agent

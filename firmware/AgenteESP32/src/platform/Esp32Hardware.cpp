#include "Esp32Hardware.h"

#include "../config/AppConfig.h"

namespace agent {

size_t Esp32Hardware::indexOf(const DeviceInfo& dev) {
  // Los DeviceInfo viven en una tabla estática: la posición es su índice.
  for (size_t i = 0; i < devices::count() && i < kMax; ++i) {
    if (&devices::at(i) == &dev) return i;
  }
  return kMax;
}

void Esp32Hardware::begin() {
  for (size_t i = 0; i < devices::count() && i < kMax; ++i) {
    const DeviceInfo& d = devices::at(i);
    if (isOutputKind(d.kind)) {
      pinMode(d.pin, OUTPUT);
      digitalWrite(d.pin, LOW);
      outputs_[i] = false;
    } else {
      pinMode(d.pin, INPUT_PULLUP);
      // HIGH (pull-up, contacto abierto) = puerta/ventana ABIERTA.
      bool open = digitalRead(d.pin) == HIGH;
      lastRaw_[i] = open;
      stable_[i] = open;
      lastChangeMs_[i] = millis();
    }
  }
}

void Esp32Hardware::poll(uint32_t nowMs, ChangeCallback cb, void* ctx) {
  for (size_t i = 0; i < devices::count() && i < kMax; ++i) {
    const DeviceInfo& d = devices::at(i);
    if (!isAccessKind(d.kind)) continue;
    bool raw = digitalRead(d.pin) == HIGH;
    if (raw != lastRaw_[i]) {
      lastRaw_[i] = raw;
      lastChangeMs_[i] = nowMs;  // reinicia la ventana de antirrebote
      continue;
    }
    if (raw != stable_[i] && nowMs - lastChangeMs_[i] >= AGENT_DEBOUNCE_MS) {
      stable_[i] = raw;
      if (cb) cb(d, raw, ctx);
    }
  }
}

void Esp32Hardware::writeOutput(const DeviceInfo& dev, bool on) {
  size_t i = indexOf(dev);
  if (i >= kMax || !isOutputKind(dev.kind)) return;
  digitalWrite(dev.pin, on ? HIGH : LOW);
  outputs_[i] = on;
}

bool Esp32Hardware::readOutput(const DeviceInfo& dev) const {
  size_t i = indexOf(dev);
  return i < kMax ? outputs_[i] : false;
}

bool Esp32Hardware::isOpen(const DeviceInfo& dev) const {
  size_t i = indexOf(dev);
  return i < kMax ? stable_[i] : true;  // desconocido => abierto (falla segura)
}

}  // namespace agent

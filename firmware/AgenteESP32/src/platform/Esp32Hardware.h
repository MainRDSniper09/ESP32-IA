/**
 * @file Esp32Hardware.h
 * @brief Implementación de IHardware con los GPIO del ESP32-S3.
 *
 * - Salidas: digitalWrite() sobre los LED de luces y aspersores.
 * - Entradas: INPUT_PULLUP + antirrebote por software. `poll()` se llama
 *   desde la tarea de E/S cada AGENT_INPUT_POLL_MS y reporta los cambios
 *   estables mediante un callback.
 *
 * Al arrancar todas las salidas quedan APAGADAS (estado seguro). El estado de
 * las salidas no se persiste: tras un reinicio, solo las tareas programadas
 * se recuperan (ver docs/OPERACION_Y_FALLOS.md).
 */
#pragma once

#include <Arduino.h>

#include "../core/DeviceCatalog.h"
#include "../core/Ports.h"

namespace agent {

class Esp32Hardware : public IHardware {
 public:
  typedef void (*ChangeCallback)(const DeviceInfo& dev, bool open, void* ctx);

  /** Configura todos los pines del catálogo y lee el estado inicial de las entradas. */
  void begin();

  /** Muestreo con antirrebote. Llama `cb` por cada cambio estable. */
  void poll(uint32_t nowMs, ChangeCallback cb, void* ctx);

  void writeOutput(const DeviceInfo& dev, bool on) override;
  bool readOutput(const DeviceInfo& dev) const override;
  bool isOpen(const DeviceInfo& dev) const override;

 private:
  static const size_t kMax = 24;
  volatile bool outputs_[kMax] = {};
  volatile bool stable_[kMax] = {};     ///< Estado aceptado (true = abierto)
  bool lastRaw_[kMax] = {};
  uint32_t lastChangeMs_[kMax] = {};

  static size_t indexOf(const DeviceInfo& dev);
};

}  // namespace agent

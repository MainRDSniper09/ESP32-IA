/**
 * @file Ports.h
 * @brief Interfaces ("puertos") que separan la lógica del hardware.
 *
 * El núcleo (`Agent`) solo conoce estas interfaces. En el ESP32-S3 se
 * implementan en `src/platform/` (GPIO, NTP, NVS, FreeRTOS). En las pruebas
 * unitarias se implementan con objetos falsos en memoria.
 */
#pragma once

#include <string>

#include "Types.h"

namespace agent {

/** Acceso a salidas y entradas digitales. */
class IHardware {
 public:
  virtual ~IHardware() = default;
  /** Escribe una salida (luz/aspersor). */
  virtual void writeOutput(const DeviceInfo& dev, bool on) = 0;
  /** Último valor escrito en una salida. */
  virtual bool readOutput(const DeviceInfo& dev) const = 0;
  /** Estado (con antirrebote) de una puerta/ventana: true = abierta. */
  virtual bool isOpen(const DeviceInfo& dev) const = 0;
};

/** Reloj de pared. */
class IClock {
 public:
  virtual ~IClock() = default;
  /** Hora actual (epoch UTC). */
  virtual time_t now() const = 0;
  /** true cuando la hora es confiable (sincronizada por NTP). */
  virtual bool isSynced() const = 0;
  /** Segundos desde el arranque (siempre disponible). */
  virtual uint32_t uptimeSeconds() const = 0;
};

/** Almacenamiento no volátil de las tareas pendientes (blob JSON). */
class ITaskStore {
 public:
  virtual ~ITaskStore() = default;
  virtual bool load(std::string& out) = 0;
  virtual bool save(const std::string& data) = 0;
};

/** Exclusión mutua entre tareas de FreeRTOS. En pruebas es un no-op. */
class ILock {
 public:
  virtual ~ILock() = default;
  virtual void lock() = 0;
  virtual void unlock() = 0;
};

/** Guardia RAII para ILock. */
class LockGuard {
 public:
  explicit LockGuard(ILock* l) : l_(l) { if (l_) l_->lock(); }
  ~LockGuard() { if (l_) l_->unlock(); }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;
 private:
  ILock* l_;
};

}  // namespace agent

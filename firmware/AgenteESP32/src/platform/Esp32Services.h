/**
 * @file Esp32Services.h
 * @brief Implementaciones ESP32 de IClock, ITaskStore e ILock.
 */
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../core/Ports.h"

namespace agent {

/**
 * Reloj basado en el RTC interno + SNTP.
 * `begin()` fija la zona horaria y arranca la sincronización NTP (no bloquea).
 * `isSynced()` es true cuando la hora es posterior a 2024-01-01, es decir,
 * cuando el SNTP ya respondió al menos una vez desde el arranque.
 */
class Esp32Clock : public IClock {
 public:
  void begin(const char* tz, const char* ntp1, const char* ntp2, const char* ntp3);
  time_t now() const override;
  bool isSynced() const override;
  uint32_t uptimeSeconds() const override;
};

/**
 * Persistencia de tareas en la NVS (memoria flash no volátil con nivelación
 * de desgaste) usando la librería Preferences. Se guarda un único blob JSON
 * en el espacio de nombres "agente", clave "tasks".
 */
class NvsTaskStore : public ITaskStore {
 public:
  bool begin();
  bool load(std::string& out) override;
  bool save(const std::string& data) override;
  /** Borra todo lo guardado (útil para pruebas). */
  void clear();

 private:
  Preferences prefs_;
  bool ok_ = false;
  std::string last_;  ///< Evita escrituras repetidas idénticas (desgaste de flash)
};

/**
 * Mutex recursivo de FreeRTOS. Protege al Agent, que es usado a la vez por la
 * tarea de E/S, la tarea de red y el servidor web. Llamar begin() en setup().
 */
class RtosLock : public ILock {
 public:
  void begin() {
    if (!m_) m_ = xSemaphoreCreateRecursiveMutex();
  }
  void lock() override {
    if (m_) xSemaphoreTakeRecursive(m_, portMAX_DELAY);
  }
  void unlock() override {
    if (m_) xSemaphoreGiveRecursive(m_);
  }

 private:
  SemaphoreHandle_t m_ = nullptr;
};

}  // namespace agent

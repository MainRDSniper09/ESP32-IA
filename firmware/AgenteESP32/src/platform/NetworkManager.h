/**
 * @file NetworkManager.h
 * @brief Wi-Fi con punto de acceso de respaldo, mDNS y reconexión automática.
 *
 * Comportamiento (ver docs/OPERACION_Y_FALLOS.md):
 *  1. Intenta conectarse a la red configurada durante AGENT_WIFI_CONNECT_TIMEOUT_MS.
 *  2. Si no lo logra (o no hay SSID configurado), abre el punto de acceso
 *     "Agente-ESP32" (IP 192.168.4.1) para que la interfaz web siga disponible.
 *     El ESP32 sigue intentando conectarse a la red en segundo plano (modo AP+STA).
 *  3. Cuando la red vuelve, el punto de acceso se apaga.
 *
 * Sin red: el control manual, el monitoreo y las tareas YA programadas siguen
 * funcionando (si la hora estaba sincronizada). El LLM no está disponible.
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace agent {

class NetworkManager {
 public:
  typedef void (*EventCallback)(bool warning, const char* message);

  void begin(EventCallback cb);
  /** Supervisión periódica (llamar desde loop()). */
  void loop();

  bool staConnected() const;
  bool apActive() const { return apActive_; }
  void writeStatus(JsonObject out) const;

 private:
  void startAp();
  void stopAp();

  EventCallback cb_ = nullptr;
  bool apActive_ = false;
  bool wasConnected_ = false;
  bool hasCredentials_ = false;
  uint32_t lastCheckMs_ = 0;
  uint32_t lastRetryMs_ = 0;
};

}  // namespace agent

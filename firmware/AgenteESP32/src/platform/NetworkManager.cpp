#include "NetworkManager.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "../config/AppConfig.h"
#include "../config/Secrets.h"

namespace agent {

void NetworkManager::begin(EventCallback cb) {
  cb_ = cb;
  hasCredentials_ = strlen(SECRET_WIFI_SSID) > 0;

  WiFi.persistent(false);  // no reescribir credenciales en flash en cada arranque
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(AGENT_HOSTNAME);
  WiFi.setAutoReconnect(true);

  if (hasCredentials_) {
    Serial.printf("[wifi] Conectando a \"%s\"...\n", SECRET_WIFI_SSID);
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < AGENT_WIFI_CONNECT_TIMEOUT_MS) {
      delay(250);  // solo en setup(); la tarea de E/S ya está corriendo en paralelo
    }
  } else {
    Serial.println("[wifi] Sin SSID configurado (config/agente_secrets.h)");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wasConnected_ = true;
    Serial.printf("[wifi] Conectado. IP: %s  RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (cb_) cb_(false, "Wi-Fi conectado");
  } else {
    startAp();
  }

  if (MDNS.begin(AGENT_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mdns] http://%s.local\n", AGENT_HOSTNAME);
  }
}

void NetworkManager::startAp() {
  if (apActive_) return;
  WiFi.mode(hasCredentials_ ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AGENT_FALLBACK_AP_SSID, SECRET_AP_PASSWORD);
  apActive_ = true;
  Serial.printf("[wifi] Punto de acceso de respaldo \"%s\" activo en http://%s\n", AGENT_FALLBACK_AP_SSID,
                WiFi.softAPIP().toString().c_str());
  if (cb_) cb_(true, "Sin Wi-Fi: punto de acceso de respaldo activo (192.168.4.1). LLM no disponible.");
}

void NetworkManager::stopAp() {
  if (!apActive_) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive_ = false;
  Serial.println("[wifi] Punto de acceso de respaldo apagado");
}

void NetworkManager::loop() {
  uint32_t now = millis();
  if (now - lastCheckMs_ < 2000) return;
  lastCheckMs_ = now;

  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !wasConnected_) {
    wasConnected_ = true;
    Serial.printf("[wifi] Reconectado. IP: %s\n", WiFi.localIP().toString().c_str());
    if (cb_) cb_(false, "Wi-Fi reconectado");
    stopAp();
  } else if (!connected && wasConnected_) {
    wasConnected_ = false;
    Serial.println("[wifi] Conexión perdida");
    if (cb_) cb_(true, "Conexión Wi-Fi perdida: el LLM no está disponible; las tareas locales continúan");
    lastRetryMs_ = now;
  }

  // Sin conexión por más de 30 s: abrir AP de respaldo y reintentar periódicamente.
  if (!connected && hasCredentials_ && now - lastRetryMs_ > 30000) {
    lastRetryMs_ = now;
    startAp();
    WiFi.disconnect();
    WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  }
}

bool NetworkManager::staConnected() const { return WiFi.status() == WL_CONNECTED; }

void NetworkManager::writeStatus(JsonObject out) const {
  bool sta = staConnected();
  out["connected"] = sta;
  out["ssid"] = hasCredentials_ ? SECRET_WIFI_SSID : "";
  out["ip"] = sta ? WiFi.localIP().toString() : String("");
  out["rssi"] = sta ? WiFi.RSSI() : 0;
  out["hostname"] = AGENT_HOSTNAME;
  out["ap_active"] = apActive_;
  out["ap_ssid"] = apActive_ ? AGENT_FALLBACK_AP_SSID : "";
  out["ap_ip"] = apActive_ ? WiFi.softAPIP().toString() : String("");
}

}  // namespace agent

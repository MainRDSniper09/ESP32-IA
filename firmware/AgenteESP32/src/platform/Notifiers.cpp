#include "Notifiers.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../config/Pins.h"
#include "../config/Secrets.h"

namespace agent {

// =========================================================== LocalIndicator
void LocalIndicator::begin() {
  pinMode(pins::LED_ALERTA, OUTPUT);
  pinMode(pins::BUZZER, OUTPUT);
  pinMode(pins::LED_ESTADO, OUTPUT);
  digitalWrite(pins::LED_ALERTA, LOW);
  digitalWrite(pins::BUZZER, LOW);
  digitalWrite(pins::LED_ESTADO, LOW);
}

void LocalIndicator::update(uint32_t nowMs, bool networkOk, uint32_t unackedAlerts) {
  // LED de alerta: 4 Hz mientras haya alertas pendientes.
  bool alertOn = unackedAlerts > 0 && ((nowMs / 125) % 2 == 0);
  digitalWrite(pins::LED_ALERTA, alertOn ? HIGH : LOW);

  // LED de estado: fijo con red, parpadeo de 1 Hz sin red.
  bool statusOn = networkOk || ((nowMs / 500) % 2 == 0);
  digitalWrite(pins::LED_ESTADO, statusOn ? HIGH : LOW);

  // Zumbador: pitido de 150 ms por cada solicitud.
  uint32_t req = beepRequestMs_;
  if (req != 0 && req != beepStartedMs_ && !beeping_) {
    beepStartedMs_ = req;
    beeping_ = true;
    digitalWrite(pins::BUZZER, HIGH);
  }
  if (beeping_ && nowMs - beepStartedMs_ >= 150) {
    beeping_ = false;
    digitalWrite(pins::BUZZER, LOW);
  }
}

// ========================================================= TelegramNotifier
namespace {
const size_t kMsgLen = 200;
struct QueuedMessage {
  char text[kMsgLen];
};
}  // namespace

void TelegramNotifier::begin() {
  enabled_ = strlen(SECRET_TELEGRAM_BOT_TOKEN) > 0 && strlen(SECRET_TELEGRAM_CHAT_ID) > 0;
  if (enabled_) queue_ = xQueueCreate(8, sizeof(QueuedMessage));
}

void TelegramNotifier::enqueue(const char* text) {
  if (!enabled_ || !queue_) return;
  QueuedMessage m;
  strncpy(m.text, text, kMsgLen - 1);
  m.text[kMsgLen - 1] = '\0';
  xQueueSend(queue_, &m, 0);  // sin espera: nunca bloquea a quien genera la alerta
}

void TelegramNotifier::process(bool networkOk) {
  if (!enabled_ || !queue_ || !networkOk) return;
  QueuedMessage m;
  while (xQueueReceive(queue_, &m, 0) == pdTRUE) {
    if (send(m.text)) ++sent_;
    else ++failed_;
  }
}

bool TelegramNotifier::send(const char* text) {
  WiFiClientSecure client;
  client.setInsecure();  // ver docs/SECURITY_NOTES.md
  HTTPClient http;
  String url = String("https://api.telegram.org/bot") + SECRET_TELEGRAM_BOT_TOKEN + "/sendMessage";
  if (!http.begin(client, url)) return false;
  http.setConnectTimeout(8000);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  JsonDocument doc;
  doc["chat_id"] = SECRET_TELEGRAM_CHAT_ID;
  doc["text"] = String("🔔 Agente ESP32: ") + text;
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  if (code != 200) Serial.printf("[telegram] Error HTTP %d\n", code);
  return code == 200;
}

}  // namespace agent

/**
 * @file Notifiers.h
 * @brief Canales de notificación.
 *
 * 1. LocalIndicator (siempre activo, comprobable sin internet):
 *    - LED de ALERTA (GPIO17): parpadea mientras haya alertas sin reconocer.
 *    - Zumbador (GPIO18, opcional): pitido corto en cada alerta nueva.
 *    - LED de ESTADO (GPIO21): fijo = Wi-Fi OK; parpadeo lento = sin red/AP.
 *    - La interfaz web muestra además cada alerta como aviso emergente.
 *
 * 2. TelegramNotifier (opcional): reenvía las alertas a un chat de Telegram si
 *    SECRET_TELEGRAM_BOT_TOKEN y SECRET_TELEGRAM_CHAT_ID están configurados.
 *    Los mensajes se encolan (sin bloquear) y se envían desde la tarea de red.
 */
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace agent {

class LocalIndicator {
 public:
  void begin();
  /** Pide un pitido (seguro desde cualquier tarea). */
  void beep() { beepRequestMs_ = millis(); }
  /** Actualiza LED y zumbador. Llamar periódicamente desde la tarea de E/S. */
  void update(uint32_t nowMs, bool networkOk, uint32_t unackedAlerts);

 private:
  volatile uint32_t beepRequestMs_ = 0;
  uint32_t beepStartedMs_ = 0;
  bool beeping_ = false;
};

class TelegramNotifier {
 public:
  void begin();
  bool enabled() const { return enabled_; }
  /** Encola un mensaje (no bloquea; si la cola está llena se descarta). */
  void enqueue(const char* text);
  /** Envía los mensajes pendientes. Solo desde la tarea de red. */
  void process(bool networkOk);
  uint32_t sent() const { return sent_; }
  uint32_t failed() const { return failed_; }

 private:
  bool send(const char* text);
  bool enabled_ = false;
  QueueHandle_t queue_ = nullptr;
  uint32_t sent_ = 0;
  uint32_t failed_ = 0;
};

}  // namespace agent

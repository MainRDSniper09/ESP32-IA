/**
 * @file Pins.h
 * @brief Mapa de pines del prototipo (ESP32-S3-DevKitC-1).
 *
 * Todos los GPIO elegidos son "seguros" en el ESP32-S3:
 *  - NO se usan pines de arranque (strapping): GPIO0, GPIO3, GPIO45, GPIO46.
 *  - NO se usan los pines USB nativos: GPIO19 (D-) y GPIO20 (D+).
 *  - NO se usan los pines de flash/PSRAM: GPIO26..GPIO32 y GPIO33..GPIO37
 *    (estos últimos están ocupados en módulos con PSRAM octal, p. ej. N8R8/N16R8).
 *  - NO se usa GPIO48 (LED RGB integrado en algunas revisiones de la placa).
 *
 * Convenciones eléctricas (ver docs/HARDWARE.md):
 *  - Salidas (luces/aspersores): GPIO -> resistencia 220-330 Ω -> ánodo LED -> cátodo -> GND.
 *    Nivel ALTO = encendido.
 *  - Entradas (puertas/ventanas): interruptor entre el GPIO y GND, con INPUT_PULLUP.
 *    Contacto cerrado (LOW)  = puerta/ventana CERRADA.
 *    Contacto abierto (HIGH) = puerta/ventana ABIERTA.
 *    Así se comporta un sensor magnético (reed switch) real y, si un cable se
 *    suelta, el sistema lo reporta como "abierto" (falla segura).
 *
 * Para cambiar un pin basta con editar este archivo: el resto del firmware
 * (catálogo, validación, prompt del LLM, interfaz web) se adapta solo.
 */
#pragma once

#include <stdint.h>

namespace pins {

// ---------------------------------------------------------------- Luces (LED)
constexpr uint8_t LUZ_ENTRADA      = 4;
constexpr uint8_t LUZ_OFICINA      = 5;
constexpr uint8_t LUZ_PARQUEADERO  = 6;
constexpr uint8_t LUZ_BODEGA       = 7;

// ----------------------------------------------------------- Aspersores (LED)
constexpr uint8_t ASPERSOR_FRONTAL = 15;
constexpr uint8_t ASPERSOR_TRASERO = 16;

// ------------------------------------------------- Puertas (interruptores)
constexpr uint8_t PUERTA_PRINCIPAL = 8;
constexpr uint8_t PUERTA_BODEGA    = 9;
constexpr uint8_t PUERTA_OFICINA   = 10;

// ------------------------------------------------- Ventanas (interruptores)
constexpr uint8_t VENTANA_OFICINA   = 11;
constexpr uint8_t VENTANA_RECEPCION = 12;

// ------------------------------------------------------ Notificación local
/** LED rojo que parpadea mientras haya alertas sin reconocer. */
constexpr uint8_t LED_ALERTA = 17;
/** Zumbador activo opcional (emite un pitido corto en cada alerta). */
constexpr uint8_t BUZZER     = 18;
/** LED de estado: fijo = Wi-Fi conectado, parpadeo lento = modo AP / sin red. */
constexpr uint8_t LED_ESTADO = 21;

}  // namespace pins

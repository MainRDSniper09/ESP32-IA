// =====================================================================
//  config.h  —  CONFIGURACIÓN GENERAL DEL PROYECTO
// ---------------------------------------------------------------------
//  Aquí se cambian los pines, la zona horaria y algunos parámetros.
//  Las claves (Wi-Fi y API del LLM) NO van aquí: van en credenciales.h
// =====================================================================
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------- Hora
// Zona horaria en formato POSIX. "<-05>5" = Colombia (UTC-5).
// Otros ejemplos: México "CST6", Argentina "<-03>3", España "CET-1CEST,M3.5.0,M10.5.0/3"
#define ZONA_HORARIA  "<-05>5"
#define SERVIDOR_NTP  "pool.ntp.org"

// ---------------------------------------------------------------- Wi-Fi
// Si el ESP32 no logra conectarse a tu red, crea su propia red Wi-Fi con
// este nombre y clave, para que la página web siga funcionando.
#define RED_RESPALDO    "Agente-ESP32"
#define CLAVE_RESPALDO  "agente1234"   // mínimo 8 caracteres

// ---------------------------------------------------------------- LLM
#define MODELO_LLM  "claude-opus-5-5"   // modelo de Claude que interpreta las órdenes

// ---------------------------------------------------------------- Pines
// Salidas (LED que simulan luces y aspersores)
#define PIN_LUZ_ENTRADA       4
#define PIN_LUZ_OFICINA       5
#define PIN_LUZ_PARQUEADERO   6
#define PIN_LUZ_BODEGA        7
#define PIN_ASPERSOR_FRONTAL  15
#define PIN_ASPERSOR_TRASERO  16

// Entradas (interruptores que simulan puertas y ventanas)
#define PIN_PUERTA_PRINCIPAL  8
#define PIN_PUERTA_BODEGA     9
#define PIN_PUERTA_OFICINA    10
#define PIN_VENTANA_OFICINA   11
#define PIN_VENTANA_RECEPCION 12

// LED rojo que parpadea cuando hay alertas sin revisar
#define PIN_LED_ALERTA        17

// ---------------------------------------------------------------- Límites
#define MAX_TAREAS          20   // tareas programadas que se pueden guardar
#define MAX_ACCIONES        6    // acciones máximas en una sola orden
#define MAX_AVISOS          20   // avisos que se muestran en la página
#define MINUTOS_TOLERANCIA  10   // si una tarea se "pasó" más de esto (equipo apagado), no se ejecuta

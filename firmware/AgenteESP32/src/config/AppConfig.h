/**
 * @file AppConfig.h
 * @brief Parámetros generales del agente (no secretos).
 *
 * Todo lo que NO es una credencial vive aquí. Las credenciales (Wi-Fi, clave
 * de la API del LLM, token de Telegram...) van en `agente_secrets.h`, que está en
 * .gitignore. Copie `agente_secrets.example.h` como `agente_secrets.h` y complételo.
 *
 * Cada valor puede sobrescribirse desde `platformio.ini` con `-D NOMBRE=valor`.
 */
#pragma once

/** Convierte una macro numérica en literal de texto: AGENT_STR(8) -> "8". */
#define AGENT_STR_(x) #x
#define AGENT_STR(x) AGENT_STR_(x)

// --------------------------------------------------------------- Identidad
#define AGENT_FIRMWARE_VERSION "1.0.0"
#ifndef AGENT_HOSTNAME
#define AGENT_HOSTNAME "agente-esp32"  ///< Nombre mDNS: http://agente-esp32.local
#endif

// ------------------------------------------------------------- Hora / NTP
/**
 * Zona horaria en formato POSIX. Por defecto Colombia (UTC-5, sin horario de verano).
 * Ejemplos: México centro "CST6", Perú "<-05>5", Argentina "<-03>3",
 * España "CET-1CEST,M3.5.0,M10.5.0/3".
 */
#ifndef AGENT_TIMEZONE
#define AGENT_TIMEZONE "<-05>5"
#endif
#ifndef AGENT_NTP_SERVER_1
#define AGENT_NTP_SERVER_1 "pool.ntp.org"
#endif
#ifndef AGENT_NTP_SERVER_2
#define AGENT_NTP_SERVER_2 "time.google.com"
#endif
#ifndef AGENT_NTP_SERVER_3
#define AGENT_NTP_SERVER_3 "time.cloudflare.com"
#endif

// ------------------------------------------------------------------ Wi-Fi
/** Tiempo máximo esperando la red al arrancar antes de abrir el punto de acceso de respaldo. */
#ifndef AGENT_WIFI_CONNECT_TIMEOUT_MS
#define AGENT_WIFI_CONNECT_TIMEOUT_MS 20000
#endif
/** SSID del punto de acceso de respaldo (modo sin Wi-Fi). La clave está en agente_secrets.h. */
#ifndef AGENT_FALLBACK_AP_SSID
#define AGENT_FALLBACK_AP_SSID "Agente-ESP32"
#endif

// ------------------------------------------------------------------- LLM
/** Tiempo máximo de una solicitud HTTPS al LLM. */
#ifndef AGENT_LLM_TIMEOUT_MS
#define AGENT_LLM_TIMEOUT_MS 45000
#endif
/** Reintentos ante errores transitorios (HTTP 429/5xx o fallo de red). */
#ifndef AGENT_LLM_MAX_RETRIES
#define AGENT_LLM_MAX_RETRIES 1
#endif
/**
 * Anthropic: nivel de esfuerzo (output_config.effort). "low" da respuestas
 * rápidas y es suficiente para traducir una orden a JSON. "" = no enviarlo
 * (necesario para modelos antiguos que no admiten effort).
 */
#ifndef AGENT_LLM_EFFORT
#define AGENT_LLM_EFFORT "low"
#endif
/** Anthropic: tope de tokens de salida (incluye el razonamiento interno del modelo). */
#ifndef AGENT_LLM_MAX_TOKENS
#define AGENT_LLM_MAX_TOKENS 8192
#endif
/** OpenAI-compatible: pedir response_format = json_object (1) o no (0). */
#ifndef AGENT_OPENAI_JSON_MODE
#define AGENT_OPENAI_JSON_MODE 1
#endif
/** Longitud máxima aceptada para una instrucción en lenguaje natural. */
#ifndef AGENT_MAX_COMMAND_CHARS
#define AGENT_MAX_COMMAND_CHARS 400
#endif

// ----------------------------------------------------------- Planificador
/** Máximo de tareas programadas pendientes (limitado por la NVS). */
#ifndef AGENT_MAX_TASKS
#define AGENT_MAX_TASKS 24
#endif
/** Máximo de pasos en un plan devuelto por el LLM. */
#ifndef AGENT_MAX_PLAN_STEPS
#define AGENT_MAX_PLAN_STEPS 8
#endif
/** Retardo máximo aceptado para una tarea (7 días). */
#ifndef AGENT_MAX_DELAY_S
#define AGENT_MAX_DELAY_S (7UL * 24UL * 3600UL)
#endif
/**
 * Si una tarea se encuentra vencida por más de este tiempo (p. ej. el equipo
 * estuvo apagado), NO se ejecuta: se marca como "perdida" y se genera alerta.
 */
#ifndef AGENT_MISSED_TASK_GRACE_S
#define AGENT_MISSED_TASK_GRACE_S 600
#endif

// ---------------------------------------------------------------- E/S
/** Periodo de muestreo de los interruptores. */
#ifndef AGENT_INPUT_POLL_MS
#define AGENT_INPUT_POLL_MS 10
#endif
/** Tiempo de estabilidad para aceptar un cambio (antirrebote). */
#ifndef AGENT_DEBOUNCE_MS
#define AGENT_DEBOUNCE_MS 50
#endif
/** Periodo con el que el planificador revisa tareas vencidas. */
#ifndef AGENT_SCHEDULER_TICK_MS
#define AGENT_SCHEDULER_TICK_MS 250
#endif

// ------------------------------------------------------------ Registro
/** Capacidad del registro circular de eventos/alertas en RAM. */
#ifndef AGENT_EVENT_LOG_SIZE
#define AGENT_EVENT_LOG_SIZE 40
#endif
/** Trabajos (jobs) de lenguaje natural que se recuerdan para consulta. */
#ifndef AGENT_JOB_HISTORY
#define AGENT_JOB_HISTORY 6
#endif

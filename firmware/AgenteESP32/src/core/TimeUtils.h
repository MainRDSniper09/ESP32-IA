/**
 * @file TimeUtils.h
 * @brief Utilidades de fecha/hora local (zona horaria definida con la variable TZ).
 *
 * En el ESP32-S3 la zona se fija con `configTzTime()`; en las pruebas con
 * `setenv("TZ", ...)` + `tzset()`. Todas las funciones usan la hora LOCAL
 * para interpretar "HH:MM" y devuelven instantes epoch (UTC).
 */
#pragma once

#include <time.h>

#include <string>

namespace agent {
namespace timeutil {

/** Hora mínima considerada válida (2024-01-01). Antes de esto el reloj no está sincronizado. */
const time_t kMinValidEpoch = 1704067200;

/** Analiza "HH:MM" (24 h, exactamente 5 caracteres). */
bool parseHHMM(const std::string& s, int& hour, int& minute);

/** Analiza "YYYY-MM-DD" verificando días por mes y años bisiestos. */
bool parseDate(const std::string& s, int& year, int& month, int& day);

/**
 * Convierte una hora local en instante epoch.
 * - Sin fecha: hoy a esa hora; si ya pasó (<= now), mañana.
 * - Con fecha: ese día a esa hora; si ya pasó devuelve false.
 */
bool resolveLocalTime(time_t now, int hour, int minute, const std::string& date, time_t& out);

/** Suma `days` días conservando la hora local (correcto ante cambios de horario). */
time_t addLocalDays(time_t t, int days);

/** "2026-11-06T20:00:00-05:00" */
std::string toIso8601(time_t t);

/** "20:00" */
std::string toHHMM(time_t t);

/** "06/11 20:00" */
std::string toShort(time_t t);

/** Descompone en hora local (portátil). */
void toLocal(time_t t, struct tm& out);

}  // namespace timeutil
}  // namespace agent

/**
 * @file TextUtils.h
 * @brief Utilidades de texto: normalización en español, extracción de JSON
 *        y detección de expresiones temporales ambiguas.
 */
#pragma once

#include <string>

namespace agent {
namespace text {

/**
 * Minúsculas + elimina tildes (UTF-8) + colapsa espacios.
 * "¿Más  TARDE?" -> "¿mas tarde?"
 */
std::string normalize(const std::string& in);

/**
 * Extrae el primer objeto JSON balanceado de una respuesta del LLM.
 * Tolera texto alrededor y bloques ```json ... ```. Respeta llaves dentro de
 * cadenas. Devuelve false si no hay un objeto completo.
 */
bool extractJsonObject(const std::string& in, std::string& out);

/**
 * Detección determinista de horarios ambiguos ("más tarde", "en un rato"...).
 *
 * Devuelve true (y la frase encontrada) si el texto contiene una expresión
 * temporal vaga y NO contiene ninguna referencia temporal explícita (dígitos,
 * números escritos, "ahora"...). Las preguntas (texto con '?') se ignoran.
 *
 * Es una segunda barrera además de las instrucciones del LLM: aunque el
 * modelo "invente" una hora, el firmware pide aclaración antes de guardar.
 */
bool findAmbiguousTime(const std::string& in, std::string& phrase);

/** Recorta a `max` bytes sin partir caracteres UTF-8. */
std::string truncateUtf8(const std::string& in, size_t max);

}  // namespace text
}  // namespace agent

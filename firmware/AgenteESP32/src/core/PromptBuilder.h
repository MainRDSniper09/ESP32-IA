/**
 * @file PromptBuilder.h
 * @brief Construye las instrucciones que se envían al LLM.
 *
 * El prompt se genera a partir de los catálogos (dispositivos y acciones), así
 * que nunca queda desactualizado respecto al firmware. Le indica al modelo:
 *  - que SOLO responda un objeto JSON con el formato de docs/JSON_FORMAT.md,
 *  - qué acciones, dispositivos, grupos y condiciones existen,
 *  - que pida aclaración ante horarios ambiguos,
 *  - que nunca invente el estado de los sensores (el firmware lo evalúa).
 *
 * - `systemPrompt()` es estable (no cambia entre solicitudes).
 * - `userMessage()` incluye la hora local actual y, si existe, el contexto de
 *   una aclaración previa.
 */
#pragma once

#include <string>

namespace agent {
namespace prompt {

/** Instrucciones fijas del sistema. */
std::string systemPrompt();

/**
 * Mensaje del usuario.
 * @param text            Instrucción en lenguaje natural.
 * @param nowLocalIso     Hora local actual ("" si el reloj no está sincronizado).
 * @param weekday         Día de la semana en español ("" si no se conoce).
 * @param prevRequest     Solicitud original si esta es la respuesta a una aclaración ("" si no).
 * @param prevQuestion    Pregunta de aclaración que se hizo ("" si no).
 */
std::string userMessage(const std::string& text, const std::string& nowLocalIso, const std::string& weekday,
                        const std::string& prevRequest, const std::string& prevQuestion);

}  // namespace prompt
}  // namespace agent

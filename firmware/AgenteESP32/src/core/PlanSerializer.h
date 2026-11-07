/**
 * @file PlanSerializer.h
 * @brief Convierte estructuras tipadas de vuelta a JSON canónico y a texto legible.
 *
 * - El JSON canónico tiene exactamente el formato que acepta PlanValidator,
 *   así que se usa también para persistir tareas en la NVS: al recuperarlas
 *   tras un reinicio, pasan otra vez por el validador.
 * - `describe*` genera frases en español para la interfaz y los eventos.
 */
#pragma once

#include <ArduinoJson.h>

#include <string>

#include "Types.h"

namespace agent {
namespace serializer {

/** {"action":..., "params":{...}} (+ "condition" si existe). Sin "when". */
void writeStepCore(const PlanStep& step, JsonObject out);

/** Paso completo, incluido "when". */
void writeStep(const PlanStep& step, JsonObject out);

/** Plan completo normalizado. */
void writePlan(const Plan& plan, JsonObject out);

/** "encender Luz de la entrada", "apagar las luces", "notificar: «...»" */
std::string describeAction(const Action& a);

/** " si todas las puertas están cerradas" (vacío si no hay condición) */
std::string describeCondition(const Condition& c);

/** Acción + condición. */
std::string describeStep(const PlanStep& s);

const char* conditionTypeName(ConditionType t);
const char* levelName(EventLevel l);

}  // namespace serializer
}  // namespace agent

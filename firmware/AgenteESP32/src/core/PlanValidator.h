/**
 * @file PlanValidator.h
 * @brief Valida el JSON producido por el LLM y lo convierte en un `Plan` tipado.
 *
 * Es la frontera de confianza del sistema: el JSON del LLM se trata como
 * entrada NO confiable. Se verifica, campo por campo:
 *  - esquema (claves permitidas, claves obligatorias, sin claves desconocidas),
 *  - tipos de datos (cadena, entero, booleano; "900" no es un entero),
 *  - nombres de acciones contra el catálogo de funciones autorizadas,
 *  - nombres de dispositivos/grupos contra el catálogo de dispositivos,
 *  - horarios ("HH:MM" válido, fechas reales, retardos dentro de límites),
 *  - coherencia (after_step apunta a un paso anterior, consultas no programables...).
 *
 * Si hay CUALQUIER error, el plan completo se rechaza (nada se ejecuta).
 * El formato está documentado en docs/JSON_FORMAT.md.
 */
#pragma once

#include <ArduinoJson.h>

#include <vector>

#include "Types.h"

namespace agent {

class PlanValidator {
 public:
  /**
   * Valida un plan completo.
   * @param root   Documento JSON (objeto raíz).
   * @param out    Plan tipado (solo es válido si retorna true).
   * @param errors Lista de errores encontrados (vacía si retorna true).
   */
  static bool validatePlan(JsonVariantConst root, Plan& out, std::vector<ValidationError>& errors);

  /**
   * Valida un único paso. `index` se usa para los mensajes de error y para
   * verificar referencias `after_step`. Con `allowTiming = false` el campo
   * `when` está prohibido (se usa al recuperar tareas de la NVS).
   */
  static bool validateStep(JsonVariantConst step, size_t index, PlanStep& out,
                           std::vector<ValidationError>& errors, bool allowTiming = true);

 private:
  static void validateParams(JsonVariantConst params, const std::string& path, Action& action,
                             std::vector<ValidationError>& errors);
  static void validateWhen(JsonVariantConst when, const std::string& path, size_t index, Timing& out,
                           std::vector<ValidationError>& errors);
  static void validateCondition(JsonVariantConst cond, const std::string& path, Condition& out,
                                std::vector<ValidationError>& errors);
};

}  // namespace agent

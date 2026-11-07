/**
 * @file ActionCatalog.h
 * @brief Catálogo explícito de acciones (funciones C++) que el LLM puede seleccionar.
 *
 * El LLM NO ejecuta código: solo elige un `action` de esta tabla y llena sus
 * `params`. Cualquier nombre que no esté aquí se rechaza. La misma tabla sirve
 * para:
 *  - validar los parámetros (tipo, obligatoriedad, valores permitidos),
 *  - describir las acciones al LLM en el prompt,
 *  - publicar el catálogo en GET /api/catalog.
 *
 * La implementación de cada acción está en Agent.cpp (métodos `do*`).
 */
#pragma once

#include <stdint.h>

#include <string>

#include "Types.h"

namespace agent {

enum class ParamType : uint8_t {
  String,   ///< Texto libre (longitud limitada)
  Integer,  ///< Entero no negativo
  Boolean,  ///< true / false
  Enum,     ///< Texto restringido a una lista "a|b|c"
};

struct ParamSpec {
  const char* name;
  ParamType type;
  bool required;
  const char* allowed;      ///< Enum: valores separados por '|'. nullptr en otros tipos.
  const char* description;  ///< Para el prompt y la documentación.
};

struct ActionSpec {
  const char* name;         ///< Nombre en el JSON: "set_device"
  ActionId id;
  const char* description;
  const ParamSpec* params;
  uint8_t paramCount;
  bool schedulable;         ///< ¿Puede programarse para el futuro?
};

namespace actions {

size_t count();
const ActionSpec& at(size_t i);
/** Busca por nombre. nullptr si no existe. */
const ActionSpec* find(const std::string& name);
/** Busca por id (siempre existe). */
const ActionSpec& spec(ActionId id);
/** Busca un parámetro dentro de una acción. nullptr si no existe. */
const ParamSpec* findParam(const ActionSpec& a, const std::string& name);
/** ¿`value` está en la lista "a|b|c"? */
bool enumContains(const char* allowed, const std::string& value);
/** "string", "integer", "boolean" o "enum". */
const char* paramTypeName(ParamType t);

}  // namespace actions
}  // namespace agent

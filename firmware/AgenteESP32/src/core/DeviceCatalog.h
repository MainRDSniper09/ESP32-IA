/**
 * @file DeviceCatalog.h
 * @brief Catálogo estático de dispositivos y grupos.
 *
 * Es la ÚNICA fuente de verdad sobre qué dispositivos existen. La usan:
 *  - el validador (rechaza nombres desconocidos),
 *  - el constructor del prompt (le dice al LLM qué nombres puede usar),
 *  - la API web (/api/devices, /api/catalog),
 *  - la capa de hardware (configura los GPIO).
 *
 * Grupos de SALIDAS : "lights", "sprinklers", "all"
 * Grupos de ACCESOS : "doors", "windows", "accesses"
 *
 * Nota: el código del núcleo es C++11 para compilar con cualquier versión
 * del core Arduino-ESP32 (2.x usa gnu++11).
 */
#pragma once

#include <stddef.h>

#include <string>

#include "Types.h"

namespace agent {
namespace devices {

/** Número de dispositivos del catálogo. */
size_t count();

/** Dispositivo en la posición i (0 <= i < count()). */
const DeviceInfo& at(size_t i);

/** Busca por id canónico. Devuelve nullptr si no existe. */
const DeviceInfo* find(const std::string& id);

/** "light", "sprinkler", "door" o "window". */
const char* kindName(DeviceKind kind);

/** true si `group` es lights|sprinklers|all. */
bool isOutputGroup(const std::string& group);

/** true si `group` es doors|windows|accesses. */
bool isAccessGroup(const std::string& group);

/** ¿La salida `dev` pertenece al grupo de salidas `group`? */
bool inOutputGroup(const DeviceInfo& dev, const std::string& group);

/**
 * ¿El acceso `dev` está incluido en `target`?
 * `target` puede ser un grupo de accesos o el id de una puerta/ventana.
 */
bool inAccessTarget(const DeviceInfo& dev, const std::string& target);

/** true si `target` es un grupo de accesos o el id de una puerta/ventana. */
bool isValidAccessTarget(const std::string& target);

/** Etiqueta en español de un grupo o dispositivo ("las puertas", "Puerta principal"). */
std::string targetLabel(const std::string& target);

}  // namespace devices
}  // namespace agent

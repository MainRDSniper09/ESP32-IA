#include "DeviceCatalog.h"

#include "../config/Pins.h"

namespace agent {
namespace devices {

namespace {

/**
 * Tabla de dispositivos. Para agregar uno nuevo:
 *   1. Declare su pin en config/Pins.h
 *   2. Agregue una fila aquí (id en snake_case, sin tildes)
 * El prompt del LLM, la validación y la interfaz web lo incluirán automáticamente.
 */
const DeviceInfo kDevices[] = {
    // id                   nombre legible              tipo                  pin
    {"luz_entrada",        "Luz de la entrada",        DeviceKind::Light,     pins::LUZ_ENTRADA},
    {"luz_oficina",        "Luz de la oficina",        DeviceKind::Light,     pins::LUZ_OFICINA},
    {"luz_parqueadero",    "Luz del parqueadero",      DeviceKind::Light,     pins::LUZ_PARQUEADERO},
    {"luz_bodega",         "Luz de la bodega",         DeviceKind::Light,     pins::LUZ_BODEGA},
    {"aspersor_frontal",   "Aspersor jardín frontal",  DeviceKind::Sprinkler, pins::ASPERSOR_FRONTAL},
    {"aspersor_trasero",   "Aspersor jardín trasero",  DeviceKind::Sprinkler, pins::ASPERSOR_TRASERO},
    {"puerta_principal",   "Puerta principal",         DeviceKind::Door,      pins::PUERTA_PRINCIPAL},
    {"puerta_bodega",      "Puerta de la bodega",      DeviceKind::Door,      pins::PUERTA_BODEGA},
    {"puerta_oficina",     "Puerta de la oficina",     DeviceKind::Door,      pins::PUERTA_OFICINA},
    {"ventana_oficina",    "Ventana de la oficina",    DeviceKind::Window,    pins::VENTANA_OFICINA},
    {"ventana_recepcion",  "Ventana de la recepción",  DeviceKind::Window,    pins::VENTANA_RECEPCION},
};

const size_t kCount = sizeof(kDevices) / sizeof(kDevices[0]);

}  // namespace

size_t count() { return kCount; }

const DeviceInfo& at(size_t i) { return kDevices[i < kCount ? i : 0]; }

const DeviceInfo* find(const std::string& id) {
  for (size_t i = 0; i < kCount; ++i) {
    if (id == kDevices[i].id) return &kDevices[i];
  }
  return nullptr;
}

const char* kindName(DeviceKind kind) {
  switch (kind) {
    case DeviceKind::Light: return "light";
    case DeviceKind::Sprinkler: return "sprinkler";
    case DeviceKind::Door: return "door";
    case DeviceKind::Window: return "window";
  }
  return "unknown";
}

bool isOutputGroup(const std::string& g) { return g == "lights" || g == "sprinklers" || g == "all"; }

bool isAccessGroup(const std::string& g) { return g == "doors" || g == "windows" || g == "accesses"; }

bool inOutputGroup(const DeviceInfo& dev, const std::string& g) {
  if (!isOutputKind(dev.kind)) return false;
  if (g == "all") return true;
  if (g == "lights") return dev.kind == DeviceKind::Light;
  if (g == "sprinklers") return dev.kind == DeviceKind::Sprinkler;
  return false;
}

bool inAccessTarget(const DeviceInfo& dev, const std::string& t) {
  if (!isAccessKind(dev.kind)) return false;
  if (t == "accesses") return true;
  if (t == "doors") return dev.kind == DeviceKind::Door;
  if (t == "windows") return dev.kind == DeviceKind::Window;
  return t == dev.id;
}

bool isValidAccessTarget(const std::string& t) {
  if (isAccessGroup(t)) return true;
  const DeviceInfo* d = find(t);
  return d != nullptr && isAccessKind(d->kind);
}

std::string targetLabel(const std::string& t) {
  if (t == "lights") return "las luces";
  if (t == "sprinklers") return "los aspersores";
  if (t == "all") return "todas las salidas";
  if (t == "doors") return "las puertas";
  if (t == "windows") return "las ventanas";
  if (t == "accesses") return "puertas y ventanas";
  const DeviceInfo* d = find(t);
  return d ? std::string(d->name) : t;
}

}  // namespace devices
}  // namespace agent

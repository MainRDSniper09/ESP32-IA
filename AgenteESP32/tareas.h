// =====================================================================
//  tareas.h  —  TAREAS PROGRAMADAS
// ---------------------------------------------------------------------
//  Una tarea es una acción que debe ocurrir en el futuro, por ejemplo
//  "encender luz_entrada a las 20:00".
//
//  - Se guardan en la memoria no volátil (NVS) del ESP32, por lo que
//    sobreviven a un reinicio o a un corte de energía.
//  - revisarTareas() se llama cada segundo y ejecuta las que ya vencieron.
//    No usa delay(): el monitoreo de puertas nunca se detiene.
// =====================================================================
#pragma once
#include <ArduinoJson.h>
#include "config.h"

struct Tarea {
  bool activa;           // false = espacio libre
  uint32_t id;           // número de la tarea (#1, #2, ...)
  uint32_t momento;      // cuándo ejecutarla (segundos desde 1970, "epoch")
  char accion[16];       // "encender", "apagar" o "avisar"
  char objetivo[24];     // dispositivo o grupo, ej: "luz_entrada", "luces"
  char condicion[24];    // "" o una condición, ej: "puertas_cerradas"
  char mensaje[104];     // texto del aviso (solo para "avisar")
};

// Carga las tareas guardadas en la memoria no volátil.
void iniciarTareas();

// Programa una tarea. Devuelve su id, o 0 si no hay espacio.
uint32_t programarTarea(const String& accion, const String& objetivo, const String& condicion,
                        const String& mensaje, time_t momento);

bool cancelarTarea(uint32_t id);
int espaciosLibres();

// Ejecuta las tareas cuya hora ya llegó (llamar cada segundo).
void revisarTareas();

// true cuando el reloj ya se sincronizó por internet (NTP).
bool horaSincronizada();

// "06/11 20:00"
String formatearMomento(time_t momento);

// Para la página web y para la acción "listar_tareas"
void tareasAJson(JsonArray lista);
String textoTareas();

// =====================================================================
//  tareas.cpp
// =====================================================================
#include "tareas.h"
#include <Preferences.h>
#include "avisos.h"
#include "plan.h"

static Tarea tareas[MAX_TAREAS];
static uint32_t siguienteId = 1;
static Preferences memoria;          // acceso a la memoria no volátil (NVS)
static SemaphoreHandle_t candado;    // protege la lista entre las dos tareas del ESP32

// Copia un String a un arreglo de caracteres de tamaño fijo, sin desbordarlo.
static void copiar(char* destino, size_t tam, const String& origen) {
  strncpy(destino, origen.c_str(), tam - 1);
  destino[tam - 1] = '\0';
}

static void guardar() {
  memoria.putBytes("lista", tareas, sizeof(tareas));
  memoria.putUInt("siguiente", siguienteId);
}

bool horaSincronizada() { return time(nullptr) > 1700000000; }  // posterior a nov-2023

String formatearMomento(time_t momento) {
  struct tm t;
  localtime_r(&momento, &t);
  char texto[32];
  snprintf(texto, sizeof(texto), "%02d/%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
  return texto;
}

void iniciarTareas() {
  candado = xSemaphoreCreateMutex();
  memoria.begin("agente", false);
  memset(tareas, 0, sizeof(tareas));

  // Recuperar lo guardado antes del reinicio (si el tamaño coincide)
  if (memoria.getBytesLength("lista") == sizeof(tareas)) {
    memoria.getBytes("lista", tareas, sizeof(tareas));
    siguienteId = memoria.getUInt("siguiente", 1);
  }

  int recuperadas = 0;
  for (int i = 0; i < MAX_TAREAS; i++) {
    Tarea& t = tareas[i];
    if (!t.activa) continue;
    // Revisión de seguridad: solo se aceptan acciones programables conocidas
    String accion = t.accion;
    if (accion != "encender" && accion != "apagar" && accion != "avisar") {
      t.activa = false;
      continue;
    }
    recuperadas++;
  }
  if (recuperadas > 0) {
    agregarAviso("Se recuperaron " + String(recuperadas) + " tarea(s) guardadas antes del reinicio", false);
  }
}

uint32_t programarTarea(const String& accion, const String& objetivo, const String& condicion,
                        const String& mensaje, time_t momento) {
  uint32_t id = 0;
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (tareas[i].activa) continue;
    Tarea& t = tareas[i];
    t.activa = true;
    t.id = siguienteId++;
    t.momento = (uint32_t)momento;
    copiar(t.accion, sizeof(t.accion), accion);
    copiar(t.objetivo, sizeof(t.objetivo), objetivo);
    copiar(t.condicion, sizeof(t.condicion), condicion);
    copiar(t.mensaje, sizeof(t.mensaje), mensaje);
    id = t.id;
    guardar();
    break;
  }
  xSemaphoreGive(candado);
  return id;
}

bool cancelarTarea(uint32_t id) {
  bool encontrada = false;
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (tareas[i].activa && tareas[i].id == id) {
      tareas[i].activa = false;
      encontrada = true;
      guardar();
    }
  }
  xSemaphoreGive(candado);
  if (encontrada) agregarAviso("Tarea #" + String(id) + " cancelada", false);
  return encontrada;
}

int espaciosLibres() {
  int libres = 0;
  for (int i = 0; i < MAX_TAREAS; i++) if (!tareas[i].activa) libres++;
  return libres;
}

void revisarTareas() {
  // Sin hora confiable no se puede saber si una tarea ya venció: se espera.
  static bool avisoHora = false;
  if (!horaSincronizada()) return;
  if (!avisoHora) {
    avisoHora = true;
    agregarAviso("Hora sincronizada por internet: " + formatearMomento(time(nullptr)), false);
  }

  time_t ahora = time(nullptr);
  static Tarea vencidas[MAX_TAREAS];   // static: no ocupa la pila de la tarea
  int cuantas = 0;

  // 1) Sacar de la lista las tareas que ya vencieron (rápido, con candado)
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (tareas[i].activa && tareas[i].momento <= (uint32_t)ahora) {
      vencidas[cuantas++] = tareas[i];
      tareas[i].activa = false;
    }
  }
  if (cuantas > 0) guardar();
  xSemaphoreGive(candado);

  // 2) Ejecutarlas (fuera del candado)
  for (int i = 0; i < cuantas; i++) {
    Tarea& t = vencidas[i];
    long retraso = (long)(ahora - t.momento);
    if (retraso > MINUTOS_TOLERANCIA * 60L) {
      // El equipo estuvo apagado: no se ejecuta tarde (ej. aspersores a las 3 a. m.)
      agregarAviso("La tarea #" + String(t.id) + " (" + t.accion + " " + t.objetivo + ", " +
                   formatearMomento(t.momento) + ") NO se ejecutó: el equipo estaba apagado o sin hora",
                   true);
      continue;
    }
    String resultado = ejecutarAccion(t.accion, t.objetivo, t.condicion, t.mensaje);
    agregarAviso("Tarea #" + String(t.id) + ": " + resultado, false);
  }
}

void tareasAJson(JsonArray lista) {
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    const Tarea& t = tareas[i];
    if (!t.activa) continue;
    JsonObject o = lista.add<JsonObject>();
    o["id"] = t.id;
    o["cuando"] = formatearMomento(t.momento);
    o["accion"] = t.accion;
    o["objetivo"] = t.objetivo;
    o["condicion"] = t.condicion;
    o["mensaje"] = t.mensaje;
  }
  xSemaphoreGive(candado);
}

String textoTareas() {
  String texto = "";
  int n = 0;
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    const Tarea& t = tareas[i];
    if (!t.activa) continue;
    texto += String(n++ ? "; " : "") + "#" + t.id + " " + formatearMomento(t.momento) + " " + t.accion + " " +
             (strlen(t.objetivo) ? t.objetivo : t.mensaje);
    if (strlen(t.condicion)) texto += String(" (si ") + t.condicion + ")";
  }
  xSemaphoreGive(candado);
  return n == 0 ? String("No hay tareas programadas") : String(n) + " tarea(s): " + texto;
}

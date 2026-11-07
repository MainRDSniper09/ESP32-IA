// =====================================================================
//  avisos.cpp
// =====================================================================
#include "avisos.h"
#include <time.h>

struct Aviso {
  String hora;      // "20:00:05" (o "--:--" si aún no hay hora)
  String texto;
  bool esAlerta;
};

static Aviso avisos[MAX_AVISOS];
static int cantidad = 0;          // cuántos avisos hay guardados
static int pendientes = 0;        // alertas sin revisar
static SemaphoreHandle_t candado; // evita que dos tareas escriban al mismo tiempo

void iniciarAvisos() {
  candado = xSemaphoreCreateMutex();
  pinMode(PIN_LED_ALERTA, OUTPUT);
  digitalWrite(PIN_LED_ALERTA, LOW);
}

static String horaActual() {
  time_t ahora = time(nullptr);
  if (ahora < 1700000000) return "--:--";   // la hora aún no se sincronizó
  struct tm t;
  localtime_r(&ahora, &t);
  char texto[32];
  snprintf(texto, sizeof(texto), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  return texto;
}

void agregarAviso(const String& texto, bool esAlerta) {
  Serial.printf("[%s] %s\n", esAlerta ? "ALERTA" : "aviso", texto.c_str());
  xSemaphoreTake(candado, portMAX_DELAY);
  // Si la lista está llena, se descarta el más antiguo
  if (cantidad == MAX_AVISOS) {
    for (int i = 1; i < MAX_AVISOS; i++) avisos[i - 1] = avisos[i];
    cantidad--;
  }
  avisos[cantidad++] = {horaActual(), texto, esAlerta};
  if (esAlerta) pendientes++;
  xSemaphoreGive(candado);
}

void avisosAJson(JsonArray lista) {
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = cantidad - 1; i >= 0; i--) {   // del más reciente al más antiguo
    JsonObject a = lista.add<JsonObject>();
    a["hora"] = avisos[i].hora;
    a["texto"] = avisos[i].texto;
    a["alerta"] = avisos[i].esAlerta;
  }
  xSemaphoreGive(candado);
}

int alertasPendientes() { return pendientes; }

void marcarAlertasVistas() { pendientes = 0; }

void actualizarLedAlerta() {
  // Parpadeo de 4 veces por segundo usando millis() (sin delay)
  bool encender = pendientes > 0 && (millis() / 125) % 2 == 0;
  digitalWrite(PIN_LED_ALERTA, encender ? HIGH : LOW);
}

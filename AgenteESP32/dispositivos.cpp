// =====================================================================
//  dispositivos.cpp
// =====================================================================
#include "dispositivos.h"
#include "avisos.h"

// ---------------------------------------------------------------------
// LISTA DE DISPOSITIVOS
// Para agregar uno nuevo: define su pin en config.h y agrega una fila aquí.
// El LLM, la validación y la página web lo reconocerán automáticamente.
// ---------------------------------------------------------------------
const Dispositivo DISPOSITIVOS[] = {
  // id                  nombre                      tipo      pin
  {"luz_entrada",        "Luz de la entrada",        LUZ,      PIN_LUZ_ENTRADA},
  {"luz_oficina",        "Luz de la oficina",        LUZ,      PIN_LUZ_OFICINA},
  {"luz_parqueadero",    "Luz del parqueadero",      LUZ,      PIN_LUZ_PARQUEADERO},
  {"luz_bodega",         "Luz de la bodega",         LUZ,      PIN_LUZ_BODEGA},
  {"aspersor_frontal",   "Aspersor frontal",         ASPERSOR, PIN_ASPERSOR_FRONTAL},
  {"aspersor_trasero",   "Aspersor trasero",         ASPERSOR, PIN_ASPERSOR_TRASERO},
  {"puerta_principal",   "Puerta principal",         PUERTA,   PIN_PUERTA_PRINCIPAL},
  {"puerta_bodega",      "Puerta de la bodega",      PUERTA,   PIN_PUERTA_BODEGA},
  {"puerta_oficina",     "Puerta de la oficina",     PUERTA,   PIN_PUERTA_OFICINA},
  {"ventana_oficina",    "Ventana de la oficina",    VENTANA,  PIN_VENTANA_OFICINA},
  {"ventana_recepcion",  "Ventana de la recepción",  VENTANA,  PIN_VENTANA_RECEPCION},
};
const int NUM_DISPOSITIVOS = sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0]);

// Estado actual de cada dispositivo (misma posición que en la lista)
static bool encendido[sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0])];
static bool abierto[sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0])];
static bool lecturaAnterior[sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0])];

// Posición del dispositivo dentro de la lista
static int posicion(const Dispositivo& d) { return &d - DISPOSITIVOS; }

bool esSalida(const Dispositivo& d) { return d.tipo == LUZ || d.tipo == ASPERSOR; }

// Con INPUT_PULLUP el pin lee HIGH cuando el interruptor está abierto.
// Interruptor cerrado (conectado a GND) = puerta CERRADA.
// Interruptor abierto o cable suelto      = puerta ABIERTA (falla segura).
static bool leerPinAbierto(int pin) { return digitalRead(pin) == HIGH; }

void iniciarDispositivos() {
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    if (esSalida(d)) {
      pinMode(d.pin, OUTPUT);
      digitalWrite(d.pin, LOW);          // al arrancar todo queda apagado
      encendido[i] = false;
    } else {
      pinMode(d.pin, INPUT_PULLUP);      // resistencia interna, el interruptor va a GND
      abierto[i] = leerPinAbierto(d.pin);
      lecturaAnterior[i] = abierto[i];
    }
  }
}

const Dispositivo* buscarDispositivo(const String& id) {
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    if (id == DISPOSITIVOS[i].id) return &DISPOSITIVOS[i];
  }
  return nullptr;
}

void cambiarSalida(const Dispositivo& d, bool encender) {
  if (!esSalida(d)) return;              // nunca se escribe en una entrada
  digitalWrite(d.pin, encender ? HIGH : LOW);
  encendido[posicion(d)] = encender;
}

bool estaEncendida(const Dispositivo& d) { return encendido[posicion(d)]; }

bool estaAbierta(const Dispositivo& d) { return abierto[posicion(d)]; }

void revisarEntradas() {
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    if (esSalida(d)) continue;
    bool lectura = leerPinAbierto(d.pin);
    // Antirrebote sencillo: el cambio se acepta solo si dos lecturas
    // seguidas (separadas 50 ms) coinciden.
    if (lectura == lecturaAnterior[i] && lectura != abierto[i]) {
      abierto[i] = lectura;
      agregarAviso(String(d.nombre) + (lectura ? " se abrió" : " se cerró"), false);
    }
    lecturaAnterior[i] = lectura;
  }
}

String listarAccesos(int tipo, bool abiertos) {
  String lista = "";
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    if (esSalida(d)) continue;
    if (tipo != -1 && d.tipo != tipo) continue;
    if (abierto[i] != abiertos) continue;
    if (lista.length() > 0) lista += ", ";
    lista += d.nombre;
  }
  return lista;
}

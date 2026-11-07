// =====================================================================
//  dispositivos.h  —  LUCES, ASPERSORES, PUERTAS Y VENTANAS
// ---------------------------------------------------------------------
//  Contiene la lista de dispositivos y las funciones para:
//    - encender/apagar una salida (LED)
//    - saber si una puerta o ventana está abierta (interruptor)
// =====================================================================
#pragma once
#include "config.h"

enum TipoDispositivo { LUZ, ASPERSOR, PUERTA, VENTANA };

struct Dispositivo {
  const char* id;       // nombre corto que usa el JSON, ej: "luz_entrada"
  const char* nombre;   // nombre para mostrar, ej: "Luz de la entrada"
  TipoDispositivo tipo;
  int pin;
};

// Lista de todos los dispositivos (está en dispositivos.cpp)
extern const Dispositivo DISPOSITIVOS[];
extern const int NUM_DISPOSITIVOS;

void iniciarDispositivos();

// Busca un dispositivo por su id. Devuelve nullptr si no existe.
const Dispositivo* buscarDispositivo(const String& id);

// true si es luz o aspersor (algo que se puede encender/apagar)
bool esSalida(const Dispositivo& d);

// Salidas
void cambiarSalida(const Dispositivo& d, bool encender);
bool estaEncendida(const Dispositivo& d);

// Entradas
bool estaAbierta(const Dispositivo& d);

// Lee los interruptores y registra un aviso cuando alguno cambia.
// Se llama cada 50 ms desde la tarea de monitoreo.
void revisarEntradas();

// Nombres de las puertas/ventanas abiertas (o cerradas), separados por coma.
// tipo: PUERTA, VENTANA, o -1 para ambas.
String listarAccesos(int tipo, bool abiertos);

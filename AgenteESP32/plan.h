// =====================================================================
//  plan.h  —  VALIDAR Y EJECUTAR LO QUE DEVUELVE EL LLM
// ---------------------------------------------------------------------
//  El LLM responde con un "plan" en JSON (ver docs/FORMATO_JSON.md).
//  Este módulo:
//    1. Revisa que el JSON sea correcto (acciones permitidas, dispositivos
//       que existen, horas válidas...). Si hay UN solo error, no se
//       ejecuta NADA.
//    2. Ejecuta las acciones inmediatas y programa las futuras.
//
//  El LLM nunca controla los pines directamente: solo puede elegir
//  acciones de esta lista (el "catálogo de funciones autorizadas"):
//
//    encender · apagar · avisar · consultar_accesos · listar_tareas · cancelar_tarea
// =====================================================================
#pragma once
#include <ArduinoJson.h>

// Valida el plan y, si es correcto, lo ejecuta.
// Escribe el resultado en `respuesta` y devuelve true si el plan era válido.
bool procesarPlan(JsonVariant plan, JsonObject respuesta);

// Ejecuta UNA acción del catálogo (se usa ahora o cuando vence una tarea).
// La condición se evalúa en ese momento leyendo los interruptores reales.
String ejecutarAccion(const String& accion, const String& objetivo, const String& condicion,
                      const String& mensaje);

// true si el texto usa una expresión de tiempo vaga ("más tarde", "en un rato")
// sin decir una hora concreta. En ese caso se pide aclaración.
bool esAmbigua(const String& texto);

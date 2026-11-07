// =====================================================================
//  llm.h  —  CONSULTAR AL MODELO DE LENGUAJE (Claude)
// ---------------------------------------------------------------------
//  Envía la orden del usuario a la API de Claude por HTTPS y recibe un
//  texto que debe contener el plan en JSON.
//
//  No se usa ninguna librería de "agentes": es una petición HTTP normal
//  (HTTPClient) y el JSON se arma/lee con ArduinoJson.
// =====================================================================
#pragma once
#include <Arduino.h>

// Devuelve true si todo salió bien. En `planJson` queda el JSON del plan;
// si falla, en `error` queda la explicación.
bool consultarLLM(const String& orden, String& planJson, String& error);

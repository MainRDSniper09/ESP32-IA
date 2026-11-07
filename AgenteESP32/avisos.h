// =====================================================================
//  avisos.h  —  NOTIFICACIONES Y ALERTAS
// ---------------------------------------------------------------------
//  Guarda los últimos avisos para mostrarlos en la página web.
//  Las ALERTAS además hacen parpadear el LED rojo hasta que el usuario
//  las marca como vistas.
// =====================================================================
#pragma once
#include <ArduinoJson.h>
#include "config.h"

void iniciarAvisos();

// esAlerta = true  -> alerta importante (LED rojo parpadea)
// esAlerta = false -> simple información (cambios de puertas, etc.)
void agregarAviso(const String& texto, bool esAlerta);

// Copia los avisos a un arreglo JSON (para /api/estado)
void avisosAJson(JsonArray lista);

int alertasPendientes();
void marcarAlertasVistas();

// Hace parpadear el LED de alerta si hay alertas pendientes.
void actualizarLedAlerta();

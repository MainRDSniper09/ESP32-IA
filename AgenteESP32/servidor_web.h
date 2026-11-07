// =====================================================================
//  servidor_web.h  —  PÁGINA WEB Y API
// ---------------------------------------------------------------------
//  Rutas (detalle en docs/API.md):
//    GET  /                    página web
//    GET  /api/estado          dispositivos, tareas y avisos
//    POST /api/orden           orden en lenguaje natural  {"texto": "..."}
//    POST /api/plan            ejecutar un plan JSON directo (sin LLM)
//    POST /api/cancelar        cancelar una tarea         {"id": 3}
//    POST /api/alertas/vistas  apagar el LED de alerta
// =====================================================================
#pragma once

void iniciarServidor();
void atenderServidor();   // llamar continuamente en loop()

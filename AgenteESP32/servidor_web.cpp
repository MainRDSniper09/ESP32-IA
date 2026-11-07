// =====================================================================
//  servidor_web.cpp
// =====================================================================
#include "servidor_web.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include "avisos.h"
#include "dispositivos.h"
#include "llm.h"
#include "pagina_web.h"
#include "plan.h"
#include "tareas.h"

static WebServer servidor(80);

// Envía un documento JSON como respuesta
static void responder(int codigo, JsonDocument& doc) {
  String texto;
  serializeJson(doc, texto);
  servidor.send(codigo, "application/json; charset=utf-8", texto);
}

static void responderError(int codigo, const String& mensaje) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["mensaje"] = mensaje;
  responder(codigo, doc);
}

// Lee el cuerpo JSON de la petición. Si está mal, responde 400 y devuelve false.
static bool leerCuerpo(JsonDocument& doc) {
  if (deserializeJson(doc, servidor.arg("plain"))) {
    responderError(400, "El cuerpo de la petición no es un JSON válido");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------- GET /
static void paginaPrincipal() { servidor.send_P(200, "text/html; charset=utf-8", PAGINA_WEB); }

// ------------------------------------------------------- GET /api/estado
static void estado() {
  JsonDocument doc;
  doc["hora"] = horaSincronizada() ? formatearMomento(time(nullptr)) : String("sin sincronizar");
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("sin conexión (modo respaldo)");
  doc["alertas_pendientes"] = alertasPendientes();

  JsonArray lista = doc["dispositivos"].to<JsonArray>();
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    JsonObject o = lista.add<JsonObject>();
    o["id"] = d.id;
    o["nombre"] = d.nombre;
    o["pin"] = d.pin;
    if (esSalida(d)) {
      o["tipo"] = d.tipo == LUZ ? "luz" : "aspersor";
      o["encendido"] = estaEncendida(d);
    } else {
      o["tipo"] = d.tipo == PUERTA ? "puerta" : "ventana";
      o["abierto"] = estaAbierta(d);
    }
  }
  tareasAJson(doc["tareas"].to<JsonArray>());
  avisosAJson(doc["avisos"].to<JsonArray>());
  responder(200, doc);
}

// ------------------------------------------------------- POST /api/orden
static void orden() {
  JsonDocument cuerpo;
  if (!leerCuerpo(cuerpo)) return;
  String texto = cuerpo["texto"] | "";
  texto.trim();
  if (texto.length() == 0) return responderError(400, "Falta el campo 'texto'");
  if (texto.length() > 400) return responderError(400, "La orden es demasiado larga (máx. 400 caracteres)");

  JsonDocument respuesta;
  respuesta["orden"] = texto;

  // 1) Ambigüedad: se pregunta ANTES de gastar una consulta al LLM
  if (esAmbigua(texto)) {
    respuesta["ok"] = true;
    respuesta["tipo"] = "aclaracion";
    respuesta["pregunta"] = "Tu orden no dice una hora exacta. ¿A qué hora (por ejemplo 20:30) o en cuántos minutos?";
    return responder(200, respuesta);
  }

  // 2) Sin internet no hay LLM
  if (WiFi.status() != WL_CONNECTED) return responderError(503, "Sin conexión a internet: no se puede consultar el LLM");

  // 3) Consultar al LLM
  String planTexto, error;
  if (!consultarLLM(texto, planTexto, error)) {
    agregarAviso("Falló la consulta al LLM: " + error, true);
    return responderError(502, error + ". No se ejecutó nada.");
  }
  respuesta["json_del_llm"] = planTexto;   // se muestra para que se vea qué respondió el modelo

  // 4) Validar y ejecutar el JSON
  JsonDocument plan;
  if (deserializeJson(plan, planTexto)) {
    respuesta["ok"] = false;
    respuesta["tipo"] = "rechazado";
    respuesta["mensaje"] = "El LLM devolvió un JSON mal formado. No se ejecutó nada.";
    return responder(422, respuesta);
  }
  bool valido = procesarPlan(plan.as<JsonVariant>(), respuesta.as<JsonObject>());
  responder(valido ? 200 : 422, respuesta);
}

// -------------------------------------------------------- POST /api/plan
// Permite probar el sistema SIN LLM, enviando el JSON directamente.
static void planDirecto() {
  JsonDocument plan;
  if (!leerCuerpo(plan)) return;
  JsonDocument respuesta;
  bool valido = procesarPlan(plan.as<JsonVariant>(), respuesta.to<JsonObject>());
  responder(valido ? 200 : 422, respuesta);
}

// ---------------------------------------------------- POST /api/cancelar
static void cancelar() {
  JsonDocument cuerpo;
  if (!leerCuerpo(cuerpo)) return;
  int id = cuerpo["id"] | 0;
  if (!cancelarTarea(id)) return responderError(404, "No existe la tarea #" + String(id));
  JsonDocument doc;
  doc["ok"] = true;
  doc["mensaje"] = "Tarea #" + String(id) + " cancelada";
  responder(200, doc);
}

// ---------------------------------------------- POST /api/alertas/vistas
static void alertasVistas() {
  marcarAlertasVistas();
  JsonDocument doc;
  doc["ok"] = true;
  responder(200, doc);
}

void iniciarServidor() {
  servidor.on("/", HTTP_GET, paginaPrincipal);
  servidor.on("/api/estado", HTTP_GET, estado);
  servidor.on("/api/orden", HTTP_POST, orden);
  servidor.on("/api/plan", HTTP_POST, planDirecto);
  servidor.on("/api/cancelar", HTTP_POST, cancelar);
  servidor.on("/api/alertas/vistas", HTTP_POST, alertasVistas);
  servidor.onNotFound([]() { responderError(404, "Ruta no encontrada"); });
  servidor.begin();
}

void atenderServidor() { servidor.handleClient(); }

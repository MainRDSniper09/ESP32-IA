// =====================================================================
//  llm.cpp
// =====================================================================
#include "llm.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "credenciales.h"
#include "dispositivos.h"
#include "tareas.h"

// ---------------------------------------------------------------------
// INSTRUCCIONES PARA EL LLM ("prompt")
// Le explican qué formato debe devolver y qué dispositivos existen.
// La lista de dispositivos se arma automáticamente desde dispositivos.cpp.
// ---------------------------------------------------------------------
static String crearInstrucciones() {
  String s =
    "Eres el asistente de automatización de una pequeña empresa. Conviertes órdenes en español "
    "en un plan JSON que un microcontrolador ESP32 valida y ejecuta. No conoces el estado real de "
    "puertas ni luces: el microcontrolador lo revisa con sus sensores.\n\n"
    "RESPONDE SOLO CON UN OBJETO JSON, sin texto adicional y sin ```.\n\n"
    "FORMATO:\n"
    "{\"tipo\": \"plan\" | \"aclaracion\" | \"no_soportado\", \"mensaje\": \"...\", \"acciones\": [ ... ]}\n"
    "- plan: incluye \"acciones\" y en \"mensaje\" un resumen corto.\n"
    "- aclaracion: si la orden es ambigua (\"más tarde\", \"en un rato\", sin hora clara, o no se sabe qué dispositivo), "
    "pon en \"mensaje\" una pregunta concreta. Sin acciones. NUNCA inventes una hora.\n"
    "- no_soportado: la orden no se puede hacer con las acciones disponibles; explica en \"mensaje\". Sin acciones.\n\n"
    "Cada acción es un objeto con estos campos (usa solo los necesarios):\n"
    "  \"accion\": encender | apagar | avisar | consultar_accesos | listar_tareas | cancelar_tarea\n"
    "  \"dispositivo\": id del dispositivo, o grupo: luces | aspersores | todo  (para encender/apagar)\n"
    "                  puertas | ventanas | todo  (para consultar_accesos)\n"
    "  \"hora\": \"HH:MM\" en formato 24 h (solo encender/apagar/avisar). Si no se indica, es inmediato.\n"
    "  \"minutos_despues\": número entero de minutos a esperar (después de \"hora\", o desde ahora si no hay hora)\n"
    "  \"condicion\": puertas_cerradas | alguna_puerta_abierta | ventanas_cerradas | alguna_ventana_abierta\n"
    "  \"mensaje\": texto del aviso (solo para avisar, máximo 100 caracteres)\n"
    "  \"id\": número de la tarea (solo para cancelar_tarea)\n\n"
    "DISPOSITIVOS:\n";
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    s += String("- ") + d.id + " (" + d.nombre + ")" + (esSalida(d) ? "" : " [solo se puede consultar]") + "\n";
  }
  s +=
    "\nREGLAS: \"8:00 p. m.\" = \"20:00\". \"Avísame si...\" = accion avisar con condicion. "
    "Una acción por dispositivo o grupo. Las puertas y ventanas NO se pueden abrir ni cerrar.\n\n"
    "EJEMPLOS:\n"
    "Orden: A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta.\n"
    "{\"tipo\":\"plan\",\"mensaje\":\"A las 20:00: luz de entrada, aspersores y aviso si hay puertas abiertas\",\"acciones\":["
    "{\"accion\":\"encender\",\"dispositivo\":\"luz_entrada\",\"hora\":\"20:00\"},"
    "{\"accion\":\"encender\",\"dispositivo\":\"aspersores\",\"hora\":\"20:00\"},"
    "{\"accion\":\"avisar\",\"mensaje\":\"Hay puertas abiertas\",\"hora\":\"20:00\",\"condicion\":\"alguna_puerta_abierta\"}]}\n"
    "Orden: Revisa las puertas y, si todas están cerradas, apaga todas las luces.\n"
    "{\"tipo\":\"plan\",\"mensaje\":\"Revisar puertas y apagar luces si están cerradas\",\"acciones\":["
    "{\"accion\":\"consultar_accesos\",\"dispositivo\":\"puertas\"},"
    "{\"accion\":\"apagar\",\"dispositivo\":\"luces\",\"condicion\":\"puertas_cerradas\"}]}\n"
    "Orden: Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero.\n"
    "{\"tipo\":\"plan\",\"mensaje\":\"18:00 oficina y 18:15 parqueadero\",\"acciones\":["
    "{\"accion\":\"encender\",\"dispositivo\":\"luz_oficina\",\"hora\":\"18:00\"},"
    "{\"accion\":\"encender\",\"dispositivo\":\"luz_parqueadero\",\"hora\":\"18:00\",\"minutos_despues\":15}]}\n"
    "Orden: ¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?\n"
    "{\"tipo\":\"plan\",\"mensaje\":\"Consultar accesos y tareas\",\"acciones\":["
    "{\"accion\":\"consultar_accesos\",\"dispositivo\":\"todo\"},{\"accion\":\"listar_tareas\"}]}\n"
    "Orden: Enciende los aspersores más tarde.\n"
    "{\"tipo\":\"aclaracion\",\"mensaje\":\"¿A qué hora exacta quieres encender los aspersores?\"}\n";
  return s;
}

bool consultarLLM(const String& orden, String& planJson, String& error) {
  if (String(LLM_API_KEY).length() < 10) {
    error = "No hay clave de la API del LLM en credenciales.h";
    return false;
  }

  // La hora actual ayuda al LLM a entender "mañana", "esta noche", etc.
  String hora = "desconocida";
  if (horaSincronizada()) hora = formatearMomento(time(nullptr));

  // ---- 1. Armar la petición (formato de la API Messages de Claude) ----
  JsonDocument peticion;
  peticion["model"] = MODELO_LLM;
  peticion["max_tokens"] = 4096;
  peticion["system"] = crearInstrucciones();
  peticion["output_config"]["effort"] = "low";   // respuesta rápida: la tarea es sencilla
  peticion["fallbacks"] = "default";             // si el modelo rechaza la orden, el servidor reintenta con otro
  JsonObject msg = peticion["messages"].to<JsonArray>().add<JsonObject>();
  msg["role"] = "user";
  msg["content"] = "Fecha y hora actual (día/mes hora): " + hora + "\nOrden: " + orden;
  String cuerpo;
  serializeJson(peticion, cuerpo);

  // ---- 2. Enviarla por HTTPS ------------------------------------------
  WiFiClientSecure cliente;
  cliente.setInsecure();   // conexión cifrada, sin verificar el certificado (simplificación del prototipo)
  HTTPClient http;
  http.begin(cliente, "https://api.anthropic.com/v1/messages");
  http.setTimeout(30000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", LLM_API_KEY);
  http.addHeader("anthropic-version", "2026-10-21");
  http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");

  Serial.println("[llm] Consultando al modelo...");
  int codigo = http.POST(cuerpo);
  if (codigo <= 0) {
    error = "No se pudo conectar con la API del LLM (" + http.errorToString(codigo) + ")";
    http.end();
    return false;
  }
  String texto = http.getString();
  http.end();

  // ---- 3. Leer la respuesta -------------------------------------------
  JsonDocument respuesta;
  if (deserializeJson(respuesta, texto)) {
    error = "La API respondió algo que no es JSON (HTTP " + String(codigo) + ")";
    return false;
  }
  if (codigo != 200) {
    error = "La API respondió con error HTTP " + String(codigo) + ": " + (respuesta["error"]["message"] | "");
    return false;
  }
  String motivo = respuesta["stop_reason"] | "";
  if (motivo == "refusal") {
    error = "El modelo no quiso procesar esa orden";
    return false;
  }

  // El texto del modelo viene en los bloques de tipo "text"
  String salida = "";
  for (JsonObject bloque : respuesta["content"].as<JsonArray>()) {
    if (bloque["type"] == "text") salida += bloque["text"].as<const char*>();
  }

  // Quedarse solo con lo que está entre la primera "{" y la última "}"
  int inicio = salida.indexOf('{');
  int fin = salida.lastIndexOf('}');
  if (inicio < 0 || fin <= inicio) {
    error = "El LLM no devolvió un JSON: " + salida.substring(0, 120);
    return false;
  }
  planJson = salida.substring(inicio, fin + 1);
  Serial.println("[llm] JSON recibido: " + planJson);
  return true;
}

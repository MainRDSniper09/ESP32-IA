/**
 * @file WebApi.h
 * @brief Servidor HTTP del ESP32-S3: interfaz web + API REST JSON.
 *
 * La referencia completa (rutas, cuerpos, respuestas, códigos de error y
 * ejemplos con curl) está en docs/API.md.
 *
 *   GET    /                      Interfaz web
 *   GET    /api/health            Vida del servicio (sin autenticación)
 *   GET    /api/status            Estado general
 *   GET    /api/catalog           Acciones, dispositivos y grupos permitidos
 *   GET    /api/devices           Salidas y entradas con su estado
 *   POST   /api/devices/set       Control manual de una salida
 *   GET    /api/accesses          Puertas/ventanas (?target=&filter=)
 *   POST   /api/command           Instrucción en lenguaje natural (asíncrona, 202)
 *   GET    /api/jobs?id=N         Resultado de una instrucción
 *   POST   /api/plan              Ejecutar un plan JSON directamente (sin LLM)
 *   POST   /api/plan/validate     Validar un plan sin ejecutarlo
 *   GET    /api/tasks             Tareas programadas
 *   DELETE /api/tasks?id=N|all=1  Cancelar tareas
 *   POST   /api/tasks/cancel      Cancelar tareas (alternativa a DELETE)
 *   GET    /api/events?since=N    Eventos y alertas
 *   POST   /api/events/ack        Reconocer alertas (apaga el LED de alerta)
 */
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

namespace agent {

class Agent;
class JobManager;
class LlmClient;
class NetworkManager;
class IClock;
class TelegramNotifier;

class WebApi {
 public:
  WebApi(Agent& agent, JobManager& jobs, LlmClient& llm, NetworkManager& net, IClock& clock,
         TelegramNotifier& telegram)
      : server_(80), agent_(agent), jobs_(jobs), llm_(llm), net_(net), clock_(clock), telegram_(telegram) {}

  void begin();
  /** Atiende clientes. Llamar continuamente desde loop(). */
  void handle() { server_.handleClient(); }

 private:
  // Infraestructura
  bool authorize();
  bool parseBody(JsonDocument& doc);
  void sendJson(int code, JsonDocument& doc);
  void sendError(int code, const char* errCode, const String& message);
  void addCors();

  // Rutas
  void handleRoot();
  void handleHealth();
  void handleStatus();
  void handleCatalog();
  void handleDevices();
  void handleDeviceSet();
  void handleAccesses();
  void handleCommand();
  void handleJob();
  void handlePlan(bool dryRun);
  void handleTasks();
  void handleTasksDelete();
  void handleTasksCancel();
  void handleEvents();
  void handleEventsAck();
  void handleNotFound();

  void cancelFromArgs(bool all, long id);

  WebServer server_;
  Agent& agent_;
  JobManager& jobs_;
  LlmClient& llm_;
  NetworkManager& net_;
  IClock& clock_;
  TelegramNotifier& telegram_;
};

}  // namespace agent

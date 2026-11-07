/**
 * ============================================================================
 *  Agente inteligente de automatización y supervisión empresarial
 *  ESP32-S3 · C++ (Arduino) · LLM remoto vía HTTPS
 * ============================================================================
 *
 *  El LLM interpreta la orden en lenguaje natural y devuelve un plan JSON.
 *  El firmware VALIDA ese plan contra un catálogo explícito de funciones,
 *  lo PLANIFICA (inmediato / a una hora / secuencia temporizada), lo GUARDA
 *  en memoria no volátil y lo EJECUTA sobre LED (luces y aspersores),
 *  evaluando las condiciones con los interruptores (puertas y ventanas).
 *
 *  Arquitectura de tareas (FreeRTOS):
 *
 *   Núcleo 1                                  Núcleo 0
 *   ┌───────────────────────────┐             ┌────────────────────────────┐
 *   │ ioTask (prioridad 3)      │             │ net (prioridad 1)          │
 *   │  - antirrebote entradas   │             │  - cola de instrucciones   │
 *   │    cada 10 ms             │             │  - HTTPS al LLM (bloquea)  │
 *   │  - agent.tick() 250 ms    │◄── mutex ──►│  - validación + ejecución  │
 *   │  - LED alerta / zumbador  │    Agent    │  - envío a Telegram        │
 *   └───────────────────────────┘             └────────────────────────────┘
 *   ┌───────────────────────────┐
 *   │ loop() (prioridad 1)      │
 *   │  - servidor web / API     │
 *   │  - supervisión Wi-Fi      │
 *   └───────────────────────────┘
 *
 *  Ninguna tarea usa delay() largos: el monitoreo y el planificador nunca
 *  se detienen mientras se espera al LLM o a una tarea programada.
 *
 *  Documentación: README.md y carpeta docs/ del repositorio.
 *  Licencia: MIT.
 * ============================================================================
 */

#include <Arduino.h>

#include "src/config/AppConfig.h"
#include "src/config/Secrets.h"
#include "src/core/Agent.h"
#include "src/core/PlanSerializer.h"
#include "src/core/TimeUtils.h"
#include "src/llm/LlmClient.h"
#include "src/platform/Esp32Hardware.h"
#include "src/platform/Esp32Services.h"
#include "src/platform/JobManager.h"
#include "src/platform/NetworkManager.h"
#include "src/platform/Notifiers.h"
#include "src/web/WebApi.h"

using namespace agent;

// Pila de loop() (servidor web): 16 KB para construir respuestas JSON con holgura.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// ------------------------------------------------------------- Componentes
static RtosLock gLock;
static Esp32Hardware gHardware;
static Esp32Clock gClock;
static NvsTaskStore gStore;
static LocalIndicator gIndicator;
static TelegramNotifier gTelegram;
static NetworkManager gNetwork;
static LlmClient gLlm;
static Agent gAgent(gHardware, gClock, gStore, &gLock);
static JobManager gJobs(gAgent, gLlm, gClock, gTelegram);
static WebApi gWeb(gAgent, gJobs, gLlm, gNetwork, gClock, gTelegram);

// ------------------------------------------------------- Callbacks / tareas

/** Cambio estable en una puerta/ventana (llamado desde ioTask). */
static void onAccessChanged(const DeviceInfo& dev, bool open, void*) { gAgent.onAccessChanged(dev, open); }

/** Eventos de red -> registro de eventos. */
static void onNetworkEvent(bool warning, const char* message) {
  gAgent.logEvent(warning ? EventLevel::Warning : EventLevel::Info, "network", message);
}

/** Cada evento nuevo: consola serie, zumbador y Telegram (si es alerta). */
static void onEvent(const Event& e) {
  Serial.printf("[evento #%lu] %-7s %-8s %s\n", static_cast<unsigned long>(e.id), serializer::levelName(e.level),
                e.source.c_str(), e.message.c_str());
  if (e.level == EventLevel::Alert) gIndicator.beep();
  if (e.level != EventLevel::Info) gTelegram.enqueue(e.message.c_str());
}

/**
 * Tarea de E/S en tiempo real. Nunca hace llamadas de red ni esperas largas.
 */
static void ioTask(void*) {
  uint32_t lastTick = 0;
  bool wasSynced = false;
  uint32_t unacked = 0;
  bool networkOk = false;
  for (;;) {
    uint32_t now = millis();
    gHardware.poll(now, onAccessChanged, nullptr);  // antirrebote de puertas/ventanas

    if (now - lastTick >= AGENT_SCHEDULER_TICK_MS) {
      lastTick = now;
      bool synced = gClock.isSynced();
      if (synced && !wasSynced) {
        gAgent.logEvent(EventLevel::Info, "time",
                        "Hora sincronizada por NTP: " + timeutil::toIso8601(gClock.now()));
      }
      wasSynced = synced;
      gAgent.tick();  // ejecuta tareas vencidas (no bloquea)
      unacked = gAgent.unacknowledgedAlerts();
      networkOk = gNetwork.staConnected();
    }
    gIndicator.update(now, networkOk, unacked);  // LED de alerta, LED de estado y zumbador
    vTaskDelay(pdMS_TO_TICKS(AGENT_INPUT_POLL_MS));
  }
}

// ------------------------------------------------------------------ setup

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("=============================================="));
  Serial.printf("  Agente ESP32-S3 v%s\n", AGENT_FIRMWARE_VERSION);
  Serial.println(F("=============================================="));
#if AGENT_USING_SECRET_TEMPLATE
  Serial.println(F("[!] No existe src/config/agente_secrets.h: se usa la plantilla (sin Wi-Fi ni LLM)."));
#endif

  gLock.begin();
  gAgent.events().setListener(onEvent);

  if (!gStore.begin()) Serial.println(F("[nvs] ERROR al abrir la memoria no volátil"));
  gHardware.begin();  // salidas apagadas, entradas con pull-up
  gIndicator.begin();
  gTelegram.begin();
  gAgent.begin();  // recupera tareas guardadas (se ejecutarán cuando haya hora válida)

  // El monitoreo arranca ANTES de conectarse a la red.
  xTaskCreatePinnedToCore(ioTask, "io", 8192, nullptr, 3, nullptr, 1);

  gClock.begin(AGENT_TIMEZONE, AGENT_NTP_SERVER_1, AGENT_NTP_SERVER_2, AGENT_NTP_SERVER_3);
  gNetwork.begin(onNetworkEvent);  // espera hasta 20 s por la red; si no, abre el AP de respaldo
  gJobs.begin();
  gWeb.begin();

  gAgent.logEvent(EventLevel::Info, "system", std::string("Sistema iniciado (firmware ") + AGENT_FIRMWARE_VERSION + ")");
  if (!gLlm.configured()) {
    gAgent.logEvent(EventLevel::Warning, "llm",
                    "LLM sin configurar: use control manual o POST /api/plan con JSON directo");
  }
}

// ------------------------------------------------------------------- loop

void loop() {
  gWeb.handle();
  gNetwork.loop();
  delay(2);  // cede la CPU a otras tareas del mismo núcleo
}

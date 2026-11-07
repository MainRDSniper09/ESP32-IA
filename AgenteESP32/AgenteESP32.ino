// =====================================================================
//  AGENTE INTELIGENTE DE AUTOMATIZACIÓN Y SUPERVISIÓN EMPRESARIAL
//  ESP32-S3 · C++ (Arduino) · LLM remoto (Claude)
// ---------------------------------------------------------------------
//  ¿Qué hace?
//    1. El usuario escribe una orden en la página web, por ejemplo:
//       "A las 8 p. m. enciende la luz de la entrada".
//    2. El ESP32 se la envía al LLM, que responde con un JSON.
//    3. El ESP32 revisa ese JSON (que sea válido y seguro) y:
//         - ejecuta las acciones inmediatas (encender/apagar LED), o
//         - las guarda como tareas para la hora indicada.
//    4. Las puertas y ventanas (interruptores) se vigilan todo el tiempo.
//
//  Archivos del proyecto:
//    config.h          pines, zona horaria y límites
//    credenciales.h    claves de Wi-Fi y del LLM (NO se sube a GitHub)
//    dispositivos.*    LED e interruptores
//    plan.*            validar y ejecutar el JSON (catálogo de acciones)
//    tareas.*          tareas programadas + memoria no volátil
//    avisos.*          notificaciones y LED de alerta
//    llm.*             consulta a la API de Claude
//    servidor_web.*    página web y API
//    pagina_web.h      HTML de la interfaz
//
//  Librería necesaria: ArduinoJson (de Benoit Blanchon), versión 7.
// =====================================================================

#include <WiFi.h>
#include "config.h"
#include "credenciales.h"
#include "avisos.h"
#include "dispositivos.h"
#include "tareas.h"
#include "servidor_web.h"

// ---------------------------------------------------------------------
// TAREA DE MONITOREO
// El ESP32-S3 tiene dos núcleos. Esta función corre aparte (núcleo 1)
// en un ciclo infinito. Así, aunque el servidor web esté esperando la
// respuesta del LLM (varios segundos), las puertas se siguen vigilando
// y las tareas programadas se ejecutan a tiempo.
// ---------------------------------------------------------------------
void tareaMonitoreo(void* parametro) {
  unsigned long ultimaRevision = 0;
  while (true) {
    revisarEntradas();          // ¿se abrió o cerró alguna puerta/ventana?
    actualizarLedAlerta();      // parpadeo del LED rojo

    if (millis() - ultimaRevision >= 1000) {   // una vez por segundo
      ultimaRevision = millis();
      revisarTareas();          // ¿ya es hora de alguna tarea?
    }
    vTaskDelay(pdMS_TO_TICKS(50));   // espera 50 ms sin bloquear al resto del sistema
  }
}

// ---------------------------------------------------------------------
// CONEXIÓN WI-FI
// Intenta conectarse durante 15 s. Si no puede, crea su propia red
// ("Agente-ESP32") para que la página web siga disponible.
// ---------------------------------------------------------------------
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  Serial.print("Conectando a Wi-Fi");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado. Abre en el navegador: http://" + WiFi.localIP().toString());
    agregarAviso("Wi-Fi conectado (" + WiFi.localIP().toString() + ")", false);
  } else {
    WiFi.mode(WIFI_AP_STA);   // red propia + sigue intentando la red normal
    WiFi.softAP(RED_RESPALDO, CLAVE_RESPALDO);
    Serial.println("Sin Wi-Fi. Conéctate a la red '" RED_RESPALDO "' y abre http://192.168.4.1");
    agregarAviso("Sin Wi-Fi: red de respaldo activa. El LLM no está disponible.", true);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Agente ESP32-S3 ===");

  iniciarAvisos();
  iniciarDispositivos();   // salidas apagadas, entradas listas
  iniciarTareas();         // recupera las tareas guardadas antes de un reinicio

  // Arranca el monitoreo en el núcleo 1 (antes de conectarse a internet)
  xTaskCreatePinnedToCore(tareaMonitoreo, "monitoreo", 8192, nullptr, 2, nullptr, 1);

  conectarWiFi();
  configTzTime(ZONA_HORARIA, SERVIDOR_NTP);   // pide la hora a internet (NTP)
  iniciarServidor();
}

void loop() {
  atenderServidor();   // responde a la página web y a la API

  // Aviso si se pierde (o vuelve) la conexión Wi-Fi
  static bool conectadoAntes = WiFi.status() == WL_CONNECTED;
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado != conectadoAntes) {
    agregarAviso(conectado ? "Wi-Fi reconectado" : "Se perdió el Wi-Fi: las tareas siguen funcionando, el LLM no", !conectado);
    conectadoAntes = conectado;
  }
  delay(2);
}

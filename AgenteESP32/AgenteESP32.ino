/*
 * =============================================================================
 *   AGENTE INTELIGENTE DE AUTOMATIZACIÓN Y SUPERVISIÓN EMPRESARIAL
 *   ESP32-S3  ·  C++ (Arduino IDE)  ·  LLM remoto (Gemini gratis o Claude)
 * =============================================================================
 *
 *  ¿QUÉ HACE?
 *  ----------
 *  1. El usuario escribe una orden en español en una página web que muestra
 *     el propio ESP32. Ej: "A las 8 p. m. enciende la luz de la entrada".
 *  2. El ESP32 envía la orden al LLM (Gemini o Claude) por internet.
 *  3. El LLM responde con un PLAN en formato JSON.
 *  4. El ESP32 REVISA ese JSON (acciones permitidas, dispositivos que existen,
 *     horas válidas...). Si algo está mal, no hace NADA.
 *  5. Si está bien: ejecuta ya lo inmediato y guarda lo programado como tareas.
 *  6. Mientras tanto vigila puertas y ventanas todo el tiempo.
 *
 *  IDEA CLAVE: el LLM solo INTERPRETA el texto. El ESP32 es quien DECIDE y
 *  ACTÚA, y solo acepta 6 acciones de una lista permitida (sección 7).
 *
 *  CONEXIONES (ver también docs/diagrama_conexiones.svg)
 *  -----------------------------------------------------
 *  SALIDAS (LED que simulan luces y aspersores):
 *     GPIO ──[ resistencia 220 Ω ]──►|── GND
 *                              pata larga  pata corta
 *
 *        GPIO 4  ── Luz de la entrada      (LED amarillo)
 *        GPIO 5  ── Luz de la oficina      (LED amarillo)
 *        GPIO 6  ── Luz del parqueadero    (LED amarillo)
 *        GPIO 7  ── Luz de la bodega       (LED amarillo)
 *        GPIO 15 ── Aspersor frontal       (LED azul)
 *        GPIO 16 ── Aspersor trasero       (LED azul)
 *        GPIO 17 ── LED de ALERTA          (LED rojo)
 *
 *  ENTRADAS (interruptores que simulan puertas y ventanas):
 *     GPIO ──── interruptor ──── GND      (sin resistencia: se usa la interna)
 *
 *        GPIO 8  ── Puerta principal
 *        GPIO 9  ── Puerta de la bodega
 *        GPIO 10 ── Puerta de la oficina
 *        GPIO 11 ── Ventana de la oficina
 *        GPIO 12 ── Ventana de la recepción
 *
 *     Interruptor CERRADO (pin unido a GND) = puerta CERRADA
 *     Interruptor ABIERTO                    = puerta ABIERTA
 *
 *  QUÉ NECESITAS EN EL ARDUINO IDE
 *  -------------------------------
 *   - Placa: "esp32" de Espressif (probado con la versión 2.0.17)
 *     -> Herramientas > Placa > ESP32S3 Dev Module
 *   - Librería: "ArduinoJson" de Benoit Blanchon (versión 7)
 *   - Monitor Serie a 115200 baudios
 *
 *  SECCIONES DE ESTE ARCHIVO
 *  -------------------------
 *    1. Claves (Wi-Fi y API)            8. Validar el JSON del LLM
 *    2. Configuración y pines           9. Consultar al LLM
 *    3. Librerías                      10. Página web (HTML)
 *    4. Dispositivos (LED/interruptor) 11. Servidor web y API
 *    5. Avisos y alertas               12. Tarea de monitoreo
 *    6. Tareas programadas             13. Wi-Fi, setup() y loop()
 *    7. Catálogo de acciones permitidas
 *
 *  Licencia MIT · Juan Nicolás Barreto Sánchez · 2026
 * =============================================================================
 */


// =============================================================================
// 1. CLAVES (Wi-Fi y API del LLM)
// -----------------------------------------------------------------------------
//  ¡CUIDADO! Este proyecto es público. NO subas tus claves reales a GitHub.
//
//  Forma recomendada: crea en esta misma carpeta un archivo llamado
//  "credenciales.h" (está en .gitignore, nunca se sube) con este contenido:
//
//      #define WIFI_NOMBRE  "MiRed"
//      #define WIFI_CLAVE   "MiClave"
//      #define LLM_API_KEY  "AIza..."     (clave de Gemini, o "sk-ant-..." de Claude)
//
//  Si ese archivo no existe, se usan los valores de abajo.
// =============================================================================
#if __has_include("credenciales.h")
  #include "credenciales.h"
#else
  #define WIFI_NOMBRE  "NombreDeTuRed"
  #define WIFI_CLAVE   "ClaveDeTuRed"
  #define LLM_API_KEY  ""          // clave de la API (ver sección 2: qué proveedor usar)
#endif


// =============================================================================
// 2. CONFIGURACIÓN Y PINES
// =============================================================================

// Hora local (formato POSIX). "<-05>5" = Colombia (UTC-5).
// Otros: México "CST6", Argentina "<-03>3", España "CET-1CEST,M3.5.0,M10.5.0/3"
#define ZONA_HORARIA  "<-05>5"
#define SERVIDOR_NTP  "pool.ntp.org"

// Si no hay Wi-Fi, el ESP32 crea su propia red con estos datos.
#define RED_RESPALDO    "Agente-ESP32"
#define CLAVE_RESPALDO  "agente1234"    // mínimo 8 caracteres

// ¿Qué modelo de lenguaje (LLM) interpreta las órdenes?
//   1 = Gemini de Google  -> GRATIS. Clave en https://aistudio.google.com/apikey (empieza por "AIza")
//   2 = Claude de Anthropic -> de pago. Clave en https://console.anthropic.com (empieza por "sk-ant-")
#define PROVEEDOR_LLM  1

#if PROVEEDOR_LLM == 1
  #define MODELO_LLM  "gemini-3.8-flash"
  #define URL_LLM     "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions"
#else
  #define MODELO_LLM  "claude-opus-5-5"
  #define URL_LLM     "https://api.anthropic.com/v1/messages"
#endif

// Pines de las salidas (LED)
#define PIN_LUZ_ENTRADA       4
#define PIN_LUZ_OFICINA       5
#define PIN_LUZ_PARQUEADERO   6
#define PIN_LUZ_BODEGA        7
#define PIN_ASPERSOR_FRONTAL  15
#define PIN_ASPERSOR_TRASERO  16
#define PIN_LED_ALERTA        17

// Pines de las entradas (interruptores)
#define PIN_PUERTA_PRINCIPAL  8
#define PIN_PUERTA_BODEGA     9
#define PIN_PUERTA_OFICINA    10
#define PIN_VENTANA_OFICINA   11
#define PIN_VENTANA_RECEPCION 12

// Límites
#define MAX_TAREAS          20   // tareas programadas que caben en memoria
#define MAX_ACCIONES        6    // acciones máximas en una orden
#define MAX_AVISOS          20   // avisos que muestra la página
#define MINUTOS_TOLERANCIA  10   // si una tarea "se pasó" más que esto (equipo apagado), no se ejecuta


// =============================================================================
// 3. LIBRERÍAS
// -----------------------------------------------------------------------------
//  Todas vienen con la placa ESP32, excepto ArduinoJson (instalar aparte).
//  No se usa ningún framework de "agentes": solo HTTP y JSON.
// =============================================================================
#include <WiFi.h>              // conexión Wi-Fi
#include <WebServer.h>         // página web y API
#include <HTTPClient.h>        // peticiones HTTP al LLM
#include <WiFiClientSecure.h>  // HTTPS (conexión cifrada)
#include <Preferences.h>       // memoria no volátil (no se borra al apagar)
#include <ArduinoJson.h>       // leer y escribir JSON
#include <time.h>              // hora y fecha


// =============================================================================
// 4. DISPOSITIVOS (luces, aspersores, puertas y ventanas)
// =============================================================================

enum TipoDispositivo { LUZ, ASPERSOR, PUERTA, VENTANA };

struct Dispositivo {
  const char* id;        // nombre que usa el JSON, ej: "luz_entrada"
  const char* nombre;    // nombre para mostrar, ej: "Luz de la entrada"
  TipoDispositivo tipo;
  int pin;
};

// Lista de dispositivos. Para agregar uno: escribe una fila nueva.
// El LLM, la validación y la página web lo reconocerán automáticamente.
const Dispositivo DISPOSITIVOS[] = {
  // id                   nombre                      tipo      pin
  {"luz_entrada",         "Luz de la entrada",        LUZ,      PIN_LUZ_ENTRADA},
  {"luz_oficina",         "Luz de la oficina",        LUZ,      PIN_LUZ_OFICINA},
  {"luz_parqueadero",     "Luz del parqueadero",      LUZ,      PIN_LUZ_PARQUEADERO},
  {"luz_bodega",          "Luz de la bodega",         LUZ,      PIN_LUZ_BODEGA},
  {"aspersor_frontal",    "Aspersor frontal",         ASPERSOR, PIN_ASPERSOR_FRONTAL},
  {"aspersor_trasero",    "Aspersor trasero",         ASPERSOR, PIN_ASPERSOR_TRASERO},
  {"puerta_principal",    "Puerta principal",         PUERTA,   PIN_PUERTA_PRINCIPAL},
  {"puerta_bodega",       "Puerta de la bodega",      PUERTA,   PIN_PUERTA_BODEGA},
  {"puerta_oficina",      "Puerta de la oficina",     PUERTA,   PIN_PUERTA_OFICINA},
  {"ventana_oficina",     "Ventana de la oficina",    VENTANA,  PIN_VENTANA_OFICINA},
  {"ventana_recepcion",   "Ventana de la recepción",  VENTANA,  PIN_VENTANA_RECEPCION},
};
const int NUM_DISPOSITIVOS = sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0]);

// Estado de cada dispositivo (misma posición que en la lista)
bool encendido[NUM_DISPOSITIVOS];        // salidas: ¿LED encendido?
bool abierto[NUM_DISPOSITIVOS];          // entradas: ¿puerta/ventana abierta?
bool lecturaAnterior[NUM_DISPOSITIVOS];  // para el antirrebote

// ---- Otros tipos y variables globales (se explican en sus secciones) --------

struct Aviso {              // sección 5
  String hora;
  String texto;
  bool esAlerta;
};

struct Tarea {              // sección 6
  bool activa;              // false = espacio libre
  uint32_t id;              // número de la tarea (#1, #2...)
  uint32_t momento;         // cuándo ejecutarla (segundos desde 1970)
  char accion[16];          // "encender", "apagar" o "avisar"
  char objetivo[24];        // dispositivo o grupo
  char condicion[24];       // "" o una condición
  char mensaje[104];        // texto (solo para "avisar")
};

Aviso avisos[MAX_AVISOS];
int cantidadAvisos = 0;
int alertasPendientes = 0;

Tarea tareas[MAX_TAREAS];
uint32_t siguienteId = 1;
Preferences memoria;

// "Candado" (mutex): el ESP32 hace dos cosas a la vez (sección 12). Cuando una
// modifica la lista de tareas o de avisos, la otra espera su turno.
SemaphoreHandle_t candado;

WebServer servidor(80);

// ---- Declaraciones adelantadas (funciones que se usan antes de definirse) ---
void agregarAviso(const String& texto, bool esAlerta);
String ejecutarAccion(const String& accion, const String& objetivo, const String& condicion, const String& mensaje);
String formatearMomento(time_t momento);

// ---- Funciones de dispositivos ----------------------------------------------

bool esSalida(const Dispositivo& d) { return d.tipo == LUZ || d.tipo == ASPERSOR; }

int posicion(const Dispositivo& d) { return &d - DISPOSITIVOS; }

// Busca un dispositivo por su id. Devuelve nullptr si no existe.
const Dispositivo* buscarDispositivo(const String& id) {
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    if (id == DISPOSITIVOS[i].id) return &DISPOSITIVOS[i];
  }
  return nullptr;
}

// Con INPUT_PULLUP el pin lee HIGH cuando el interruptor está abierto.
// Si un cable se suelta, se ve como "abierta" (falla segura).
bool leerPinAbierto(int pin) { return digitalRead(pin) == HIGH; }

void iniciarDispositivos() {
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    if (esSalida(d)) {
      pinMode(d.pin, OUTPUT);
      digitalWrite(d.pin, LOW);        // al arrancar todo queda apagado
      encendido[i] = false;
    } else {
      pinMode(d.pin, INPUT_PULLUP);    // resistencia interna; el interruptor va a GND
      abierto[i] = leerPinAbierto(d.pin);
      lecturaAnterior[i] = abierto[i];
    }
  }
  pinMode(PIN_LED_ALERTA, OUTPUT);
  digitalWrite(PIN_LED_ALERTA, LOW);
}

void cambiarSalida(const Dispositivo& d, bool encender) {
  if (!esSalida(d)) return;            // nunca se escribe en una entrada
  digitalWrite(d.pin, encender ? HIGH : LOW);
  encendido[posicion(d)] = encender;
}

// Lee los interruptores (cada 50 ms) y avisa cuando alguno cambia.
// Antirrebote: el cambio se acepta si dos lecturas seguidas coinciden.
void revisarEntradas() {
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    if (esSalida(d)) continue;
    bool lectura = leerPinAbierto(d.pin);
    if (lectura == lecturaAnterior[i] && lectura != abierto[i]) {
      abierto[i] = lectura;
      agregarAviso(String(d.nombre) + (lectura ? " se abrió" : " se cerró"), false);
    }
    lecturaAnterior[i] = lectura;
  }
}

// Nombres de puertas/ventanas abiertas (o cerradas), separados por coma.
// tipo: PUERTA, VENTANA o -1 para ambas.
String listarAccesos(int tipo, bool abiertos) {
  String lista = "";
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    if (esSalida(d)) continue;
    if (tipo != -1 && d.tipo != tipo) continue;
    if (abierto[i] != abiertos) continue;
    if (lista.length() > 0) lista += ", ";
    lista += d.nombre;
  }
  return lista;
}


// =============================================================================
// 5. AVISOS Y ALERTAS
// -----------------------------------------------------------------------------
//  Se guardan los últimos 20 avisos para mostrarlos en la página.
//  Las ALERTAS además hacen parpadear el LED rojo hasta marcarlas como vistas.
// =============================================================================

bool horaSincronizada() { return time(nullptr) > 1700000000; }   // posterior a nov-2023

String horaActual() {
  if (!horaSincronizada()) return "--:--";
  time_t ahora = time(nullptr);
  struct tm t;
  localtime_r(&ahora, &t);
  char texto[32];
  snprintf(texto, sizeof(texto), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  return texto;
}

void agregarAviso(const String& texto, bool esAlerta) {
  Serial.printf("[%s] %s\n", esAlerta ? "ALERTA" : "aviso", texto.c_str());
  xSemaphoreTake(candado, portMAX_DELAY);
  if (cantidadAvisos == MAX_AVISOS) {          // lista llena: se borra el más antiguo
    for (int i = 1; i < MAX_AVISOS; i++) avisos[i - 1] = avisos[i];
    cantidadAvisos--;
  }
  avisos[cantidadAvisos++] = {horaActual(), texto, esAlerta};
  if (esAlerta) alertasPendientes++;
  xSemaphoreGive(candado);
}

// LED rojo: parpadea 4 veces por segundo si hay alertas (con millis, sin delay)
void actualizarLedAlerta() {
  bool encender = alertasPendientes > 0 && (millis() / 125) % 2 == 0;
  digitalWrite(PIN_LED_ALERTA, encender ? HIGH : LOW);
}


// =============================================================================
// 6. TAREAS PROGRAMADAS
// -----------------------------------------------------------------------------
//  Una tarea es algo que debe pasar en el futuro: "encender luz_entrada a las
//  20:00". Se guardan en la memoria no volátil (NVS) del ESP32, así que
//  sobreviven a un reinicio o a un corte de luz.
// =============================================================================

// Copia un texto a un arreglo de tamaño fijo sin desbordarlo
void copiarTexto(char* destino, size_t tam, const String& origen) {
  strncpy(destino, origen.c_str(), tam - 1);
  destino[tam - 1] = '\0';
}

void guardarTareas() {
  memoria.putBytes("lista", tareas, sizeof(tareas));
  memoria.putUInt("siguiente", siguienteId);
}

// "06/11 20:00"
String formatearMomento(time_t momento) {
  struct tm t;
  localtime_r(&momento, &t);
  char texto[32];
  snprintf(texto, sizeof(texto), "%02d/%02d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
  return texto;
}

// Al arrancar: recupera las tareas guardadas antes del reinicio
void cargarTareas() {
  memoria.begin("agente", false);
  memset(tareas, 0, sizeof(tareas));
  if (memoria.getBytesLength("lista") == sizeof(tareas)) {
    memoria.getBytes("lista", tareas, sizeof(tareas));
    siguienteId = memoria.getUInt("siguiente", 1);
  }
  int recuperadas = 0;
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (!tareas[i].activa) continue;
    // Seguridad: solo acciones programables conocidas
    String accion = tareas[i].accion;
    if (accion != "encender" && accion != "apagar" && accion != "avisar") {
      tareas[i].activa = false;
      continue;
    }
    recuperadas++;
  }
  if (recuperadas > 0) agregarAviso("Se recuperaron " + String(recuperadas) + " tarea(s) guardadas antes del reinicio", false);
}

int espaciosLibres() {
  int libres = 0;
  for (int i = 0; i < MAX_TAREAS; i++) if (!tareas[i].activa) libres++;
  return libres;
}

// Guarda una tarea nueva. Devuelve su número, o 0 si no hay espacio.
uint32_t programarTarea(const String& accion, const String& objetivo, const String& condicion,
                        const String& mensaje, time_t momento) {
  uint32_t id = 0;
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (tareas[i].activa) continue;
    Tarea& t = tareas[i];
    t.activa = true;
    t.id = siguienteId++;
    t.momento = (uint32_t)momento;
    copiarTexto(t.accion, sizeof(t.accion), accion);
    copiarTexto(t.objetivo, sizeof(t.objetivo), objetivo);
    copiarTexto(t.condicion, sizeof(t.condicion), condicion);
    copiarTexto(t.mensaje, sizeof(t.mensaje), mensaje);
    id = t.id;
    guardarTareas();
    break;
  }
  xSemaphoreGive(candado);
  return id;
}

bool cancelarTarea(uint32_t id) {
  bool encontrada = false;
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (tareas[i].activa && tareas[i].id == id) {
      tareas[i].activa = false;
      encontrada = true;
      guardarTareas();
    }
  }
  xSemaphoreGive(candado);
  if (encontrada) agregarAviso("Tarea #" + String(id) + " cancelada", false);
  return encontrada;
}

// Se llama cada segundo: ejecuta las tareas cuya hora ya llegó.
// No usa delay(): solo compara la hora actual con la de cada tarea.
void revisarTareas() {
  static bool avisoHora = false;
  if (!horaSincronizada()) return;   // sin hora confiable, las tareas esperan
  if (!avisoHora) {
    avisoHora = true;
    agregarAviso("Hora sincronizada por internet: " + formatearMomento(time(nullptr)), false);
  }

  time_t ahora = time(nullptr);
  static Tarea vencidas[MAX_TAREAS];
  int cuantas = 0;

  // 1) Sacar de la lista las que ya vencieron (rápido, con el candado puesto)
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    if (tareas[i].activa && tareas[i].momento <= (uint32_t)ahora) {
      vencidas[cuantas++] = tareas[i];
      tareas[i].activa = false;
    }
  }
  if (cuantas > 0) guardarTareas();
  xSemaphoreGive(candado);

  // 2) Ejecutarlas (ya sin candado)
  for (int i = 0; i < cuantas; i++) {
    Tarea& t = vencidas[i];
    long retraso = (long)(ahora - t.momento);
    if (retraso > MINUTOS_TOLERANCIA * 60L) {
      // El equipo estuvo apagado: no se ejecuta tarde (ej. aspersores a las 3 a. m.)
      agregarAviso("La tarea #" + String(t.id) + " (" + t.accion + " " + t.objetivo + ", " +
                   formatearMomento(t.momento) + ") NO se ejecutó: el equipo estaba apagado o sin hora", true);
      continue;
    }
    String resultado = ejecutarAccion(t.accion, t.objetivo, t.condicion, t.mensaje);
    agregarAviso("Tarea #" + String(t.id) + ": " + resultado, false);
  }
}

// Texto con la lista de tareas (para la acción "listar_tareas")
String textoTareas() {
  String texto = "";
  int n = 0;
  xSemaphoreTake(candado, portMAX_DELAY);
  for (int i = 0; i < MAX_TAREAS; i++) {
    const Tarea& t = tareas[i];
    if (!t.activa) continue;
    texto += String(n++ ? "; " : "") + "#" + t.id + " " + formatearMomento(t.momento) + " " + t.accion + " " +
             (strlen(t.objetivo) ? t.objetivo : t.mensaje);
    if (strlen(t.condicion)) texto += String(" (si ") + t.condicion + ")";
  }
  xSemaphoreGive(candado);
  return n == 0 ? String("No hay tareas programadas") : String(n) + " tarea(s): " + texto;
}


// =============================================================================
// 7. CATÁLOGO DE ACCIONES PERMITIDAS
// -----------------------------------------------------------------------------
//  Estas son las ÚNICAS 6 cosas que el LLM puede pedir. El LLM nunca toca los
//  pines: solo elige una acción de esta lista y el ESP32 la ejecuta.
//
//    encender · apagar · avisar · consultar_accesos · listar_tareas · cancelar_tarea
// =============================================================================

const char* ACCIONES[]    = {"encender", "apagar", "avisar", "consultar_accesos", "listar_tareas", "cancelar_tarea"};
const char* CONDICIONES[] = {"puertas_cerradas", "alguna_puerta_abierta", "ventanas_cerradas", "alguna_ventana_abierta"};
const char* CAMPOS[]      = {"accion", "dispositivo", "hora", "minutos_despues", "condicion", "mensaje", "id"};
#define TAM(lista) (int)(sizeof(lista) / sizeof(lista[0]))

bool estaEnLista(const String& valor, const char* lista[], int tam) {
  for (int i = 0; i < tam; i++) if (valor == lista[i]) return true;
  return false;
}

// Solo estas se pueden programar; las consultas se responden de inmediato.
bool esProgramable(const String& accion) {
  return accion == "encender" || accion == "apagar" || accion == "avisar";
}

// Las CONDICIONES se revisan con los interruptores REALES, en el momento de
// ejecutar. El LLM no sabe cómo están las puertas; el ESP32 sí.
bool condicionCumplida(const String& condicion, String& detalle) {
  int tipo = condicion.indexOf("puerta") >= 0 ? PUERTA : VENTANA;
  String abiertas = listarAccesos(tipo, true);
  detalle = abiertas.length() == 0 ? String("todas cerradas") : "abiertas: " + abiertas;
  if (condicion.endsWith("cerradas")) return abiertas.length() == 0;  // puertas_cerradas / ventanas_cerradas
  return abiertas.length() > 0;                                      // alguna_..._abierta
}

// Ejecuta UNA acción del catálogo y devuelve un texto con lo que pasó.
String ejecutarAccion(const String& accion, const String& objetivo, const String& condicion,
                      const String& mensaje) {
  String detalle = "";
  if (condicion.length() > 0 && !condicionCumplida(condicion, detalle)) {
    return "No se hizo '" + accion + " " + (objetivo.length() ? objetivo : mensaje) +
           "' porque la condición '" + condicion + "' no se cumple (" + detalle + ")";
  }

  // ---- encender / apagar: un dispositivo o un grupo (luces, aspersores, todo)
  if (accion == "encender" || accion == "apagar") {
    bool encender = accion == "encender";
    const Dispositivo* d = buscarDispositivo(objetivo);
    if (d != nullptr && esSalida(*d)) {
      cambiarSalida(*d, encender);
      return String(d->nombre) + (encender ? ": encendido" : ": apagado");
    }
    int cambiados = 0;
    for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
      const Dispositivo& x = DISPOSITIVOS[i];
      bool entra = (objetivo == "luces" && x.tipo == LUZ) || (objetivo == "aspersores" && x.tipo == ASPERSOR) ||
                   (objetivo == "todo" && esSalida(x));
      if (entra) { cambiarSalida(x, encender); cambiados++; }
    }
    if (cambiados == 0) return "Dispositivo desconocido: " + objetivo;
    return String(encender ? "Se encendieron " : "Se apagaron ") + objetivo + " (" + cambiados + ")";
  }

  // ---- avisar: alerta en la página + LED rojo
  if (accion == "avisar") {
    String texto = mensaje;
    if (detalle.length() > 0 && detalle != "todas cerradas") texto += " (" + detalle + ")";
    agregarAviso(texto, true);
    return "Aviso enviado: " + texto;
  }

  // ---- consultar_accesos: qué puertas/ventanas están abiertas
  if (accion == "consultar_accesos") {
    int tipo = objetivo == "puertas" ? PUERTA : (objetivo == "ventanas" ? VENTANA : -1);
    String abiertas = listarAccesos(tipo, true);
    String cerradas = listarAccesos(tipo, false);
    if (abiertas.length() == 0) return "Todo está cerrado (" + cerradas + ")";
    return "Abiertas: " + abiertas + (cerradas.length() ? ". Cerradas: " + cerradas : String(""));
  }

  // ---- listar_tareas
  if (accion == "listar_tareas") return textoTareas();

  // ---- cancelar_tarea
  if (accion == "cancelar_tarea") {
    return cancelarTarea(objetivo.toInt()) ? "Tarea #" + objetivo + " cancelada" : "No existe la tarea #" + objetivo;
  }

  return "Acción no permitida";   // nunca debería llegar aquí (ya se validó)
}


// =============================================================================
// 8. VALIDAR EL JSON DEL LLM
// -----------------------------------------------------------------------------
//  El JSON del LLM se trata como "no confiable": se revisa campo por campo.
//  Si hay UN SOLO error, NO se ejecuta NADA (así no quedan cosas a medias).
//  Formato completo en docs/FORMATO_JSON.md
// =============================================================================

// ¿Se puede encender/apagar? Una luz, un aspersor o un grupo.
bool esObjetivoValido(const String& objetivo) {
  if (objetivo == "luces" || objetivo == "aspersores" || objetivo == "todo") return true;
  const Dispositivo* d = buscarDispositivo(objetivo);
  return d != nullptr && esSalida(*d);
}

// Revisa que el texto sea "HH:MM" (24 horas)
bool leerHora(const String& texto, int& h, int& m) {
  if (texto.length() != 5 || texto[2] != ':') return false;
  if (!isDigit(texto[0]) || !isDigit(texto[1]) || !isDigit(texto[3]) || !isDigit(texto[4])) return false;
  h = texto.substring(0, 2).toInt();
  m = texto.substring(3, 5).toInt();
  return h <= 23 && m <= 59;
}

// Próxima vez que el reloj marque HH:MM (hoy, o mañana si ya pasó)
time_t proximaHora(int h, int m, time_t ahora) {
  struct tm t;
  localtime_r(&ahora, &t);
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  time_t momento = mktime(&t);
  if (momento <= ahora) {
    t.tm_mday += 1;
    momento = mktime(&t);
  }
  return momento;
}

// Valida el plan y, si es correcto, lo ejecuta. Escribe el resultado en
// "respuesta". Devuelve true si el plan era válido.
bool procesarPlan(JsonVariant plan, JsonObject respuesta) {
  JsonArray errores = respuesta["errores"].to<JsonArray>();

  // ---- 8.1 Estructura general: solo se permiten "tipo", "mensaje" y "acciones"
  if (!plan.is<JsonObject>()) {
    errores.add("La respuesta no es un objeto JSON");
  } else {
    for (JsonPair campo : plan.as<JsonObject>()) {
      String nombre = campo.key().c_str();
      if (nombre != "tipo" && nombre != "mensaje" && nombre != "acciones") errores.add("Campo no permitido: '" + nombre + "'");
    }
  }
  String tipo = plan["tipo"] | "";
  String mensaje = plan["mensaje"] | "";

  // ---- 8.2 Aclaración: falta información -> se pregunta y NO se guarda nada
  if (errores.size() == 0 && tipo == "aclaracion") {
    if (mensaje.length() == 0) {
      errores.add("Una aclaración necesita la pregunta en 'mensaje'");
    } else {
      respuesta["ok"] = true;
      respuesta["tipo"] = "aclaracion";
      respuesta["pregunta"] = mensaje;
      return true;
    }
  }

  // ---- 8.3 No soportado: la orden no corresponde a ninguna acción
  if (errores.size() == 0 && tipo == "no_soportado") {
    respuesta["ok"] = true;
    respuesta["tipo"] = "no_soportado";
    respuesta["mensaje"] = mensaje.length() ? mensaje : String("Esa orden no se puede realizar con este sistema.");
    return true;
  }
  if (errores.size() == 0 && tipo != "plan") errores.add("'tipo' debe ser plan, aclaracion o no_soportado");

  // ---- 8.4 Revisar cada acción
  JsonArray acciones = plan["acciones"].as<JsonArray>();
  if (tipo == "plan") {
    if (acciones.isNull() || acciones.size() == 0) errores.add("El plan no tiene acciones");
    else if (acciones.size() > MAX_ACCIONES) errores.add("Máximo " + String(MAX_ACCIONES) + " acciones por orden");
  }

  int aProgramar = 0;
  int numero = 0;
  for (JsonVariant a : acciones) {
    numero++;
    String p = "Acción " + String(numero) + ": ";
    if (!a.is<JsonObject>()) { errores.add(p + "debe ser un objeto JSON"); continue; }

    // Campos desconocidos -> error
    for (JsonPair campo : a.as<JsonObject>()) {
      String nombre = campo.key().c_str();
      if (!estaEnLista(nombre, CAMPOS, TAM(CAMPOS))) errores.add(p + "campo no permitido '" + nombre + "'");
    }

    // La acción debe estar en el catálogo
    if (!a["accion"].is<const char*>()) { errores.add(p + "falta 'accion'"); continue; }
    String accion = a["accion"].as<const char*>();
    if (!estaEnLista(accion, ACCIONES, TAM(ACCIONES))) { errores.add(p + "acción no permitida '" + accion + "'"); continue; }
    String dispositivo = a["dispositivo"] | "";

    // Datos que necesita cada acción
    if (accion == "encender" || accion == "apagar") {
      const Dispositivo* d = buscarDispositivo(dispositivo);
      if (dispositivo.length() == 0) errores.add(p + "falta 'dispositivo'");
      else if (d != nullptr && !esSalida(*d)) errores.add(p + "'" + dispositivo + "' es una puerta/ventana: solo se puede consultar, no encender ni apagar");
      else if (!esObjetivoValido(dispositivo)) errores.add(p + "el dispositivo '" + dispositivo + "' no existe");
    }
    if (accion == "consultar_accesos" && dispositivo.length() > 0 &&
        dispositivo != "puertas" && dispositivo != "ventanas" && dispositivo != "todo") {
      errores.add(p + "para consultar use 'puertas', 'ventanas' o 'todo'");
    }
    if (accion == "avisar") {
      String texto = a["mensaje"] | "";
      if (texto.length() == 0) errores.add(p + "falta el 'mensaje' del aviso");
      if (texto.length() > 100) errores.add(p + "el mensaje es demasiado largo (máx. 100 caracteres)");
    }
    if (accion == "cancelar_tarea" && !(a["id"].is<int>() && a["id"].as<int>() > 0)) {
      errores.add(p + "'id' debe ser el número de la tarea");
    }

    // Hora y minutos (tipos correctos y rangos válidos)
    bool tieneHora = !a["hora"].isNull();
    bool tieneMinutos = !a["minutos_despues"].isNull();
    if ((tieneHora || tieneMinutos) && !esProgramable(accion)) {
      errores.add(p + "'" + accion + "' es una consulta inmediata y no se puede programar");
    }
    int h, m;
    if (tieneHora && !(a["hora"].is<const char*>() && leerHora(a["hora"].as<const char*>(), h, m))) {
      errores.add(p + "hora inválida, use el formato HH:MM de 24 horas (ej. \"20:00\")");
    }
    int minutos = 0;
    if (tieneMinutos) {
      if (!a["minutos_despues"].is<int>() || a["minutos_despues"].as<int>() < 0 || a["minutos_despues"].as<int>() > 1440)
        errores.add(p + "'minutos_despues' debe ser un número entero entre 0 y 1440");
      else minutos = a["minutos_despues"].as<int>();
    }

    // Condición
    if (!a["condicion"].isNull()) {
      String condicion = a["condicion"] | "";
      if (!estaEnLista(condicion, CONDICIONES, TAM(CONDICIONES))) errores.add(p + "condición no permitida '" + condicion + "'");
      else if (!esProgramable(accion)) errores.add(p + "'" + accion + "' no admite condición");
    }

    if (tieneHora || minutos > 0) aProgramar++;
  }

  if (aProgramar > 0 && !horaSincronizada())
    errores.add("El reloj aún no está sincronizado por internet: por ahora solo se aceptan órdenes inmediatas");
  if (aProgramar > espaciosLibres())
    errores.add("No hay espacio para más tareas (máximo " + String(MAX_TAREAS) + ")");

  // ---- 8.5 Si hay errores, NO se ejecuta nada
  if (errores.size() > 0) {
    respuesta["ok"] = false;
    respuesta["tipo"] = "rechazado";
    respuesta["mensaje"] = "La orden tiene errores. No se ejecutó nada.";
    return false;
  }

  // ---- 8.6 Todo correcto: ejecutar lo inmediato y programar lo demás
  JsonArray resultados = respuesta["resultados"].to<JsonArray>();
  time_t ahora = time(nullptr);
  for (JsonVariant a : acciones) {
    String accion = a["accion"].as<const char*>();
    String objetivo = a["dispositivo"] | "";
    if (accion == "cancelar_tarea") objetivo = String(a["id"].as<int>());
    String condicion = a["condicion"] | "";
    String texto = a["mensaje"] | "";

    // ¿Cuándo? 0 = ahora mismo
    time_t momento = 0;
    int h, m;
    if (!a["hora"].isNull() && leerHora(a["hora"].as<const char*>(), h, m)) momento = proximaHora(h, m, ahora);
    int minutos = a["minutos_despues"] | 0;
    if (minutos > 0) momento = (momento ? momento : ahora) + minutos * 60;

    if (momento == 0) {
      resultados.add(ejecutarAccion(accion, objetivo, condicion, texto));
    } else {
      uint32_t id = programarTarea(accion, objetivo, condicion, texto, momento);
      String descripcion = accion + " " + (objetivo.length() ? objetivo : "\"" + texto + "\"");
      if (condicion.length()) descripcion += " si " + condicion;
      resultados.add("Tarea #" + String(id) + " programada para " + formatearMomento(momento) + ": " + descripcion);
    }
  }
  respuesta["ok"] = true;
  respuesta["tipo"] = "plan";
  respuesta["mensaje"] = mensaje;
  return true;
}

// ¿La orden usa un tiempo vago ("más tarde", "en un rato") sin decir una hora
// concreta? Entonces se pregunta ANTES de consultar al LLM.
bool esAmbigua(const String& textoOriginal) {
  String t = textoOriginal;
  t.toLowerCase();
  const char* conTilde[] = {"á", "é", "í", "ó", "ú", "Á", "É", "Í", "Ó", "Ú"};
  const char* sinTilde[] = {"a", "e", "i", "o", "u", "a", "e", "i", "o", "u"};
  for (int i = 0; i < 10; i++) t.replace(conTilde[i], sinTilde[i]);

  if (t.indexOf('?') >= 0) return false;   // es una pregunta, no una orden con horario

  const char* vagas[] = {"mas tarde", "en un rato", "al rato", "luego", "pronto", "despuesito",
                         "en algun momento", "cuando puedas", "esta noche", "esta tarde"};
  bool tieneVaga = false;
  for (const char* v : vagas) if (t.indexOf(v) >= 0) tieneVaga = true;
  if (!tieneVaga) return false;

  // Si además dice una hora o cantidad concreta, ya no es ambigua
  for (unsigned i = 0; i < t.length(); i++) if (isDigit(t[i])) return false;
  const char* concretas[] = {"ahora", "inmediat", "media hora", "una hora", "un minuto", "cinco", "diez",
                             "quince", "veinte", "treinta", "cuarenta", "mediodia", "medianoche"};
  for (const char* c : concretas) if (t.indexOf(c) >= 0) return false;
  return true;
}


// =============================================================================
// 9. CONSULTAR AL LLM (Gemini o Claude, según PROVEEDOR_LLM de la sección 2)
// -----------------------------------------------------------------------------
//  Petición HTTPS normal con HTTPClient. Se le envían unas INSTRUCCIONES
//  (qué formato devolver y qué dispositivos existen) y la orden del usuario.
// =============================================================================

String crearInstrucciones() {
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

// Devuelve true si todo salió bien; el JSON del plan queda en "planJson".
// Si falla, la explicación queda en "error".
bool consultarLLM(const String& orden, String& planJson, String& error) {
  if (String(LLM_API_KEY).length() < 10) {
    error = "No hay clave de la API del LLM (sección 1 del código)";
    return false;
  }
  String hora = horaSincronizada() ? formatearMomento(time(nullptr)) : String("desconocida");
  String mensajeUsuario = "Fecha y hora actual (día/mes hora): " + hora + "\nOrden: " + orden;

  // ---- 9.1 Armar la petición
  //  Los dos proveedores piden lo mismo (instrucciones + orden), pero cada uno
  //  con su propio formato de JSON.
  JsonDocument peticion;
  peticion["model"] = MODELO_LLM;
#if PROVEEDOR_LLM == 1
  // Formato "chat completions" (Gemini): las instrucciones van como mensaje "system"
  JsonArray mensajes = peticion["messages"].to<JsonArray>();
  JsonObject sistema = mensajes.add<JsonObject>();
  sistema["role"] = "system";
  sistema["content"] = crearInstrucciones();
  JsonObject usuario = mensajes.add<JsonObject>();
  usuario["role"] = "user";
  usuario["content"] = mensajeUsuario;
#else
  // Formato "Messages" (Claude): las instrucciones van en el campo "system"
  peticion["max_tokens"] = 4096;
  peticion["system"] = crearInstrucciones();
  peticion["output_config"]["effort"] = "low";   // respuesta rápida: la tarea es sencilla
  peticion["fallbacks"] = "default";             // si el modelo rechaza la orden, el servidor reintenta con otro
  JsonObject usuario = peticion["messages"].to<JsonArray>().add<JsonObject>();
  usuario["role"] = "user";
  usuario["content"] = mensajeUsuario;
#endif
  String cuerpo;
  serializeJson(peticion, cuerpo);

  // ---- 9.2 Enviarla por HTTPS
  WiFiClientSecure cliente;
  cliente.setInsecure();   // conexión cifrada sin verificar el certificado (simplificación del prototipo)
  HTTPClient http;
  http.begin(cliente, URL_LLM);
  http.setTimeout(30000);
  http.addHeader("Content-Type", "application/json");
#if PROVEEDOR_LLM == 1
  http.addHeader("Authorization", String("Bearer ") + LLM_API_KEY);
#else
  http.addHeader("x-api-key", LLM_API_KEY);
  http.addHeader("anthropic-version", "2026-10-21");
  http.addHeader("anthropic-beta", "server-side-fallback-2026-07-01");
#endif

  Serial.println("[llm] Consultando al modelo...");
  int codigo = http.POST(cuerpo);
  if (codigo <= 0) {
    error = "No se pudo conectar con la API del LLM (" + http.errorToString(codigo) + ")";
    http.end();
    return false;
  }
  String texto = http.getString();
  http.end();

  // ---- 9.3 Leer la respuesta
  JsonDocument respuesta;
  if (deserializeJson(respuesta, texto)) {
    error = "La API respondió algo que no es JSON (HTTP " + String(codigo) + ")";
    return false;
  }
  if (codigo != 200) {
    // El mensaje de error puede venir como {"error":{...}} o como [{"error":{...}}]
    String detalle = respuesta["error"]["message"] | (respuesta[0]["error"]["message"] | "");
    error = "La API respondió con error HTTP " + String(codigo) + ": " + detalle;
    return false;
  }

  String salida = "";
#if PROVEEDOR_LLM == 1
  // Gemini: el texto viene en choices[0].message.content
  salida = respuesta["choices"][0]["message"]["content"] | "";
#else
  // Claude: primero revisar si el modelo se negó; el texto viene en bloques "text"
  String motivo = respuesta["stop_reason"] | "";
  if (motivo == "refusal") {
    error = "El modelo no quiso procesar esa orden";
    return false;
  }
  for (JsonObject bloque : respuesta["content"].as<JsonArray>()) {
    if (bloque["type"] == "text") salida += bloque["text"].as<const char*>();
  }
#endif

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


// =============================================================================
// 10. PÁGINA WEB (HTML + CSS + JavaScript)
// -----------------------------------------------------------------------------
//  Página sencilla guardada en la memoria del ESP32. Cada 2 segundos pide
//  /api/estado para mostrar luces, puertas, tareas y avisos.
//
//  Nota: en el JavaScript las funciones se escriben "const nombre = () => {}"
//  y no "function nombre() {}". El Arduino IDE revisa este archivo buscando
//  funciones de C++ y se confundiría con las de JavaScript.
// =============================================================================

const char PAGINA_WEB[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Agente ESP32</title>
<style>
  body { font-family: Arial, sans-serif; background: #f2f4f7; margin: 0; padding: 16px; color: #222; }
  h1 { font-size: 22px; margin: 0 0 4px; }
  .info { color: #666; font-size: 14px; margin-bottom: 16px; }
  .caja { background: white; border-radius: 10px; padding: 14px; margin-bottom: 14px; box-shadow: 0 1px 3px #0002; }
  h2 { font-size: 17px; margin: 0 0 10px; }
  textarea { width: 100%; box-sizing: border-box; height: 60px; font-size: 15px; padding: 8px; }
  button { padding: 7px 12px; border: none; border-radius: 6px; background: #0b7a75; color: white; cursor: pointer; font-size: 14px; }
  button.gris { background: #8a939c; }
  .ejemplo { background: #e3eeed; color: #0b5a56; margin: 4px 4px 0 0; font-size: 13px; }
  pre { background: #f6f6f6; padding: 10px; white-space: pre-wrap; font-size: 13px; border-radius: 6px; }
  .fila { display: flex; justify-content: space-between; align-items: center; padding: 6px 0; border-bottom: 1px solid #eee; gap: 8px; }
  .on { color: #c77700; font-weight: bold; } .off { color: #888; }
  .abierta { color: #c62828; font-weight: bold; } .cerrada { color: #2e7d32; }
  .alerta { color: #c62828; font-weight: bold; }
  .columnas { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
  @media (max-width: 800px) { .columnas { grid-template-columns: 1fr; } }
</style>
</head>
<body>
  <h1>🏢 Agente de automatización ESP32-S3</h1>
  <div class="info">Hora: <span id="hora">...</span> · Wi-Fi: <span id="wifi">...</span></div>

  <div class="caja">
    <h2>1. Escribe una orden</h2>
    <textarea id="texto" placeholder="Ejemplo: Enciende la luz de la oficina a las 6:00 p. m."></textarea>
    <p id="pendiente" class="alerta"></p>
    <button onclick="enviarOrden()">Enviar orden</button>
    <div id="ejemplos"></div>
    <h2 style="margin-top:14px">Respuesta</h2>
    <pre id="respuesta">Aquí aparecerá lo que hizo el sistema.</pre>
  </div>

  <div class="columnas">
    <div class="caja"><h2>💡 Luces y aspersores</h2><div id="salidas"></div></div>
    <div class="caja"><h2>🚪 Puertas y ventanas</h2><div id="entradas"></div></div>
    <div class="caja"><h2>⏰ Tareas programadas</h2><div id="tareas"></div></div>
    <div class="caja">
      <h2>🔔 Avisos <button class="gris" onclick="alertasVistas()">Marcar alertas como vistas</button></h2>
      <div id="avisos"></div>
    </div>
  </div>

<script>
// Órdenes de ejemplo (los escenarios del taller)
const EJEMPLOS = [
  "A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta",
  "Revisa las puertas y, si todas están cerradas, apaga todas las luces",
  "Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero",
  "¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?",
  "Enciende los aspersores más tarde"
];
for (const e of EJEMPLOS) {
  const b = document.createElement("button");
  b.className = "ejemplo"; b.textContent = e;
  b.onclick = () => document.getElementById("texto").value = e;
  document.getElementById("ejemplos").appendChild(b);
}

// Si el sistema hizo una pregunta, se guarda la orden original
let ordenPendiente = null;

const enviarOrden = async () => {
  let texto = document.getElementById("texto").value.trim();
  if (!texto) return;
  if (ordenPendiente) {   // la respuesta completa la orden anterior
    texto = ordenPendiente + ". Aclaración: " + texto;
    ordenPendiente = null;
  }
  document.getElementById("respuesta").textContent = "Pensando... (consultando al LLM)";
  document.getElementById("pendiente").textContent = "";
  const r = await fetch("/api/orden", { method: "POST", body: JSON.stringify({ texto }) });
  const datos = await r.json();
  mostrarRespuesta(datos);
  if (datos.tipo === "aclaracion") {
    ordenPendiente = texto;
    document.getElementById("pendiente").textContent = "❓ " + datos.pregunta + " (escribe tu respuesta y envía)";
  }
  document.getElementById("texto").value = "";
  actualizar();
}

const mostrarRespuesta = (d) => {
  let t = (d.ok ? "✅ " : "❌ ") + (d.tipo || "") + "\n";
  if (d.mensaje) t += d.mensaje + "\n";
  if (d.pregunta) t += "Pregunta: " + d.pregunta + "\n";
  if (d.resultados) t += "\nResultados:\n - " + d.resultados.join("\n - ") + "\n";
  if (d.errores && d.errores.length) t += "\nErrores:\n - " + d.errores.join("\n - ") + "\n";
  if (d.json_del_llm) t += "\nJSON que devolvió el LLM:\n" + d.json_del_llm;
  document.getElementById("respuesta").textContent = t;
}

// Botones de encender/apagar: envían un plan JSON directo (sin LLM)
const cambiar = async (id, accion) => {
  const plan = { tipo: "plan", acciones: [{ accion: accion, dispositivo: id }] };
  await fetch("/api/plan", { method: "POST", body: JSON.stringify(plan) });
  actualizar();
}

const cancelar = async (id) => {
  await fetch("/api/cancelar", { method: "POST", body: JSON.stringify({ id }) });
  actualizar();
}

const alertasVistas = async () => {
  await fetch("/api/alertas/vistas", { method: "POST" });
  actualizar();
}

// Evita que un texto se interprete como HTML
const seguro = (s) => { const d = document.createElement("div"); d.textContent = s; return d.innerHTML; };

const actualizar = async () => {
  const e = await (await fetch("/api/estado")).json();
  document.getElementById("hora").textContent = e.hora;
  document.getElementById("wifi").textContent = e.wifi;

  let salidas = "", entradas = "";
  for (const d of e.dispositivos) {
    if (d.tipo === "luz" || d.tipo === "aspersor") {
      salidas += `<div class="fila"><span>${d.nombre} <small>(GPIO ${d.pin})</small></span>
        <span class="${d.encendido ? "on" : "off"}">${d.encendido ? "ENCENDIDO" : "apagado"}</span>
        <button onclick="cambiar('${d.id}','${d.encendido ? "apagar" : "encender"}')">${d.encendido ? "Apagar" : "Encender"}</button></div>`;
    } else {
      entradas += `<div class="fila"><span>${d.nombre} <small>(GPIO ${d.pin})</small></span>
        <span class="${d.abierto ? "abierta" : "cerrada"}">${d.abierto ? "ABIERTA" : "cerrada"}</span></div>`;
    }
  }
  document.getElementById("salidas").innerHTML = salidas;
  document.getElementById("entradas").innerHTML = entradas;

  document.getElementById("tareas").innerHTML = e.tareas.length === 0 ? "No hay tareas." :
    e.tareas.map(t => `<div class="fila"><span><b>#${t.id}</b> ${t.cuando} — ${t.accion} ${seguro(t.objetivo || t.mensaje)}
      ${t.condicion ? "<small>(si " + t.condicion + ")</small>" : ""}</span>
      <button class="gris" onclick="cancelar(${t.id})">Cancelar</button></div>`).join("");

  document.getElementById("avisos").innerHTML = e.avisos.length === 0 ? "Sin avisos." :
    e.avisos.map(a => `<div class="fila ${a.alerta ? "alerta" : ""}"><span>${a.hora} — ${seguro(a.texto)}</span></div>`).join("");
}

actualizar();
setInterval(actualizar, 2000);
</script>
</body>
</html>
)HTML";


// =============================================================================
// 11. SERVIDOR WEB Y API
// -----------------------------------------------------------------------------
//    GET  /                    página web
//    GET  /api/estado          dispositivos, tareas y avisos
//    POST /api/orden           orden en lenguaje natural   {"texto": "..."}
//    POST /api/plan            plan JSON directo (sin LLM)
//    POST /api/cancelar        cancelar una tarea          {"id": 3}
//    POST /api/alertas/vistas  apagar el LED de alerta
//  Detalle de cada petición y respuesta en docs/API.md
// =============================================================================

void responder(int codigo, JsonDocument& doc) {
  String texto;
  serializeJson(doc, texto);
  servidor.send(codigo, "application/json; charset=utf-8", texto);
}

void responderError(int codigo, const String& mensaje) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["mensaje"] = mensaje;
  responder(codigo, doc);
}

// Lee el cuerpo JSON de la petición. Si está mal, responde 400.
bool leerCuerpo(JsonDocument& doc) {
  if (deserializeJson(doc, servidor.arg("plain"))) {
    responderError(400, "El cuerpo de la petición no es un JSON válido");
    return false;
  }
  return true;
}

// GET / -> la página web
void rutaPagina() { servidor.send_P(200, "text/html; charset=utf-8", PAGINA_WEB); }

// GET /api/estado -> todo lo que muestra la página
void rutaEstado() {
  JsonDocument doc;
  doc["hora"] = horaSincronizada() ? formatearMomento(time(nullptr)) : String("sin sincronizar");
  doc["wifi"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("sin conexión (modo respaldo)");
  doc["alertas_pendientes"] = alertasPendientes;

  JsonArray lista = doc["dispositivos"].to<JsonArray>();
  for (int i = 0; i < NUM_DISPOSITIVOS; i++) {
    const Dispositivo& d = DISPOSITIVOS[i];
    JsonObject o = lista.add<JsonObject>();
    o["id"] = d.id;
    o["nombre"] = d.nombre;
    o["pin"] = d.pin;
    if (esSalida(d)) {
      o["tipo"] = d.tipo == LUZ ? "luz" : "aspersor";
      o["encendido"] = encendido[i];
    } else {
      o["tipo"] = d.tipo == PUERTA ? "puerta" : "ventana";
      o["abierto"] = abierto[i];
    }
  }

  xSemaphoreTake(candado, portMAX_DELAY);
  JsonArray listaTareas = doc["tareas"].to<JsonArray>();
  for (int i = 0; i < MAX_TAREAS; i++) {
    const Tarea& t = tareas[i];
    if (!t.activa) continue;
    JsonObject o = listaTareas.add<JsonObject>();
    o["id"] = t.id;
    o["cuando"] = formatearMomento(t.momento);
    o["accion"] = t.accion;
    o["objetivo"] = t.objetivo;
    o["condicion"] = t.condicion;
    o["mensaje"] = t.mensaje;
  }
  JsonArray listaAvisos = doc["avisos"].to<JsonArray>();
  for (int i = cantidadAvisos - 1; i >= 0; i--) {   // del más reciente al más antiguo
    JsonObject a = listaAvisos.add<JsonObject>();
    a["hora"] = avisos[i].hora;
    a["texto"] = avisos[i].texto;
    a["alerta"] = avisos[i].esAlerta;
  }
  xSemaphoreGive(candado);
  responder(200, doc);
}

// POST /api/orden -> orden en lenguaje natural
void rutaOrden() {
  JsonDocument cuerpo;
  if (!leerCuerpo(cuerpo)) return;
  String texto = cuerpo["texto"] | "";
  texto.trim();
  if (texto.length() == 0) return responderError(400, "Falta el campo 'texto'");
  if (texto.length() > 400) return responderError(400, "La orden es demasiado larga (máx. 400 caracteres)");

  JsonDocument respuesta;
  respuesta["orden"] = texto;

  // 1) Orden ambigua: se pregunta ANTES de gastar una consulta al LLM
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
  respuesta["json_del_llm"] = planTexto;   // se muestra para ver qué respondió el modelo

  // 4) Validar y ejecutar
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

// POST /api/plan -> probar el sistema SIN LLM, enviando el JSON directamente
void rutaPlan() {
  JsonDocument plan;
  if (!leerCuerpo(plan)) return;
  JsonDocument respuesta;
  bool valido = procesarPlan(plan.as<JsonVariant>(), respuesta.to<JsonObject>());
  responder(valido ? 200 : 422, respuesta);
}

// POST /api/cancelar -> {"id": 3}
void rutaCancelar() {
  JsonDocument cuerpo;
  if (!leerCuerpo(cuerpo)) return;
  int id = cuerpo["id"] | 0;
  if (!cancelarTarea(id)) return responderError(404, "No existe la tarea #" + String(id));
  JsonDocument doc;
  doc["ok"] = true;
  doc["mensaje"] = "Tarea #" + String(id) + " cancelada";
  responder(200, doc);
}

// POST /api/alertas/vistas -> apaga el parpadeo del LED rojo
void rutaAlertasVistas() {
  alertasPendientes = 0;
  JsonDocument doc;
  doc["ok"] = true;
  responder(200, doc);
}

void iniciarServidor() {
  servidor.on("/", HTTP_GET, rutaPagina);
  servidor.on("/api/estado", HTTP_GET, rutaEstado);
  servidor.on("/api/orden", HTTP_POST, rutaOrden);
  servidor.on("/api/plan", HTTP_POST, rutaPlan);
  servidor.on("/api/cancelar", HTTP_POST, rutaCancelar);
  servidor.on("/api/alertas/vistas", HTTP_POST, rutaAlertasVistas);
  servidor.onNotFound([]() { responderError(404, "Ruta no encontrada"); });
  servidor.begin();
}


// =============================================================================
// 12. TAREA DE MONITOREO (hacer dos cosas a la vez)
// -----------------------------------------------------------------------------
//  El ESP32-S3 tiene dos núcleos. Esta función corre aparte, en el núcleo 1,
//  en un ciclo infinito. Así, aunque loop() esté esperando la respuesta del
//  LLM (varios segundos), las puertas se siguen vigilando y las tareas se
//  ejecutan a tiempo.
// =============================================================================

void tareaMonitoreo(void* parametro) {
  unsigned long ultimaRevision = 0;
  while (true) {
    revisarEntradas();          // ¿se abrió o cerró alguna puerta/ventana?
    actualizarLedAlerta();      // parpadeo del LED rojo
    if (millis() - ultimaRevision >= 1000) {   // una vez por segundo
      ultimaRevision = millis();
      revisarTareas();          // ¿ya es hora de alguna tarea?
    }
    vTaskDelay(pdMS_TO_TICKS(50));   // pausa de 50 ms que NO bloquea al resto
  }
}


// =============================================================================
// 13. WI-FI, setup() y loop()
// =============================================================================

// Intenta conectarse 15 s. Si no puede, crea su propia red para que la página
// web siga disponible (en http://192.168.4.1).
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

  candado = xSemaphoreCreateMutex();
  iniciarDispositivos();   // salidas apagadas, entradas listas
  cargarTareas();          // recupera las tareas guardadas antes de un reinicio

  // Arranca el monitoreo en el núcleo 1 (antes de conectarse a internet)
  xTaskCreatePinnedToCore(tareaMonitoreo, "monitoreo", 8192, nullptr, 2, nullptr, 1);

  conectarWiFi();
  configTzTime(ZONA_HORARIA, SERVIDOR_NTP);   // pide la hora a internet (NTP)
  iniciarServidor();
}

void loop() {
  servidor.handleClient();   // responde a la página web y a la API

  // Avisar si se pierde (o vuelve) la conexión Wi-Fi
  static bool conectadoAntes = WiFi.status() == WL_CONNECTED;
  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado != conectadoAntes) {
    agregarAviso(conectado ? "Wi-Fi reconectado" : "Se perdió el Wi-Fi: las tareas siguen funcionando, el LLM no", !conectado);
    conectadoAntes = conectado;
  }
  delay(2);
}

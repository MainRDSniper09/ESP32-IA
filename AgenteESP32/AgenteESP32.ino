/*
 * =============================================================================
 *   ASISTENTE DE VOZ PARA LAS LUCES DE UNA EMPRESA  —  PARTE 1
 *   ESP32-S3  ·  C++ (Arduino IDE)  ·  Interpretación de texto en el propio ESP32
 * =============================================================================
 *
 *  ¿QUÉ HACE?
 *  ----------
 *  Una empresa de un solo piso tiene 6 espacios. Cada uno tiene una luz,
 *  representada por un LED. El usuario dice una orden como:
 *
 *        "apaga la luz de la cocina"
 *
 *  1. El micrófono del celular o del PC (desde la página web) convierte la voz
 *     en texto. También se puede escribir en la página o en el Monitor Serie.
 *  2. El ESP32 INTERPRETA ese texto él mismo (sin internet, sin IA externa):
 *       - Compara cada palabra captada con las palabras que conoce.
 *       - Calcula un PORCENTAJE de parecido (0 % = nada que ver, 100 % = igual).
 *         Así entiende aunque el reconocimiento de voz se equivoque un poco
 *         ("cosina" se parece 83 % a "cocina").
 *  3. Con esos porcentajes DECIDE:
 *       - QUÉ hacer: encender o apagar.
 *       - DÓNDE: una habitación, varias ("la cocina y la oficina") o todas.
 *  4. Enciende/apaga los LED y avisa con un LED verde (entendido) o
 *     rojo (no entendido).
 *
 *  CONEXIONES (ver docs/diagrama_conexiones.svg)
 *  ---------------------------------------------
 *  Cada LED:  GPIO ──[ resistencia 220 Ω ]──►|── GND
 *                                    pata larga  pata corta
 *
 *     GPIO 4  ── Recepción        (LED amarillo)
 *     GPIO 5  ── Oficina          (LED amarillo)
 *     GPIO 6  ── Sala de juntas   (LED amarillo)
 *     GPIO 7  ── Cocina           (LED amarillo)
 *     GPIO 15 ── Baño             (LED amarillo)
 *     GPIO 16 ── Bodega           (LED amarillo)
 *     GPIO 17 ── LED VERDE: "orden entendida"
 *     GPIO 18 ── LED ROJO:  "no entendí"
 *
 *  QUÉ NECESITAS EN EL ARDUINO IDE
 *  -------------------------------
 *   - Placa "esp32" de Espressif (probado con 2.0.17) -> ESP32S3 Dev Module
 *   - Librería "ArduinoJson" de Benoit Blanchon (versión 7)
 *   - Monitor Serie a 115200 baudios
 *
 *  SECCIONES DE ESTE ARCHIVO
 *  -------------------------
 *    1. Wi-Fi                       6. Ejecutar la decisión (LED)
 *    2. Habitaciones y pines        7. Monitor Serie
 *    3. Vocabulario ("entrenamiento") 8. Página web
 *    4. Herramientas de texto       9. Servidor web y API
 *    5. El intérprete (porcentajes) 10. setup() y loop()
 *
 *  Licencia MIT · Juan Nicolás Barreto Sánchez · 2026
 * =============================================================================
 */

#include <WiFi.h>         // conexión Wi-Fi
#include <WebServer.h>    // página web y API
#include <ArduinoJson.h>  // respuestas en formato JSON


// =============================================================================
// 1. WI-FI
// -----------------------------------------------------------------------------
//  Recomendado: crea en esta carpeta un archivo "credenciales.h" (no se sube a
//  GitHub) con:
//      #define WIFI_NOMBRE "MiRed"
//      #define WIFI_CLAVE  "MiClave"
//  Si no existe, se usan los valores de abajo.
//  Sin Wi-Fi el ESP32 crea su propia red "Asistente-ESP32" (clave asistente123).
//  La interpretación NO necesita internet: funciona igual en ambos casos.
// =============================================================================
#if __has_include("credenciales.h")
  #include "credenciales.h"
#else
  #define WIFI_NOMBRE  "NombreDeTuRed"
  #define WIFI_CLAVE   "ClaveDeTuRed"
#endif

#define RED_RESPALDO    "Asistente-ESP32"
#define CLAVE_RESPALDO  "asistente123"


// =============================================================================
// 2. HABITACIONES Y PINES
// -----------------------------------------------------------------------------
//  Cada habitación tiene: nombre, pin del LED y las palabras con las que el
//  usuario puede referirse a ella. Para agregar una habitación, añade una fila.
// =============================================================================

#define PIN_LED_ENTENDIDO     17   // LED verde
#define PIN_LED_NO_ENTENDIDO  18   // LED rojo

struct Habitacion {
  const char* nombre;          // cómo se muestra
  int pin;                     // GPIO del LED
  const char* palabras[5];     // cómo se le puede decir (sin tildes, en minúscula)
};

const Habitacion HABITACIONES[] = {
  // nombre            pin   palabras que la identifican
  {"Recepción",        4,    {"recepcion", "entrada", "lobby"}},
  {"Oficina",          5,    {"oficina", "despacho", "escritorio"}},
  {"Sala de juntas",   6,    {"juntas", "reuniones", "reunion", "sala"}},
  {"Cocina",           7,    {"cocina", "cafeteria", "comedor"}},
  {"Baño",             15,   {"bano", "banos", "sanitario"}},
  {"Bodega",           16,   {"bodega", "almacen", "deposito"}},
};
const int NUM_HABITACIONES = sizeof(HABITACIONES) / sizeof(HABITACIONES[0]);

bool luzEncendida[NUM_HABITACIONES];   // estado actual de cada LED


// =============================================================================
// 3. VOCABULARIO (los "datos de entrenamiento" del asistente)
// -----------------------------------------------------------------------------
//  El asistente sabe lo que significa cada palabra de estas listas. Para que
//  entienda una forma nueva de hablar, basta con AGREGAR la palabra aquí.
//  Todas van en minúscula y sin tildes. La lista termina con nullptr.
// =============================================================================

const char* PALABRAS_ENCENDER[] = {
  "enciende", "encender", "encienda", "enciendan", "prende", "prender", "prenda", "prendan",
  "activa", "activar", "ilumina", "iluminar", nullptr
};

const char* PALABRAS_APAGAR[] = {
  "apaga", "apagar", "apague", "apaguen", "desactiva", "desactivar", "quita", "corta", nullptr
};

const char* PALABRAS_TODAS[] = {
  "todas", "todos", "todo", "toda", "general", nullptr
};

// Palabras que no aportan significado y se ignoran ("apaga LA luz DE LA cocina")
const char* PALABRAS_VACIAS[] = {
  "el", "la", "los", "las", "lo", "de", "del", "en", "y", "a", "al", "un", "una", "por", "favor",
  "porfa", "me", "se", "que", "puedes", "podrias", "quiero", "luz", "luces", "foco", "focos",
  "lampara", "lamparas", "ahora", "ya", "ok", "oye", "asistente", nullptr
};

// Porcentaje mínimo para aceptar que dos palabras "son la misma".
// 70 % tolera errores pequeños del reconocimiento de voz ("cosina" ≈ "cocina").
const int UMBRAL = 70;

// ---- Resultado del intérprete (se usa en la sección 5) ----------------------
// (Se declara aquí, antes de cualquier función, porque el Arduino IDE lo exige.)

struct Coincidencia {
  int porcentaje;    // 0 a 100
  String dicha;      // palabra que se captó (ej. "cosina")
  String conocida;   // palabra del vocabulario a la que se parece (ej. "cocina")
};

struct Analisis {
  String texto;                              // tal como llegó
  String normalizado;                        // sin tildes ni signos
  String palabrasClave;                      // las que se usaron para decidir
  Coincidencia encender, apagar, todas;
  Coincidencia lugar[NUM_HABITACIONES];
  int accion;                                // 1 = encender, 0 = apagar, -1 = no se sabe
  bool afectada[NUM_HABITACIONES];           // habitaciones a cambiar
  int confianza;                             // % general de la decisión
  bool entendido;
  String mensaje;
};


// =============================================================================
// 4. HERRAMIENTAS DE TEXTO
// =============================================================================

// Pasa a minúsculas, quita tildes y signos: "¡Apaga la Cocina!" -> "apaga la cocina"
String normalizar(String texto) {
  texto.toLowerCase();
  const char* conTilde[] = {"á", "é", "í", "ó", "ú", "ü", "ñ", "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ"};
  const char* sinTilde[] = {"a", "e", "i", "o", "u", "u", "n", "a", "e", "i", "o", "u", "u", "n"};
  for (int i = 0; i < 14; i++) texto.replace(conTilde[i], sinTilde[i]);

  String limpio = "";
  for (unsigned i = 0; i < texto.length(); i++) {
    char c = texto[i];
    bool letraONumero = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    limpio += letraONumero ? c : ' ';            // signos (¿?¡!,.) se vuelven espacios
  }
  while (limpio.indexOf("  ") >= 0) limpio.replace("  ", " ");
  limpio.trim();
  return limpio;
}

bool estaEnLista(const String& palabra, const char* lista[]) {
  for (int i = 0; lista[i] != nullptr; i++) if (palabra == lista[i]) return true;
  return false;
}

// Separa el texto en palabras y descarta las "vacías". Devuelve cuántas quedaron.
int separarPalabras(const String& texto, String palabras[], int maximo) {
  int cantidad = 0;
  int inicio = 0;
  while (inicio < (int)texto.length() && cantidad < maximo) {
    int fin = texto.indexOf(' ', inicio);
    if (fin < 0) fin = texto.length();
    String palabra = texto.substring(inicio, fin);
    if (palabra.length() > 0 && !estaEnLista(palabra, PALABRAS_VACIAS)) palabras[cantidad++] = palabra;
    inicio = fin + 1;
  }
  return cantidad;
}

// DISTANCIA DE LEVENSHTEIN: cuántas letras hay que cambiar, agregar o quitar
// para convertir una palabra en otra. "cosina" -> "cocina" = 1 cambio.
int distanciaLevenshtein(const String& a, const String& b) {
  const int MAX = 24;
  int n = min((int)a.length(), MAX), m = min((int)b.length(), MAX);
  int anterior[MAX + 1], actual[MAX + 1];
  for (int j = 0; j <= m; j++) anterior[j] = j;
  for (int i = 1; i <= n; i++) {
    actual[0] = i;
    for (int j = 1; j <= m; j++) {
      int costo = (a[i - 1] == b[j - 1]) ? 0 : 1;
      actual[j] = min(min(anterior[j] + 1, actual[j - 1] + 1), anterior[j - 1] + costo);
    }
    for (int j = 0; j <= m; j++) anterior[j] = actual[j];
  }
  return anterior[m];
}

// PORCENTAJE DE PARECIDO entre dos palabras (100 % = idénticas).
// porcentaje = (1 - cambios / largo de la palabra más larga) x 100
int parecido(const String& a, const String& b) {
  int largo = max(a.length(), b.length());
  if (largo == 0) return 0;
  return (int)(100.0 * (1.0 - (float)distanciaLevenshtein(a, b) / largo) + 0.5);
}

// Busca, entre las palabras dichas, la que más se parece a alguna palabra de
// la lista. Devuelve ese porcentaje y cuáles palabras coincidieron.
int mejorParecido(String palabras[], int cantidad, const char* lista[], String& dicha, String& conocida) {
  int mejor = 0;
  dicha = "";
  conocida = "";
  for (int i = 0; i < cantidad; i++) {
    for (int j = 0; lista[j] != nullptr; j++) {
      int p = parecido(palabras[i], lista[j]);
      if (p > mejor) { mejor = p; dicha = palabras[i]; conocida = lista[j]; }
    }
  }
  return mejor;
}


// =============================================================================
// 5. EL INTÉRPRETE: de texto a decisión, con porcentajes
// -----------------------------------------------------------------------------
//  Pasos:
//   a) Normalizar el texto y quedarse con las palabras importantes.
//   b) Calcular % de parecido con "encender", "apagar", "todas" y cada habitación.
//   c) Decidir: la acción con mayor % (si supera el UMBRAL) y los lugares que
//      superen el UMBRAL. "todas" -> todas las habitaciones.
// =============================================================================

Analisis interpretar(const String& texto) {
  Analisis r;
  r.texto = texto;

  // a) Limpiar y separar palabras
  r.normalizado = normalizar(texto);
  String palabras[20];
  int cantidad = separarPalabras(r.normalizado, palabras, 20);
  r.palabrasClave = "";
  for (int i = 0; i < cantidad; i++) r.palabrasClave += (i ? " " : "") + palabras[i];

  // b) Porcentajes de parecido
  r.encender.porcentaje = mejorParecido(palabras, cantidad, PALABRAS_ENCENDER, r.encender.dicha, r.encender.conocida);
  r.apagar.porcentaje   = mejorParecido(palabras, cantidad, PALABRAS_APAGAR,   r.apagar.dicha,   r.apagar.conocida);
  r.todas.porcentaje    = mejorParecido(palabras, cantidad, PALABRAS_TODAS,    r.todas.dicha,    r.todas.conocida);
  for (int h = 0; h < NUM_HABITACIONES; h++) {
    const char* lista[6];
    int k = 0;
    while (k < 5 && HABITACIONES[h].palabras[k] != nullptr) { lista[k] = HABITACIONES[h].palabras[k]; k++; }
    lista[k] = nullptr;
    r.lugar[h].porcentaje = mejorParecido(palabras, cantidad, lista, r.lugar[h].dicha, r.lugar[h].conocida);
  }

  // c1) ¿Qué hacer? La acción con mayor porcentaje, si supera el umbral
  int pctAccion = max(r.encender.porcentaje, r.apagar.porcentaje);
  if (pctAccion >= UMBRAL && r.encender.porcentaje != r.apagar.porcentaje)
    r.accion = (r.encender.porcentaje > r.apagar.porcentaje) ? 1 : 0;
  else
    r.accion = -1;

  // c2) ¿Dónde? "todas" o las habitaciones que superen el umbral
  int cuantas = 0, sumaPct = 0;
  bool todas = r.todas.porcentaje >= UMBRAL;
  // "apaga las luces" (plural, sin decir dónde) también significa todas
  bool dijoLuces = (" " + r.normalizado + " ").indexOf(" luces ") >= 0;
  for (int h = 0; h < NUM_HABITACIONES; h++) {
    r.afectada[h] = !todas && r.lugar[h].porcentaje >= UMBRAL;
    if (r.afectada[h]) { cuantas++; sumaPct += r.lugar[h].porcentaje; }
  }
  if (!todas && cuantas == 0 && dijoLuces) todas = true;
  if (todas) {
    for (int h = 0; h < NUM_HABITACIONES; h++) r.afectada[h] = true;
    cuantas = NUM_HABITACIONES;
    sumaPct = (r.todas.porcentaje >= UMBRAL ? r.todas.porcentaje : 100) * NUM_HABITACIONES;
  }

  // c3) Decisión final y confianza = promedio entre el % de la acción y el de los lugares
  int pctLugar = cuantas > 0 ? sumaPct / cuantas : 0;
  r.confianza = (pctAccion + pctLugar) / 2;
  r.entendido = r.accion != -1 && cuantas > 0;

  if (cantidad == 0) {
    r.mensaje = "No escuché ninguna palabra importante. Intenta: \"apaga la luz de la cocina\".";
  } else if (r.accion == -1 && cuantas == 0) {
    r.mensaje = "No entendí la orden. Di qué hacer (encender/apagar) y dónde.";
  } else if (r.accion == -1) {
    r.mensaje = "Entendí el lugar, pero no si quieres encender o apagar.";
  } else if (cuantas == 0) {
    r.mensaje = String("Entendí que quieres ") + (r.accion ? "encender" : "apagar") + ", pero no en qué lugar.";
  } else {
    String lista = "";
    for (int h = 0; h < NUM_HABITACIONES; h++) {
      if (!r.afectada[h]) continue;
      if (lista.length()) lista += ", ";
      lista += HABITACIONES[h].nombre;
    }
    r.mensaje = String("Listo: ") + (r.accion ? "encendí" : "apagué") + " la luz de: " + (todas ? "TODAS las habitaciones" : lista);
  }
  return r;
}


// =============================================================================
// 6. EJECUTAR LA DECISIÓN (LED)
// =============================================================================

// Parpadeo del LED de respuesta (verde o rojo) sin usar delay()
int pinParpadeo = -1;
int cambiosRestantes = 0;
unsigned long ultimoCambio = 0;

void avisarConLed(bool entendido) {
  digitalWrite(PIN_LED_ENTENDIDO, LOW);
  digitalWrite(PIN_LED_NO_ENTENDIDO, LOW);
  pinParpadeo = entendido ? PIN_LED_ENTENDIDO : PIN_LED_NO_ENTENDIDO;
  cambiosRestantes = 6;              // 3 parpadeos = 6 cambios (encender/apagar)
  ultimoCambio = 0;
}

void actualizarParpadeo() {
  if (cambiosRestantes == 0 || millis() - ultimoCambio < 150) return;
  ultimoCambio = millis();
  digitalWrite(pinParpadeo, cambiosRestantes % 2 == 0 ? HIGH : LOW);
  cambiosRestantes--;
}

void cambiarLuz(int h, bool encender) {
  digitalWrite(HABITACIONES[h].pin, encender ? HIGH : LOW);
  luzEncendida[h] = encender;
}

// Interpreta el texto y, si lo entendió, cambia los LED.
Analisis procesarOrden(const String& texto) {
  Analisis r = interpretar(texto);
  if (r.entendido) {
    for (int h = 0; h < NUM_HABITACIONES; h++) {
      if (r.afectada[h]) cambiarLuz(h, r.accion == 1);
    }
  }
  avisarConLed(r.entendido);
  return r;
}


// =============================================================================
// 7. MONITOR SERIE (probar sin página web ni Wi-Fi)
// -----------------------------------------------------------------------------
//  Escribe una orden en el Monitor Serie y pulsa Enter. Se muestra el
//  análisis completo con todos los porcentajes.
// =============================================================================

String barra(int porcentaje) {           // "████████░░" para ver el % de un vistazo
  String b = "";
  for (int i = 0; i < 10; i++) b += (i < porcentaje / 10) ? "#" : ".";
  return b;
}

void imprimirAnalisis(const Analisis& r) {
  Serial.println();
  Serial.println("==================================================");
  Serial.println("Orden recibida : \"" + r.texto + "\"");
  Serial.println("Palabras clave : " + r.palabrasClave);
  Serial.println("--------------------------------------------------");
  Serial.printf("ENCENDER      %s %3d%%  %s\n", barra(r.encender.porcentaje).c_str(), r.encender.porcentaje,
                r.encender.dicha.length() ? (r.encender.dicha + " ~ " + r.encender.conocida).c_str() : "");
  Serial.printf("APAGAR        %s %3d%%  %s\n", barra(r.apagar.porcentaje).c_str(), r.apagar.porcentaje,
                r.apagar.dicha.length() ? (r.apagar.dicha + " ~ " + r.apagar.conocida).c_str() : "");
  Serial.printf("TODAS         %s %3d%%  %s\n", barra(r.todas.porcentaje).c_str(), r.todas.porcentaje,
                r.todas.dicha.length() ? (r.todas.dicha + " ~ " + r.todas.conocida).c_str() : "");
  for (int h = 0; h < NUM_HABITACIONES; h++) {
    const Coincidencia& c = r.lugar[h];
    Serial.printf("%-14s%s %3d%%  %s\n", HABITACIONES[h].nombre, barra(c.porcentaje).c_str(), c.porcentaje,
                  c.dicha.length() ? (c.dicha + " ~ " + c.conocida).c_str() : "");
  }
  Serial.println("--------------------------------------------------");
  Serial.printf("Confianza: %d%%  (umbral para aceptar: %d%%)\n", r.confianza, UMBRAL);
  Serial.println((r.entendido ? "OK  " : "??  ") + r.mensaje);
  Serial.println("==================================================");
}

void revisarMonitorSerie() {
  if (!Serial.available()) return;
  String texto = Serial.readStringUntil('\n');
  texto.trim();
  if (texto.length() == 0) return;
  imprimirAnalisis(procesarOrden(texto));
}


// =============================================================================
// 8. PÁGINA WEB (HTML + CSS + JavaScript)
// -----------------------------------------------------------------------------
//  - Botón de micrófono: el navegador convierte la voz en texto (Web Speech API)
//    y lo envía al ESP32. El ESP32 hace TODA la interpretación.
//  - Plano de la empresa: cada habitación se ilumina si su LED está encendido.
//  - Tabla de porcentajes: qué tanto se pareció cada palabra captada.
//  Nota: en el JavaScript las funciones se escriben "const f = () => {}" porque
//  el Arduino IDE confundiría "function f() {}" con una función de C++.
// =============================================================================

const char PAGINA_WEB[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Asistente de voz ESP32</title>
<style>
  body { font-family: Arial, sans-serif; background: #eef1f5; margin: 0; padding: 16px; color: #222; }
  .contenedor { max-width: 900px; margin: auto; }
  h1 { font-size: 22px; margin: 0 0 12px; }
  .caja { background: white; border-radius: 12px; padding: 16px; margin-bottom: 14px; box-shadow: 0 1px 3px #0002; }
  h2 { font-size: 16px; margin: 0 0 10px; }
  #microfono { width: 90px; height: 90px; border-radius: 50%; border: none; font-size: 40px; cursor: pointer;
               background: #0b7a75; color: white; display: block; margin: 0 auto 10px; }
  #microfono.escuchando { background: #c62828; animation: pulso 1s infinite; }
  @keyframes pulso { 50% { transform: scale(1.08); } }
  .centro { text-align: center; color: #555; font-size: 14px; }
  .fila { display: flex; gap: 8px; margin-top: 10px; }
  input { flex: 1; font-size: 16px; padding: 10px; border: 1px solid #ccc; border-radius: 8px; }
  button.enviar { padding: 10px 16px; border: none; border-radius: 8px; background: #0b7a75; color: white; font-size: 15px; cursor: pointer; }
  .plano { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
  .cuarto { border: 3px solid #555; border-radius: 6px; padding: 18px 6px; text-align: center; background: #2b2f36;
            color: #aaa; cursor: pointer; transition: all .3s; user-select: none; }
  .cuarto.on { background: #ffe066; color: #333; box-shadow: 0 0 18px #ffd43b; }
  .cuarto b { display: block; font-size: 15px; } .cuarto small { font-size: 12px; }
  table { width: 100%; border-collapse: collapse; font-size: 14px; }
  td { padding: 5px 4px; border-bottom: 1px solid #eee; }
  .barra { background: #e5e7eb; border-radius: 4px; height: 14px; width: 100%; }
  .relleno { height: 14px; border-radius: 4px; background: #9ca3af; }
  .relleno.ok { background: #16a34a; }
  #resultado { font-size: 17px; font-weight: bold; margin-bottom: 8px; }
  .verde { color: #15803d; } .rojo { color: #b91c1c; }
  .ejemplo { display: inline-block; margin: 6px 6px 0 0; padding: 5px 10px; border-radius: 14px; background: #e3eeed;
             color: #0b5a56; font-size: 13px; cursor: pointer; }
</style>
</head>
<body>
<div class="contenedor">
  <h1>🎙️ Asistente de voz — Luces de la empresa</h1>

  <div class="caja">
    <button id="microfono" onclick="escuchar()">🎤</button>
    <div class="centro" id="estadoMic">Pulsa el micrófono y di, por ejemplo: "apaga la luz de la cocina"</div>
    <div class="fila">
      <input id="texto" placeholder="...o escribe la orden aquí" onkeydown="if (event.key === 'Enter') enviar()">
      <button class="enviar" onclick="enviar()">Enviar</button>
    </div>
    <div id="ejemplos"></div>
  </div>

  <div class="caja">
    <h2>🏢 Plano de la empresa (toca una habitación para encenderla/apagarla)</h2>
    <div class="plano" id="plano"></div>
  </div>

  <div class="caja">
    <h2>🧠 Lo que entendió el asistente</h2>
    <div id="resultado">Aún no hay órdenes.</div>
    <div id="captado" class="centro" style="text-align:left"></div>
    <table id="tabla"></table>
  </div>
</div>

<script>
const UMBRAL = 70;
const EJEMPLOS = ["apaga la luz de la cocina", "enciende la oficina y la recepción", "prende todas las luces",
                  "apaga las luces", "enciende la luz del baño", "apaga la cosina"];

for (const e of EJEMPLOS) {
  const s = document.createElement("span");
  s.className = "ejemplo"; s.textContent = e;
  s.onclick = () => { document.getElementById("texto").value = e; enviar(); };
  document.getElementById("ejemplos").appendChild(s);
}

// ---------- Voz a texto (lo hace el navegador) ----------
const Reconocimiento = window.SpeechRecognition || window.webkitSpeechRecognition;
let mic = null;
if (Reconocimiento) {
  mic = new Reconocimiento();
  mic.lang = "es-ES";
  mic.interimResults = false;
  mic.onresult = (e) => {
    const dicho = e.results[0][0].transcript;
    document.getElementById("texto").value = dicho;
    document.getElementById("estadoMic").textContent = "Escuché: \"" + dicho + "\"";
    enviar();
  };
  mic.onerror = (e) => {
    document.getElementById("estadoMic").textContent = e.error === "not-allowed"
      ? "El navegador bloqueó el micrófono. Revisa el paso del tutorial para permitirlo, o usa el dictado del teclado."
      : "Error del micrófono: " + e.error;
  };
  mic.onend = () => document.getElementById("microfono").classList.remove("escuchando");
} else {
  document.getElementById("estadoMic").textContent =
    "Este navegador no reconoce voz. Usa Google Chrome, o el micrófono del teclado del celular.";
}

const escuchar = () => {
  if (!mic) return;
  document.getElementById("microfono").classList.add("escuchando");
  document.getElementById("estadoMic").textContent = "Escuchando... habla ahora";
  mic.start();
};

// ---------- Enviar la orden al ESP32 ----------
const enviar = async () => {
  const texto = document.getElementById("texto").value.trim();
  if (!texto) return;
  const r = await fetch("/api/comando", { method: "POST", body: JSON.stringify({ texto }) });
  mostrar(await r.json());
  actualizar();
};

const fila = (nombre, c) => {
  const ok = c.porcentaje >= UMBRAL;
  const palabras = c.dicha ? `"${c.dicha}" ≈ "${c.conocida}"` : "";
  return `<tr><td style="width:28%">${nombre}</td>
    <td style="width:35%"><div class="barra"><div class="relleno ${ok ? "ok" : ""}" style="width:${c.porcentaje}%"></div></div></td>
    <td style="width:10%"><b>${c.porcentaje}%</b></td><td style="color:#666">${palabras}</td></tr>`;
};

const mostrar = (d) => {
  const res = document.getElementById("resultado");
  res.className = d.entendido ? "verde" : "rojo";
  res.textContent = (d.entendido ? "✅ " : "❓ ") + d.mensaje + "  (confianza " + d.confianza + "%)";
  document.getElementById("captado").textContent =
    "Texto: \"" + d.texto + "\"  →  palabras clave: " + (d.palabras_clave || "(ninguna)");
  let t = "<tr><td colspan=4><b>¿Qué hacer?</b></td></tr>" + fila("Encender", d.encender) + fila("Apagar", d.apagar);
  t += "<tr><td colspan=4><b>¿Dónde?</b> (verde = supera el " + UMBRAL + "%)</td></tr>" + fila("Todas", d.todas);
  for (const l of d.lugares) t += fila(l.nombre, l);
  document.getElementById("tabla").innerHTML = t;
};

// ---------- Plano de la empresa ----------
const actualizar = async () => {
  const e = await (await fetch("/api/estado")).json();
  document.getElementById("plano").innerHTML = e.luces.map(l =>
    `<div class="cuarto ${l.encendida ? "on" : ""}" onclick="tocar('${l.nombre}', ${l.encendida})">
       <b>${l.encendida ? "💡" : "🌑"} ${l.nombre}</b><small>GPIO ${l.pin}</small></div>`).join("");
};

// Tocar una habitación envía la orden en texto, así también pasa por el intérprete
const tocar = (nombre, encendida) => {
  document.getElementById("texto").value = (encendida ? "apaga" : "enciende") + " la luz de " + nombre;
  enviar();
};

actualizar();
setInterval(actualizar, 2000);
</script>
</body>
</html>
)HTML";


// =============================================================================
// 9. SERVIDOR WEB Y API
// -----------------------------------------------------------------------------
//   GET  /              página web
//   GET  /api/estado    estado de cada luz
//   POST /api/comando   {"texto": "apaga la luz de la cocina"} -> análisis completo
//  Detalle en docs/API.md
// =============================================================================

WebServer servidor(80);

void agregarCoincidencia(JsonObject o, const Coincidencia& c) {
  o["porcentaje"] = c.porcentaje;
  o["dicha"] = c.dicha;
  o["conocida"] = c.conocida;
}

void rutaPagina() { servidor.send_P(200, "text/html; charset=utf-8", PAGINA_WEB); }

void rutaEstado() {
  JsonDocument doc;
  JsonArray luces = doc["luces"].to<JsonArray>();
  for (int h = 0; h < NUM_HABITACIONES; h++) {
    JsonObject l = luces.add<JsonObject>();
    l["nombre"] = HABITACIONES[h].nombre;
    l["pin"] = HABITACIONES[h].pin;
    l["encendida"] = luzEncendida[h];
  }
  String texto;
  serializeJson(doc, texto);
  servidor.send(200, "application/json; charset=utf-8", texto);
}

void rutaComando() {
  JsonDocument cuerpo;
  if (deserializeJson(cuerpo, servidor.arg("plain")) || !cuerpo["texto"].is<const char*>()) {
    servidor.send(400, "application/json; charset=utf-8", "{\"error\":\"Envía un JSON como {\\\"texto\\\": \\\"apaga la cocina\\\"}\"}");
    return;
  }
  Analisis r = procesarOrden(cuerpo["texto"].as<const char*>());
  imprimirAnalisis(r);   // también se ve en el Monitor Serie

  JsonDocument doc;
  doc["texto"] = r.texto;
  doc["normalizado"] = r.normalizado;
  doc["palabras_clave"] = r.palabrasClave;
  doc["entendido"] = r.entendido;
  doc["accion"] = r.accion == 1 ? "encender" : (r.accion == 0 ? "apagar" : "desconocida");
  doc["confianza"] = r.confianza;
  doc["umbral"] = UMBRAL;
  doc["mensaje"] = r.mensaje;
  agregarCoincidencia(doc["encender"].to<JsonObject>(), r.encender);
  agregarCoincidencia(doc["apagar"].to<JsonObject>(), r.apagar);
  agregarCoincidencia(doc["todas"].to<JsonObject>(), r.todas);
  JsonArray lugares = doc["lugares"].to<JsonArray>();
  JsonArray afectadas = doc["afectadas"].to<JsonArray>();
  for (int h = 0; h < NUM_HABITACIONES; h++) {
    JsonObject l = lugares.add<JsonObject>();
    l["nombre"] = HABITACIONES[h].nombre;
    agregarCoincidencia(l, r.lugar[h]);
    if (r.entendido && r.afectada[h]) afectadas.add(HABITACIONES[h].nombre);
  }
  String texto;
  serializeJson(doc, texto);
  servidor.send(200, "application/json; charset=utf-8", texto);
}


// =============================================================================
// 10. setup() y loop()
// =============================================================================

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
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(RED_RESPALDO, CLAVE_RESPALDO);
    Serial.println("Sin Wi-Fi. Conéctate a la red '" RED_RESPALDO "' (clave " CLAVE_RESPALDO ") y abre http://192.168.4.1");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Asistente de voz ESP32-S3 ===");

  for (int h = 0; h < NUM_HABITACIONES; h++) {
    pinMode(HABITACIONES[h].pin, OUTPUT);
    cambiarLuz(h, false);                       // al arrancar todo apagado
  }
  pinMode(PIN_LED_ENTENDIDO, OUTPUT);
  pinMode(PIN_LED_NO_ENTENDIDO, OUTPUT);

  conectarWiFi();
  servidor.on("/", HTTP_GET, rutaPagina);
  servidor.on("/api/estado", HTTP_GET, rutaEstado);
  servidor.on("/api/comando", HTTP_POST, rutaComando);
  servidor.begin();

  Serial.println("Listo. Escribe una orden aquí (ej: apaga la luz de la cocina) y pulsa Enter.");
}

void loop() {
  servidor.handleClient();    // página web y API
  revisarMonitorSerie();      // órdenes escritas en el Monitor Serie
  actualizarParpadeo();       // LED verde / rojo de respuesta
}

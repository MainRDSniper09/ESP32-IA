/*
 * =============================================================================
 *   ASISTENTE DE VOZ PARA LAS LUCES DE UNA EMPRESA  —  PARTE 1
 *   ESP32-S3  ·  C++ (Arduino IDE)  ·  Algoritmo TF-IDF dentro del propio ESP32
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
 *  2. El ESP32 INTERPRETA ese texto él mismo (sin internet, sin IA externa)
 *     con el algoritmo TF-IDF:
 *       - Tiene un CORPUS: una lista de frases de ejemplo, cada una con su
 *         significado ("apaga la luz" = apagar, "luz de la cocina" = cocina...).
 *       - Convierte cada frase, y también la orden recibida, en una lista de
 *         números (un VECTOR TF-IDF): cuánto pesa cada palabra.
 *       - Compara la orden con cada frase del corpus (SIMILITUD COSENO) y
 *         obtiene un PORCENTAJE de similitud (0 % = nada que ver, 100 % = igual).
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
 *    1. Wi-Fi                          7. Encender / apagar los LED
 *    2. Habitaciones y pines           8. Monitor Serie
 *    3. Corpus de entrenamiento        9. Página web
 *    4. Preparar el texto             10. Servidor web y API
 *    5. El algoritmo TF-IDF           11. setup() y loop()
 *    6. El intérprete (decisión)
 *
 *  Licencia MIT · Juan Nicolás Barreto Sánchez · 2026
 * =============================================================================
 */

#include <WiFi.h>         // conexión Wi-Fi
#include <WebServer.h>    // página web y API
#include <ArduinoJson.h>  // respuestas en formato JSON
#include <math.h>         // log() y sqrt() para TF-IDF


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
//  Cada habitación tiene: el nombre que se muestra, su "intención" (la etiqueta
//  con la que aparece en el corpus de la sección 3) y el pin de su LED.
// =============================================================================

#define PIN_LED_ENTENDIDO     17   // LED verde
#define PIN_LED_NO_ENTENDIDO  18   // LED rojo

struct Habitacion {
  const char* nombre;      // cómo se muestra en pantalla
  const char* intencion;   // etiqueta usada en el corpus (sección 3)
  int pin;                 // GPIO del LED
};

const Habitacion HABITACIONES[] = {
  // nombre            intención       pin
  {"Recepción",        "recepcion",    4},
  {"Oficina",          "oficina",      5},
  {"Sala de juntas",   "juntas",       6},
  {"Cocina",           "cocina",       7},
  {"Baño",             "bano",         15},
  {"Bodega",           "bodega",       16},
};
const int NUM_HABITACIONES = sizeof(HABITACIONES) / sizeof(HABITACIONES[0]);

bool luzEncendida[NUM_HABITACIONES];   // estado actual de cada LED


// =============================================================================
// 3. CORPUS DE ENTRENAMIENTO
// -----------------------------------------------------------------------------
//  En TF-IDF cada frase de ejemplo se llama DOCUMENTO, y el conjunto de todas
//  se llama CORPUS. Cada documento tiene:
//     - frase:      un ejemplo de cómo habla el usuario.
//     - intencion:  lo que significa. Puede ser "encender", "apagar", "todas"
//                   o la intención de una habitación (columna 2 de la sección 2).
//
//  Para que el asistente entienda una forma nueva de hablar, basta con
//  AGREGAR una fila aquí. Se pueden escribir con tildes y mayúsculas: el
//  programa las limpia antes de usarlas.
// =============================================================================

struct Documento {
  const char* frase;
  const char* intencion;
};

const Documento CORPUS[] = {
  // ---------- ENCENDER ----------
  {"enciende la luz",            "encender"},
  {"encender la luz",            "encender"},
  {"encienda la luz",            "encender"},
  {"prende la luz",              "encender"},
  {"prender la luz",             "encender"},
  {"prenda la luz",              "encender"},
  {"activa la luz",              "encender"},
  {"ilumina",                    "encender"},

  // ---------- APAGAR ----------
  {"apaga la luz",               "apagar"},
  {"apagar la luz",              "apagar"},
  {"apague la luz",              "apagar"},
  {"desactiva la luz",           "apagar"},
  {"quita la luz",               "apagar"},
  {"corta la luz",               "apagar"},

  // ---------- TODAS LAS HABITACIONES ----------
  {"todas las luces",            "todas"},
  {"las luces",                  "todas"},
  {"todas las habitaciones",     "todas"},
  {"todo el piso",               "todas"},
  {"toda la empresa",            "todas"},
  {"general",                    "todas"},

  // ---------- RECEPCIÓN ----------
  {"luz de la recepción",        "recepcion"},
  {"recepción",                  "recepcion"},
  {"entrada",                    "recepcion"},
  {"lobby",                      "recepcion"},

  // ---------- OFICINA ----------
  {"luz de la oficina",          "oficina"},
  {"oficina",                    "oficina"},
  {"despacho",                   "oficina"},
  {"escritorio",                 "oficina"},

  // ---------- SALA DE JUNTAS ----------
  {"luz de la sala de juntas",   "juntas"},
  {"sala de juntas",             "juntas"},
  {"sala de reuniones",          "juntas"},
  {"reunión",                    "juntas"},

  // ---------- COCINA ----------
  {"luz de la cocina",           "cocina"},
  {"cocina",                     "cocina"},
  {"cafetería",                  "cocina"},
  {"comedor",                    "cocina"},

  // ---------- BAÑO ----------
  {"luz del baño",               "bano"},
  {"baño",                       "bano"},
  {"baños",                      "bano"},
  {"sanitario",                  "bano"},

  // ---------- BODEGA ----------
  {"luz de la bodega",           "bodega"},
  {"bodega",                     "bodega"},
  {"almacén",                    "bodega"},
  {"depósito",                   "bodega"},
};
const int NUM_DOCUMENTOS = sizeof(CORPUS) / sizeof(CORPUS[0]);

// PALABRAS VACÍAS: palabras muy comunes que no aportan significado
// ("apaga LA luz DE LA cocina"). Se eliminan antes de aplicar TF-IDF.
// La lista termina con nullptr.
const char* PALABRAS_VACIAS[] = {
  "el", "la", "los", "las", "lo", "de", "del", "en", "y", "o", "a", "al", "un", "una",
  "por", "favor", "porfa", "me", "mi", "se", "que", "con", "para", "puedes", "podrias",
  "quiero", "ahora", "ya", "ok", "oye", "hola", "asistente", nullptr
};

// Porcentaje mínimo de similitud para aceptar que la orden "habla de" una
// intención. Con TF-IDF los porcentajes bajan cuando la orden tiene muchas
// palabras (ej. "enciende la cocina y la oficina"), por eso el umbral es 30 %.
const int UMBRAL = 30;

// Tamaños máximos (la ESP32 tiene memoria limitada)
const int MAX_VOCABULARIO = 150;   // palabras distintas en todo el corpus
const int MAX_PALABRAS    = 20;    // palabras en una sola frase u orden


// ---- Tipos que usa el intérprete -------------------------------------------
// (Se declaran aquí, antes de cualquier función, porque el Arduino IDE lo exige.)

// Una fila de la tabla TF-IDF de la orden recibida
struct PesoPalabra {
  String palabra;
  float tf = 0;           // frecuencia en la orden
  float idf = 0;          // qué tan rara es en el corpus
  float tfidf = 0;        // tf x idf = peso final
  bool conocida = false;  // ¿aparece en el corpus?
};

// La frase del corpus más parecida a la orden, para una intención
struct Coincidencia {
  int porcentaje = 0;     // similitud coseno x 100
  String frase;           // frase del corpus que dio ese porcentaje
};

// Todo lo que el intérprete calculó y decidió sobre una orden
struct Analisis {
  String texto;                              // tal como llegó
  String normalizado;                        // sin tildes ni signos
  PesoPalabra pesos[MAX_PALABRAS];           // tabla TF-IDF de la orden
  int numPesos = 0;
  Coincidencia encender, apagar, todas;      // % de cada intención
  Coincidencia lugar[NUM_HABITACIONES];
  int accion = -1;                           // 1 = encender, 0 = apagar, -1 = no se sabe
  bool afectada[NUM_HABITACIONES];           // habitaciones a cambiar
  bool todasLasHabitaciones = false;
  int confianza = 0;                         // % general de la decisión
  bool entendido = false;
  String mensaje;
};


// =============================================================================
// 4. PREPARAR EL TEXTO
// -----------------------------------------------------------------------------
//  Antes de aplicar TF-IDF, el texto se limpia y se parte en palabras:
//     "¡Apaga la Luz de la Cocina!"  ->  [apaga, luz, cocina]
// =============================================================================

// Pasa a minúsculas, quita tildes y signos:
//   "¡Apaga la Cocina!" -> "apaga la cocina"
String normalizar(String texto) {
  texto.toLowerCase();

  // Cambiar letras con tilde por letras sin tilde
  const char* conTilde[] = {"á", "é", "í", "ó", "ú", "ü", "ñ", "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ"};
  const char* sinTilde[] = {"a", "e", "i", "o", "u", "u", "n", "a", "e", "i", "o", "u", "u", "n"};
  for (int i = 0; i < 14; i++) {
    texto.replace(conTilde[i], sinTilde[i]);
  }

  // Dejar solo letras y números; los signos (¿?¡!,.) se vuelven espacios
  String limpio = "";
  for (int i = 0; i < (int)texto.length(); i++) {
    char c = texto[i];
    bool esLetra  = (c >= 'a' && c <= 'z');
    bool esNumero = (c >= '0' && c <= '9');
    if (esLetra || esNumero) limpio += c;
    else                     limpio += ' ';
  }

  // Quitar espacios repetidos y los de los extremos
  while (limpio.indexOf("  ") >= 0) {
    limpio.replace("  ", " ");
  }
  limpio.trim();
  return limpio;
}

// ¿La palabra está en la lista de palabras vacías?
bool esPalabraVacia(const String& palabra) {
  for (int i = 0; PALABRAS_VACIAS[i] != nullptr; i++) {
    if (palabra == PALABRAS_VACIAS[i]) return true;
  }
  return false;
}

// Separa un texto YA normalizado en palabras, descartando las vacías.
// Guarda las palabras en el arreglo "palabras" y devuelve cuántas quedaron.
//   "apaga la luz de la cocina" -> palabras = [apaga, luz, cocina], devuelve 3
int separarPalabras(const String& texto, String palabras[]) {
  int cantidad = 0;
  int inicio = 0;
  while (inicio < (int)texto.length() && cantidad < MAX_PALABRAS) {
    int fin = texto.indexOf(' ', inicio);       // dónde termina esta palabra
    if (fin < 0) fin = texto.length();          // era la última palabra
    String palabra = texto.substring(inicio, fin);
    if (palabra.length() > 0 && !esPalabraVacia(palabra)) {
      palabras[cantidad] = palabra;
      cantidad++;
    }
    inicio = fin + 1;
  }
  return cantidad;
}

// Cuenta cuántas veces aparece "buscada" dentro de la lista de palabras.
int contarVeces(const String palabras[], int cantidad, const String& buscada) {
  int veces = 0;
  for (int i = 0; i < cantidad; i++) {
    if (palabras[i] == buscada) veces++;
  }
  return veces;
}


// =============================================================================
// 5. EL ALGORITMO TF-IDF
// -----------------------------------------------------------------------------
//  TF-IDF le da a cada palabra un PESO que dice qué tan importante es:
//
//   TF  (Term Frequency, frecuencia del término)
//       = veces que la palabra aparece en la frase / total de palabras de la frase
//       Una palabra que se repite en la frase pesa más.
//
//   IDF (Inverse Document Frequency, frecuencia inversa de documento)
//       = log( total de documentos / documentos que contienen la palabra )
//       Una palabra que aparece en MUCHAS frases del corpus (ej. "luz") pesa
//       poco porque no ayuda a distinguir. Una palabra RARA (ej. "cocina")
//       pesa mucho porque identifica muy bien de qué se habla.
//
//   TF-IDF = TF x IDF
//
//  Cada frase se convierte en un VECTOR: una lista con un número por cada
//  palabra del vocabulario (0 si la palabra no está en la frase).
//
//  Para comparar dos vectores se usa la SIMILITUD COSENO:
//       coseno = (A · B) / (|A| x |B|)
//  Da 1 (100 %) si las frases usan las mismas palabras con los mismos pesos,
//  y 0 (0 %) si no comparten ninguna palabra.
//
//  ENTRENAR el modelo = calcular el vocabulario, el IDF y los vectores de todo
//  el corpus. Se hace una sola vez, al encender la ESP32 (en setup()).
// =============================================================================

String vocabulario[MAX_VOCABULARIO];       // todas las palabras distintas del corpus
int    tamanoVocabulario = 0;              // cuántas hay
int    documentosConPalabra[MAX_VOCABULARIO];   // en cuántos documentos aparece cada una
float  idf[MAX_VOCABULARIO];               // IDF de cada palabra
float  vectoresCorpus[NUM_DOCUMENTOS][MAX_VOCABULARIO];   // vector TF-IDF de cada documento

// Devuelve la posición de la palabra en el vocabulario, o -1 si no está.
int buscarEnVocabulario(const String& palabra) {
  for (int i = 0; i < tamanoVocabulario; i++) {
    if (vocabulario[i] == palabra) return i;
  }
  return -1;
}

// PASO 1 DEL ENTRENAMIENTO: reunir todas las palabras distintas del corpus.
void construirVocabulario() {
  tamanoVocabulario = 0;
  for (int d = 0; d < NUM_DOCUMENTOS; d++) {
    String palabras[MAX_PALABRAS];
    int cantidad = separarPalabras(normalizar(CORPUS[d].frase), palabras);

    for (int i = 0; i < cantidad; i++) {
      bool esNueva = (buscarEnVocabulario(palabras[i]) == -1);
      if (esNueva && tamanoVocabulario < MAX_VOCABULARIO) {
        vocabulario[tamanoVocabulario] = palabras[i];
        tamanoVocabulario++;
      }
    }
  }
}

// ¿El documento número d contiene la palabra?
bool documentoContiene(int d, const String& palabra) {
  String palabras[MAX_PALABRAS];
  int cantidad = separarPalabras(normalizar(CORPUS[d].frase), palabras);
  return contarVeces(palabras, cantidad, palabra) > 0;
}

// PASO 2 DEL ENTRENAMIENTO: calcular el IDF de cada palabra del vocabulario.
//   IDF = log( total de documentos / documentos que contienen la palabra )
void calcularIDF() {
  for (int i = 0; i < tamanoVocabulario; i++) {
    int documentos = 0;
    for (int d = 0; d < NUM_DOCUMENTOS; d++) {
      if (documentoContiene(d, vocabulario[i])) documentos++;
    }
    documentosConPalabra[i] = documentos;
    idf[i] = log((float)NUM_DOCUMENTOS / documentos);
  }
}

// Convierte un texto en su vector TF-IDF (un número por palabra del vocabulario).
// Se usa igual para las frases del corpus y para la orden del usuario.
//   TF     = veces que aparece la palabra / total de palabras del texto
//   TF-IDF = TF x IDF
void calcularVectorTFIDF(const String& texto, float vector[]) {
  String palabras[MAX_PALABRAS];
  int total = separarPalabras(normalizar(texto), palabras);

  for (int i = 0; i < tamanoVocabulario; i++) {
    float tf = 0;
    if (total > 0) {
      tf = (float)contarVeces(palabras, total, vocabulario[i]) / total;
    }
    vector[i] = tf * idf[i];
  }
}

// SIMILITUD COSENO entre dos vectores TF-IDF: devuelve un valor de 0 a 1.
//   coseno = (A · B) / (|A| x |B|)
//   A · B = suma de A[i] x B[i]        (producto punto)
//   |A|   = raíz de la suma de A[i]²   (largo del vector)
float similitudCoseno(const float a[], const float b[]) {
  float productoPunto = 0;
  float sumaCuadradosA = 0;
  float sumaCuadradosB = 0;
  for (int i = 0; i < tamanoVocabulario; i++) {
    productoPunto  += a[i] * b[i];
    sumaCuadradosA += a[i] * a[i];
    sumaCuadradosB += b[i] * b[i];
  }
  if (sumaCuadradosA == 0 || sumaCuadradosB == 0) return 0;   // un vector vacío no se parece a nada
  return productoPunto / (sqrt(sumaCuadradosA) * sqrt(sumaCuadradosB));
}

// ENTRENAR: vocabulario -> IDF -> vector TF-IDF de cada documento del corpus.
void entrenarModelo() {
  construirVocabulario();
  calcularIDF();
  for (int d = 0; d < NUM_DOCUMENTOS; d++) {
    calcularVectorTFIDF(CORPUS[d].frase, vectoresCorpus[d]);
  }
}


// =============================================================================
// 6. EL INTÉRPRETE: de texto a decisión
// -----------------------------------------------------------------------------
//  Pasos:
//   a) Convertir la orden en su vector TF-IDF.
//   b) Compararla (similitud coseno) con CADA documento del corpus. Para cada
//      intención (encender, apagar, todas, cada habitación) se guarda el MAYOR
//      porcentaje que obtuvo alguna de sus frases.
//   c) Decidir:
//        - Acción: la de mayor % entre encender y apagar, si llega al UMBRAL.
//        - Lugar:  las habitaciones que lleguen al UMBRAL. Si no se nombró
//                  ninguna pero "todas" llega al UMBRAL -> todas.
// =============================================================================

// Llena la tabla con el TF, IDF y TF-IDF de cada palabra de la orden
// (solo para mostrarlo en pantalla; la decisión usa el vector completo).
void calcularTablaDePesos(Analisis& r) {
  String palabras[MAX_PALABRAS];
  int total = separarPalabras(r.normalizado, palabras);
  r.numPesos = 0;

  for (int i = 0; i < total; i++) {
    // Si la palabra se repite, ya está en la tabla: no se agrega dos veces
    bool repetida = false;
    for (int j = 0; j < r.numPesos; j++) {
      if (r.pesos[j].palabra == palabras[i]) repetida = true;
    }
    if (repetida) continue;

    int posicion = buscarEnVocabulario(palabras[i]);
    PesoPalabra& fila = r.pesos[r.numPesos];
    fila.palabra  = palabras[i];
    fila.conocida = (posicion != -1);
    fila.tf       = (float)contarVeces(palabras, total, palabras[i]) / total;
    fila.idf      = fila.conocida ? idf[posicion] : 0;   // palabra desconocida: no pesa
    fila.tfidf    = fila.tf * fila.idf;
    r.numPesos++;
  }
}

// Si el porcentaje nuevo es mayor que el guardado, lo reemplaza.
void guardarSiEsMayor(Coincidencia& c, int porcentaje, const char* frase) {
  if (porcentaje > c.porcentaje) {
    c.porcentaje = porcentaje;
    c.frase = frase;
  }
}

Analisis interpretar(const String& texto) {
  Analisis r;
  r.texto = texto;
  r.normalizado = normalizar(texto);
  calcularTablaDePesos(r);

  // a) Vector TF-IDF de la orden
  float vectorOrden[MAX_VOCABULARIO];
  calcularVectorTFIDF(texto, vectorOrden);

  // b) Similitud coseno con cada documento del corpus
  for (int d = 0; d < NUM_DOCUMENTOS; d++) {
    float coseno = similitudCoseno(vectorOrden, vectoresCorpus[d]);
    int porcentaje = round(coseno * 100);
    String intencion = CORPUS[d].intencion;

    if (intencion == "encender") guardarSiEsMayor(r.encender, porcentaje, CORPUS[d].frase);
    if (intencion == "apagar")   guardarSiEsMayor(r.apagar,   porcentaje, CORPUS[d].frase);
    if (intencion == "todas")    guardarSiEsMayor(r.todas,    porcentaje, CORPUS[d].frase);
    for (int h = 0; h < NUM_HABITACIONES; h++) {
      if (intencion == HABITACIONES[h].intencion) guardarSiEsMayor(r.lugar[h], porcentaje, CORPUS[d].frase);
    }
  }

  // c1) ¿Qué hacer? La acción con mayor porcentaje, si llega al umbral
  int pctAccion = max(r.encender.porcentaje, r.apagar.porcentaje);
  if (pctAccion >= UMBRAL && r.encender.porcentaje > r.apagar.porcentaje) r.accion = 1;
  else if (pctAccion >= UMBRAL && r.apagar.porcentaje > r.encender.porcentaje) r.accion = 0;
  else r.accion = -1;   // ninguna llega al umbral, o empatan

  // c2) ¿Dónde? Las habitaciones que lleguen al umbral
  int cuantas = 0;
  int sumaPorcentajes = 0;
  for (int h = 0; h < NUM_HABITACIONES; h++) {
    r.afectada[h] = (r.lugar[h].porcentaje >= UMBRAL);
    if (r.afectada[h]) {
      cuantas++;
      sumaPorcentajes += r.lugar[h].porcentaje;
    }
  }
  // Si no nombró ninguna habitación pero dijo "todas" / "las luces" -> todas
  if (cuantas == 0 && r.todas.porcentaje >= UMBRAL) {
    r.todasLasHabitaciones = true;
    for (int h = 0; h < NUM_HABITACIONES; h++) r.afectada[h] = true;
    cuantas = 1;                                  // cuenta como un solo lugar: "todas"
    sumaPorcentajes = r.todas.porcentaje;
  }

  // c3) Confianza = promedio entre el % de la acción y el % promedio de los lugares
  int pctLugar = (cuantas > 0) ? sumaPorcentajes / cuantas : 0;
  r.confianza = (pctAccion + pctLugar) / 2;
  r.entendido = (r.accion != -1 && cuantas > 0);

  // c4) Mensaje para el usuario
  if (r.numPesos == 0) {
    r.mensaje = "No escuché ninguna palabra importante. Intenta: \"apaga la luz de la cocina\".";
  } else if (r.accion == -1 && cuantas == 0) {
    r.mensaje = "No entendí la orden. Di qué hacer (encender/apagar) y dónde.";
  } else if (r.accion == -1) {
    r.mensaje = "Entendí el lugar, pero no si quieres encender o apagar.";
  } else if (cuantas == 0) {
    r.mensaje = String("Entendí que quieres ") + (r.accion == 1 ? "encender" : "apagar") + ", pero no en qué lugar.";
  } else {
    String lista = "";
    for (int h = 0; h < NUM_HABITACIONES; h++) {
      if (!r.afectada[h]) continue;
      if (lista.length() > 0) lista += ", ";
      lista += HABITACIONES[h].nombre;
    }
    if (r.todasLasHabitaciones) lista = "TODAS las habitaciones";
    r.mensaje = String("Listo: ") + (r.accion == 1 ? "encendí" : "apagué") + " la luz de: " + lista;
  }
  return r;
}


// =============================================================================
// 7. ENCENDER / APAGAR LOS LED
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
// 8. MONITOR SERIE (probar sin página web ni Wi-Fi)
// -----------------------------------------------------------------------------
//  Al encender se imprime el modelo entrenado (vocabulario e IDF).
//  Escribe una orden en el Monitor Serie y pulsa Enter: se muestra la tabla
//  TF-IDF de la orden y el % de similitud con cada intención.
// =============================================================================

String barra(int porcentaje) {           // "########.." para ver el % de un vistazo
  String b = "";
  for (int i = 0; i < 10; i++) b += (i < porcentaje / 10) ? "#" : ".";
  return b;
}

void imprimirModelo() {
  Serial.println();
  Serial.println("========== MODELO TF-IDF ENTRENADO ==========");
  Serial.printf("Documentos en el corpus: %d\n", NUM_DOCUMENTOS);
  Serial.printf("Palabras en el vocabulario: %d\n", tamanoVocabulario);
  Serial.println("palabra           docs   IDF = log(N/docs)");
  for (int i = 0; i < tamanoVocabulario; i++) {
    Serial.printf("%-16s  %3d    %.3f\n", vocabulario[i].c_str(), documentosConPalabra[i], idf[i]);
  }
  Serial.println("=============================================");
}

void imprimirCoincidencia(const char* nombre, const Coincidencia& c) {
  Serial.printf("%-15s %s %3d%%  %s\n", nombre, barra(c.porcentaje).c_str(), c.porcentaje,
                c.porcentaje > 0 ? ("~ \"" + c.frase + "\"").c_str() : "");
}

void imprimirAnalisis(const Analisis& r) {
  Serial.println();
  Serial.println("==================================================");
  Serial.println("Orden recibida : \"" + r.texto + "\"");
  Serial.println("Normalizada    : \"" + r.normalizado + "\"");
  Serial.println("--------------------------------------------------");
  Serial.println("palabra            TF      IDF     TF-IDF");
  for (int i = 0; i < r.numPesos; i++) {
    const PesoPalabra& p = r.pesos[i];
    if (p.conocida) Serial.printf("%-16s  %.3f   %.3f   %.3f\n", p.palabra.c_str(), p.tf, p.idf, p.tfidf);
    else            Serial.printf("%-16s  %.3f   (no está en el corpus, se ignora)\n", p.palabra.c_str(), p.tf);
  }
  Serial.println("--------------------------------------------------");
  Serial.println("Similitud coseno con cada intención:");
  imprimirCoincidencia("ENCENDER", r.encender);
  imprimirCoincidencia("APAGAR", r.apagar);
  imprimirCoincidencia("TODAS", r.todas);
  for (int h = 0; h < NUM_HABITACIONES; h++) imprimirCoincidencia(HABITACIONES[h].nombre, r.lugar[h]);
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
// 9. PÁGINA WEB (HTML + CSS + JavaScript)
// -----------------------------------------------------------------------------
//  - Botón de micrófono: el navegador convierte la voz en texto (Web Speech API)
//    y lo envía al ESP32. El ESP32 hace TODA la interpretación (TF-IDF).
//  - Plano de la empresa: cada habitación se ilumina si su LED está encendido.
//  - Tablas: pesos TF-IDF de la orden y % de similitud con cada intención.
//  - Modelo: vocabulario e IDF calculados en el entrenamiento.
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
  h3 { font-size: 14px; margin: 14px 0 6px; }
  #microfono { width: 90px; height: 90px; border-radius: 50%; border: none; font-size: 40px; cursor: pointer;
               background: #0b7a75; color: white; display: block; margin: 0 auto 10px; }
  #microfono.escuchando { background: #c62828; animation: pulso 1s infinite; }
  @keyframes pulso { 50% { transform: scale(1.08); } }
  .centro { text-align: center; color: #555; font-size: 14px; }
  .fila { display: flex; gap: 8px; margin-top: 10px; }
  input { flex: 1; min-width: 0; font-size: 16px; padding: 10px; border: 1px solid #ccc; border-radius: 8px; }
  button.enviar { padding: 10px 16px; border: none; border-radius: 8px; background: #0b7a75; color: white; font-size: 15px; cursor: pointer; }
  .plano { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
  .cuarto { border: 3px solid #555; border-radius: 6px; padding: 18px 6px; text-align: center; background: #2b2f36;
            color: #aaa; cursor: pointer; transition: all .3s; user-select: none; }
  .cuarto.on { background: #ffe066; color: #333; box-shadow: 0 0 18px #ffd43b; }
  .cuarto b { display: block; font-size: 15px; } .cuarto small { font-size: 12px; }
  table { width: 100%; border-collapse: collapse; font-size: 14px; }
  th { text-align: left; font-size: 12px; color: #666; padding: 4px; border-bottom: 2px solid #ddd; }
  td { padding: 5px 4px; border-bottom: 1px solid #eee; }
  .barra { background: #e5e7eb; border-radius: 4px; height: 14px; width: 100%; }
  .relleno { height: 14px; border-radius: 4px; background: #9ca3af; }
  .relleno.ok { background: #16a34a; }
  .gris { color: #999; }
  #resultado { font-size: 17px; font-weight: bold; margin-bottom: 8px; }
  .verde { color: #15803d; } .rojo { color: #b91c1c; }
  .ejemplo { display: inline-block; margin: 6px 6px 0 0; padding: 5px 10px; border-radius: 14px; background: #e3eeed;
             color: #0b5a56; font-size: 13px; cursor: pointer; }
  .tablaModelo { max-height: 320px; overflow-y: auto; }
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
    <h2>🧠 Lo que entendió el asistente (TF-IDF)</h2>
    <div id="resultado">Aún no hay órdenes.</div>
    <div id="captado" class="centro" style="text-align:left"></div>
    <h3>1. Peso de cada palabra de la orden</h3>
    <table id="tablaPesos"></table>
    <h3>2. Similitud coseno con cada intención</h3>
    <table id="tabla"></table>
  </div>

  <div class="caja">
    <h2>📚 Modelo entrenado</h2>
    <div class="centro" style="text-align:left" id="resumenModelo"></div>
    <div class="tablaModelo"><table id="tablaModelo"></table></div>
  </div>
</div>

<script>
const UMBRAL = 30;
const EJEMPLOS = ["apaga la luz de la cocina", "enciende la oficina y la recepción", "prende todas las luces",
                  "apaga las luces", "enciende la luz del baño", "desactiva el almacén"];

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
  const frase = c.porcentaje > 0 ? `≈ "${c.frase}"` : "";
  return `<tr><td style="width:24%">${nombre}</td>
    <td style="width:32%"><div class="barra"><div class="relleno ${ok ? "ok" : ""}" style="width:${c.porcentaje}%"></div></div></td>
    <td style="width:10%"><b>${c.porcentaje}%</b></td><td style="color:#666">${frase}</td></tr>`;
};

const mostrar = (d) => {
  const res = document.getElementById("resultado");
  res.className = d.entendido ? "verde" : "rojo";
  res.textContent = (d.entendido ? "✅ " : "❓ ") + d.mensaje + "  (confianza " + d.confianza + "%)";
  document.getElementById("captado").textContent = "Texto: \"" + d.texto + "\"  →  normalizado: \"" + d.normalizado + "\"";

  let p = "<tr><th>Palabra</th><th>TF</th><th>IDF</th><th>TF-IDF</th></tr>";
  for (const w of d.pesos) {
    p += w.conocida
      ? `<tr><td>${w.palabra}</td><td>${w.tf.toFixed(3)}</td><td>${w.idf.toFixed(3)}</td><td><b>${w.tfidf.toFixed(3)}</b></td></tr>`
      : `<tr class="gris"><td>${w.palabra}</td><td>${w.tf.toFixed(3)}</td><td colspan=2>no está en el corpus (se ignora)</td></tr>`;
  }
  document.getElementById("tablaPesos").innerHTML = p;

  let t = "<tr><td colspan=4><b>¿Qué hacer?</b></td></tr>" + fila("Encender", d.encender) + fila("Apagar", d.apagar);
  t += "<tr><td colspan=4><b>¿Dónde?</b> (verde = llega al " + UMBRAL + "%)</td></tr>" + fila("Todas", d.todas);
  for (const l of d.lugares) t += fila(l.nombre, l);
  document.getElementById("tabla").innerHTML = t;
};

// ---------- Modelo entrenado (vocabulario e IDF) ----------
const cargarModelo = async () => {
  const m = await (await fetch("/api/modelo")).json();
  document.getElementById("resumenModelo").textContent =
    m.documentos + " frases en el corpus · " + m.vocabulario.length + " palabras distintas · IDF = log(" +
    m.documentos + " / documentos con la palabra). Las palabras comunes (IDF bajo) pesan menos.";
  const filas = m.vocabulario.sort((a, b) => a.idf - b.idf).map(v =>
    `<tr><td>${v.palabra}</td><td>${v.documentos}</td><td>${v.idf.toFixed(3)}</td></tr>`).join("");
  document.getElementById("tablaModelo").innerHTML =
    "<tr><th>Palabra</th><th>Documentos que la tienen</th><th>IDF</th></tr>" + filas;
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

cargarModelo();
actualizar();
setInterval(actualizar, 2000);
</script>
</body>
</html>
)HTML";


// =============================================================================
// 10. SERVIDOR WEB Y API
// -----------------------------------------------------------------------------
//   GET  /              página web
//   GET  /api/estado    estado de cada luz
//   GET  /api/modelo    corpus, vocabulario e IDF (el modelo entrenado)
//   POST /api/comando   {"texto": "apaga la luz de la cocina"} -> análisis completo
//  Detalle en docs/API.md
// =============================================================================

WebServer servidor(80);

float redondear(float x) { return round(x * 1000) / 1000.0; }   // 3 decimales

void agregarCoincidencia(JsonObject o, const Coincidencia& c) {
  o["porcentaje"] = c.porcentaje;
  o["frase"] = c.frase;
}

void enviarJson(JsonDocument& doc) {
  String texto;
  serializeJson(doc, texto);
  servidor.send(200, "application/json; charset=utf-8", texto);
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
  enviarJson(doc);
}

void rutaModelo() {
  JsonDocument doc;
  doc["documentos"] = NUM_DOCUMENTOS;
  JsonArray vocab = doc["vocabulario"].to<JsonArray>();
  for (int i = 0; i < tamanoVocabulario; i++) {
    JsonObject v = vocab.add<JsonObject>();
    v["palabra"] = vocabulario[i];
    v["documentos"] = documentosConPalabra[i];
    v["idf"] = redondear(idf[i]);
  }
  JsonArray corpus = doc["corpus"].to<JsonArray>();
  for (int d = 0; d < NUM_DOCUMENTOS; d++) {
    JsonObject c = corpus.add<JsonObject>();
    c["frase"] = CORPUS[d].frase;
    c["intencion"] = CORPUS[d].intencion;
  }
  enviarJson(doc);
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
  doc["entendido"] = r.entendido;
  doc["accion"] = r.accion == 1 ? "encender" : (r.accion == 0 ? "apagar" : "desconocida");
  doc["confianza"] = r.confianza;
  doc["umbral"] = UMBRAL;
  doc["mensaje"] = r.mensaje;

  JsonArray pesos = doc["pesos"].to<JsonArray>();
  for (int i = 0; i < r.numPesos; i++) {
    JsonObject p = pesos.add<JsonObject>();
    p["palabra"] = r.pesos[i].palabra;
    p["conocida"] = r.pesos[i].conocida;
    p["tf"] = redondear(r.pesos[i].tf);
    p["idf"] = redondear(r.pesos[i].idf);
    p["tfidf"] = redondear(r.pesos[i].tfidf);
  }

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
  enviarJson(doc);
}


// =============================================================================
// 11. setup() y loop()
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
  Serial.println("\n=== Asistente de voz ESP32-S3 (TF-IDF) ===");

  for (int h = 0; h < NUM_HABITACIONES; h++) {
    pinMode(HABITACIONES[h].pin, OUTPUT);
    cambiarLuz(h, false);                       // al arrancar todo apagado
  }
  pinMode(PIN_LED_ENTENDIDO, OUTPUT);
  pinMode(PIN_LED_NO_ENTENDIDO, OUTPUT);

  entrenarModelo();                             // calcula vocabulario, IDF y vectores
  imprimirModelo();

  conectarWiFi();
  servidor.on("/", HTTP_GET, rutaPagina);
  servidor.on("/api/estado", HTTP_GET, rutaEstado);
  servidor.on("/api/modelo", HTTP_GET, rutaModelo);
  servidor.on("/api/comando", HTTP_POST, rutaComando);
  servidor.begin();

  Serial.println("Listo. Escribe una orden aquí (ej: apaga la luz de la cocina) y pulsa Enter.");
}

void loop() {
  servidor.handleClient();    // página web y API
  revisarMonitorSerie();      // órdenes escritas en el Monitor Serie
  actualizarParpadeo();       // LED verde / rojo de respuesta
}

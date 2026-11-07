// =====================================================================
//  plan.cpp
// =====================================================================
#include "plan.h"
#include "config.h"
#include "dispositivos.h"
#include "tareas.h"
#include "avisos.h"

// ---------------------------------------------------------------------
// Listas de valores permitidos. Cualquier otra cosa se RECHAZA.
// ---------------------------------------------------------------------
static const char* ACCIONES[] = {"encender", "apagar", "avisar",
                                 "consultar_accesos", "listar_tareas", "cancelar_tarea"};
static const char* CONDICIONES[] = {"puertas_cerradas", "alguna_puerta_abierta",
                                    "ventanas_cerradas", "alguna_ventana_abierta"};
static const char* CAMPOS_ACCION[] = {"accion", "dispositivo", "hora", "minutos_despues",
                                      "condicion", "mensaje", "id"};

static bool estaEnLista(const String& valor, const char* lista[], int tam) {
  for (int i = 0; i < tam; i++) if (valor == lista[i]) return true;
  return false;
}
#define TAM(lista) (int)(sizeof(lista) / sizeof(lista[0]))

// Solo estas acciones se pueden programar para después.
// Las consultas (accesos, tareas) se responden de inmediato.
static bool esProgramable(const String& accion) {
  return accion == "encender" || accion == "apagar" || accion == "avisar";
}

// ¿Se puede encender/apagar? Una luz, un aspersor o un grupo.
static bool esObjetivoValido(const String& objetivo) {
  if (objetivo == "luces" || objetivo == "aspersores" || objetivo == "todo") return true;
  const Dispositivo* d = buscarDispositivo(objetivo);
  return d != nullptr && esSalida(*d);
}

// Revisa que el texto tenga la forma "HH:MM" (24 horas).
static bool leerHora(const String& texto, int& h, int& m) {
  if (texto.length() != 5 || texto[2] != ':') return false;
  for (int i : {0, 1, 3, 4}) if (!isDigit(texto[i])) return false;
  h = texto.substring(0, 2).toInt();
  m = texto.substring(3, 5).toInt();
  return h <= 23 && m <= 59;
}

// Próxima vez que el reloj marque HH:MM (hoy, o mañana si ya pasó).
static time_t proximaHora(int h, int m, time_t ahora) {
  struct tm t;
  localtime_r(&ahora, &t);
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  time_t momento = mktime(&t);
  if (momento <= ahora) {
    t.tm_mday += 1;     // ya pasó hoy -> mañana
    momento = mktime(&t);
  }
  return momento;
}

// =====================================================================
//  1) VALIDAR Y EJECUTAR UN PLAN
// =====================================================================
bool procesarPlan(JsonVariant plan, JsonObject respuesta) {
  JsonArray errores = respuesta["errores"].to<JsonArray>();

  // ---- Estructura general -------------------------------------------
  if (!plan.is<JsonObject>()) {
    errores.add("La respuesta no es un objeto JSON");
  } else {
    for (JsonPair campo : plan.as<JsonObject>()) {
      String nombre = campo.key().c_str();
      if (nombre != "tipo" && nombre != "mensaje" && nombre != "acciones")
        errores.add("Campo no permitido: '" + nombre + "'");
    }
  }
  String tipo = plan["tipo"] | "";
  String mensaje = plan["mensaje"] | "";

  // ---- Aclaración: el LLM necesita más información ---------------------
  if (errores.size() == 0 && tipo == "aclaracion") {
    if (mensaje.length() == 0) {
      errores.add("Una aclaración necesita la pregunta en 'mensaje'");
    } else {
      respuesta["ok"] = true;
      respuesta["tipo"] = "aclaracion";
      respuesta["pregunta"] = mensaje;
      return true;   // no se guarda NADA hasta que el usuario aclare
    }
  }

  // ---- No soportado: la orden no corresponde a ninguna acción ----------
  if (errores.size() == 0 && tipo == "no_soportado") {
    respuesta["ok"] = true;
    respuesta["tipo"] = "no_soportado";
    respuesta["mensaje"] = mensaje.length() ? mensaje : String("Esa orden no se puede realizar con este sistema.");
    return true;
  }

  if (errores.size() == 0 && tipo != "plan") errores.add("'tipo' debe ser plan, aclaracion o no_soportado");

  // ---- Revisar cada acción ------------------------------------------
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

    for (JsonPair campo : a.as<JsonObject>()) {
      String nombre = campo.key().c_str();
      if (!estaEnLista(nombre, CAMPOS_ACCION, TAM(CAMPOS_ACCION))) errores.add(p + "campo no permitido '" + nombre + "'");
    }

    if (!a["accion"].is<const char*>()) { errores.add(p + "falta 'accion'"); continue; }
    String accion = a["accion"].as<const char*>();
    if (!estaEnLista(accion, ACCIONES, TAM(ACCIONES))) {
      errores.add(p + "acción no permitida '" + accion + "'");
      continue;
    }
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

    // Hora y minutos
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

  // ---- Si hay errores, NO se ejecuta nada ------------------------------
  if (errores.size() > 0) {
    respuesta["ok"] = false;
    respuesta["tipo"] = "rechazado";
    respuesta["mensaje"] = "La orden tiene errores. No se ejecutó nada.";
    return false;
  }

  // ---- Todo correcto: ejecutar o programar -----------------------------
  JsonArray resultados = respuesta["resultados"].to<JsonArray>();
  time_t ahora = time(nullptr);
  for (JsonVariant a : acciones) {
    String accion = a["accion"].as<const char*>();
    String objetivo = a["dispositivo"] | "";
    if (accion == "cancelar_tarea") objetivo = String(a["id"].as<int>());
    String condicion = a["condicion"] | "";
    String texto = a["mensaje"] | "";

    // Calcular cuándo: 0 = ahora mismo
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

// =====================================================================
//  2) CONDICIONES (se evalúan con los interruptores REALES)
// =====================================================================
static bool condicionCumplida(const String& condicion, String& detalle) {
  int tipo = condicion.indexOf("puerta") >= 0 ? PUERTA : VENTANA;
  String abiertas = listarAccesos(tipo, true);
  detalle = abiertas.length() == 0 ? String("todas cerradas") : "abiertas: " + abiertas;
  if (condicion.endsWith("cerradas")) return abiertas.length() == 0;   // puertas_cerradas / ventanas_cerradas
  return abiertas.length() > 0;                                       // alguna_..._abierta
}

// =====================================================================
//  3) CATÁLOGO DE FUNCIONES AUTORIZADAS
// =====================================================================
String ejecutarAccion(const String& accion, const String& objetivo, const String& condicion,
                      const String& mensaje) {
  String detalle = "";
  if (condicion.length() > 0 && !condicionCumplida(condicion, detalle)) {
    return "No se hizo '" + accion + " " + (objetivo.length() ? objetivo : mensaje) +
           "' porque la condición '" + condicion + "' no se cumple (" + detalle + ")";
  }

  // ---- encender / apagar ---------------------------------------------
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

  // ---- avisar --------------------------------------------------------
  if (accion == "avisar") {
    String texto = mensaje;
    if (detalle.length() > 0 && detalle != "todas cerradas") texto += " (" + detalle + ")";
    agregarAviso(texto, true);      // alerta: se ve en la página y parpadea el LED rojo
    return "Aviso enviado: " + texto;
  }

  // ---- consultar_accesos ---------------------------------------------
  if (accion == "consultar_accesos") {
    int tipo = objetivo == "puertas" ? PUERTA : (objetivo == "ventanas" ? VENTANA : -1);
    String abiertas = listarAccesos(tipo, true);
    String cerradas = listarAccesos(tipo, false);
    if (abiertas.length() == 0) return "Todo está cerrado (" + cerradas + ")";
    return "Abiertas: " + abiertas + (cerradas.length() ? ". Cerradas: " + cerradas : String(""));
  }

  // ---- listar_tareas -------------------------------------------------
  if (accion == "listar_tareas") return textoTareas();

  // ---- cancelar_tarea ------------------------------------------------
  if (accion == "cancelar_tarea") {
    return cancelarTarea(objetivo.toInt()) ? "Tarea #" + objetivo + " cancelada"
                                           : "No existe la tarea #" + objetivo;
  }

  return "Acción no permitida";   // nunca debería llegar aquí (ya se validó)
}

// =====================================================================
//  4) DETECTAR ÓRDENES AMBIGUAS ("más tarde", "en un rato"...)
// =====================================================================
bool esAmbigua(const String& textoOriginal) {
  String t = textoOriginal;
  t.toLowerCase();
  // Quitar tildes para comparar más fácil
  const char* conTilde[] = {"á", "é", "í", "ó", "ú", "Á", "É", "Í", "Ó", "Ú"};
  const char* sinTilde[] = {"a", "e", "i", "o", "u", "a", "e", "i", "o", "u"};
  for (int i = 0; i < 10; i++) t.replace(conTilde[i], sinTilde[i]);

  if (t.indexOf('?') >= 0) return false;   // es una pregunta, no una orden con horario

  const char* vagas[] = {"mas tarde", "en un rato", "al rato", "luego", "pronto", "despuesito",
                         "en algun momento", "cuando puedas", "esta noche", "esta tarde"};
  bool tieneVaga = false;
  for (const char* v : vagas) if (t.indexOf(v) >= 0) tieneVaga = true;
  if (!tieneVaga) return false;

  // Si además dice una hora o cantidad concreta, NO es ambigua.
  for (unsigned i = 0; i < t.length(); i++) if (isDigit(t[i])) return false;
  const char* concretas[] = {"ahora", "inmediat", "media hora", "una hora", "un minuto", "cinco", "diez",
                             "quince", "veinte", "treinta", "cuarenta", "mediodia", "medianoche"};
  for (const char* c : concretas) if (t.indexOf(c) >= 0) return false;
  return true;
}

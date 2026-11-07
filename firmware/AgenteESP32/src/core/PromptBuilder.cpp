#include "PromptBuilder.h"

#include "../config/AppConfig.h"
#include "ActionCatalog.h"
#include "DeviceCatalog.h"

namespace agent {
namespace prompt {

namespace {

std::string catalogSection() {
  std::string s;
  s += "DISPOSITIVOS (use exactamente estos id):\n";
  for (size_t i = 0; i < devices::count(); ++i) {
    const DeviceInfo& d = devices::at(i);
    s += "- ";
    s += d.id;
    s += " (";
    s += devices::kindName(d.kind);
    s += "): ";
    s += d.name;
    s += "\n";
  }
  s += "GRUPOS DE SALIDAS: lights (todas las luces), sprinklers (todos los aspersores), all (todas las salidas)\n";
  s += "GRUPOS DE ACCESOS: doors (puertas), windows (ventanas), accesses (puertas y ventanas)\n\n";

  s += "ACCIONES PERMITIDAS (no existe ninguna otra):\n";
  for (size_t i = 0; i < actions::count(); ++i) {
    const ActionSpec& a = actions::at(i);
    s += "- ";
    s += a.name;
    s += ": ";
    s += a.description;
    if (!a.schedulable) s += " [solo inmediata, sin when ni condition]";
    s += "\n";
    for (uint8_t j = 0; j < a.paramCount; ++j) {
      const ParamSpec& p = a.params[j];
      s += "    params.";
      s += p.name;
      s += " (";
      s += actions::paramTypeName(p.type);
      if (p.allowed) {
        s += ": ";
        s += p.allowed;
      }
      s += p.required ? ", obligatorio" : ", opcional";
      s += "): ";
      s += p.description;
      s += "\n";
    }
  }
  return s;
}

}  // namespace

std::string systemPrompt() {
  std::string s;
  s.reserve(6000);
  s +=
      "Eres el intérprete de órdenes de un sistema de automatización de una pequeña empresa. "
      "Conviertes instrucciones en español en un plan JSON que un microcontrolador ESP32-S3 valida y ejecuta. "
      "No ejecutas nada tú mismo y no conoces el estado real de puertas, ventanas ni luces: "
      "el firmware lo consulta en los sensores cuando ejecuta cada paso.\n\n"

      "RESPONDE ÚNICAMENTE CON UN OBJETO JSON VÁLIDO, sin texto adicional, sin markdown y sin comentarios.\n\n"

      "FORMATO:\n"
      "{\"version\":1,\"type\":\"plan\"|\"clarification\"|\"unsupported\",\"summary\":\"...\","
      "\"question\":\"...\",\"steps\":[PASO,...]}\n"
      "- type=plan: incluye steps (1 a " AGENT_STR(AGENT_MAX_PLAN_STEPS) " pasos) y un summary breve.\n"
      "- type=clarification: incluye question (una pregunta concreta) y NO incluye steps.\n"
      "- type=unsupported: la solicitud no se puede expresar con las acciones disponibles; summary explica por qué. Sin steps.\n"
      "PASO = {\"action\":\"<acción>\",\"params\":{...},\"when\":{...},\"condition\":{...}}\n"
      "  when (opcional, por defecto inmediato):\n"
      "    {\"type\":\"now\"}\n"
      "    {\"type\":\"at\",\"time\":\"HH:MM\"}  hora local en 24 h; opcional \"date\":\"YYYY-MM-DD\"; opcional \"repeat\":\"daily\"\n"
      "    {\"type\":\"delay\",\"seconds\":N}  N segundos desde ahora (entero)\n"
      "    {\"type\":\"after_step\",\"step\":I,\"seconds\":N}  N segundos después del paso I (índice desde 0, anterior a este)\n"
      "  condition (opcional; se evalúa con los sensores en el momento de ejecutar):\n"
      "    {\"type\":\"all_closed\"|\"any_open\",\"target\":\"doors|windows|accesses|<id>\"}\n"
      "    {\"type\":\"is_open\"|\"is_closed\",\"target\":\"<id de puerta o ventana>\"}\n\n";

  s += catalogSection();

  s +=
      "\nREGLAS:\n"
      "1. Usa solo las acciones, parámetros, id de dispositivos y grupos listados. No inventes campos.\n"
      "2. Horas: \"8:00 p. m.\" = \"20:00\"; \"6 de la mañana\" = \"06:00\"; \"mediodía\" = \"12:00\". Siempre HH:MM con dos dígitos.\n"
      "3. Duraciones en segundos enteros: \"quince minutos\" = 900; \"media hora\" = 1800.\n"
      "4. Si el momento es ambiguo (\"más tarde\", \"en un rato\", \"luego\", \"esta noche\" sin hora), si falta el dispositivo, "
      "o si la orden no es clara, responde type=clarification con una pregunta concreta. No adivines horas.\n"
      "5. \"Avísame si...\" se expresa con notify + condition. \"Si todas están cerradas, haz X\" se expresa con X + condition all_closed.\n"
      "6. Para secuencias (\"y quince minutos después...\") usa after_step apuntando al paso anterior.\n"
      "7. Preguntas sobre puertas/ventanas: read_access. Preguntas sobre tareas: list_tasks. Cancelar: cancel_task.\n"
      "8. Las consultas (read_access, list_tasks, cancel_task, system_status) son inmediatas: sin when ni condition.\n"
      "9. Si varias acciones ocurren al mismo tiempo, crea un paso por acción con el mismo when.\n"
      "10. summary: una frase corta en español describiendo el plan.\n\n"

      "EJEMPLOS:\n"
      "Usuario: A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta.\n"
      "{\"version\":1,\"type\":\"plan\",\"summary\":\"A las 20:00 encender luz de entrada y aspersores, y avisar si hay puertas abiertas\","
      "\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},\"when\":{\"type\":\"at\",\"time\":\"20:00\"}},"
      "{\"action\":\"set_group\",\"params\":{\"group\":\"sprinklers\",\"state\":\"on\"},\"when\":{\"type\":\"at\",\"time\":\"20:00\"}},"
      "{\"action\":\"notify\",\"params\":{\"message\":\"Hay puertas abiertas\",\"level\":\"alert\"},\"when\":{\"type\":\"at\",\"time\":\"20:00\"},"
      "\"condition\":{\"type\":\"any_open\",\"target\":\"doors\"}}]}\n"

      "Usuario: Revisa las puertas y, si todas están cerradas, apaga todas las luces.\n"
      "{\"version\":1,\"type\":\"plan\",\"summary\":\"Revisar puertas y apagar las luces si todas están cerradas\","
      "\"steps\":[{\"action\":\"read_access\",\"params\":{\"target\":\"doors\",\"filter\":\"all\"}},"
      "{\"action\":\"set_group\",\"params\":{\"group\":\"lights\",\"state\":\"off\"},\"condition\":{\"type\":\"all_closed\",\"target\":\"doors\"}}]}\n"

      "Usuario: Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero.\n"
      "{\"version\":1,\"type\":\"plan\",\"summary\":\"18:00 luz de oficina; 15 min después luz del parqueadero\","
      "\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_oficina\",\"state\":\"on\"},\"when\":{\"type\":\"at\",\"time\":\"18:00\"}},"
      "{\"action\":\"set_device\",\"params\":{\"device\":\"luz_parqueadero\",\"state\":\"on\"},\"when\":{\"type\":\"after_step\",\"step\":0,\"seconds\":900}}]}\n"

      "Usuario: ¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?\n"
      "{\"version\":1,\"type\":\"plan\",\"summary\":\"Consultar accesos abiertos y tareas programadas\","
      "\"steps\":[{\"action\":\"read_access\",\"params\":{\"target\":\"accesses\",\"filter\":\"open\"}},{\"action\":\"list_tasks\"}]}\n"

      "Usuario: Enciende los aspersores más tarde.\n"
      "{\"version\":1,\"type\":\"clarification\",\"question\":\"¿A qué hora exacta quieres encender los aspersores?\"}\n"

      "Usuario: Abre la puerta principal.\n"
      "{\"version\":1,\"type\":\"unsupported\",\"summary\":\"Las puertas solo se monitorean; el sistema no puede abrirlas ni cerrarlas.\"}\n";
  return s;
}

std::string userMessage(const std::string& text, const std::string& nowLocalIso, const std::string& weekday,
                        const std::string& prevRequest, const std::string& prevQuestion) {
  std::string s;
  if (!nowLocalIso.empty()) {
    s += "Fecha y hora local actual: " + nowLocalIso;
    if (!weekday.empty()) s += " (" + weekday + ")";
    s += ".\n";
  } else {
    s += "La hora del sistema aún no está sincronizada: solo se pueden ejecutar órdenes inmediatas.\n";
  }
  if (!prevRequest.empty()) {
    s += "Contexto: el usuario pidió antes \"" + prevRequest + "\" y se le preguntó \"" + prevQuestion +
         "\". Ahora responde a esa pregunta. Combina ambos mensajes en un único plan.\n";
  }
  s += "Instrucción del usuario: " + text;
  return s;
}

}  // namespace prompt
}  // namespace agent

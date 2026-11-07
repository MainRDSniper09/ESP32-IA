/**
 * Pruebas unitarias del núcleo (se ejecutan en el PC, sin ESP32):
 *     cd firmware && pio test -e native
 *
 * Cubren los criterios de aceptación del enunciado:
 *  - operaciones válidas se ejecutan,
 *  - JSON / parámetros incorrectos se rechazan,
 *  - instrucciones ambiguas piden aclaración,
 *  - condiciones se evalúan con el estado real de los sensores,
 *  - tareas programadas, secuencias, cancelación,
 *  - recuperación de tareas tras un reinicio y tareas perdidas.
 */
#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <map>
#include <string>

#include "core/Agent.h"
#include "core/DeviceCatalog.h"
#include "core/PlanValidator.h"
#include "core/PromptBuilder.h"
#include "core/TextUtils.h"
#include "core/TimeUtils.h"

using namespace agent;

// ============================================================ Dobles de prueba

class FakeHardware : public IHardware {
 public:
  std::map<std::string, bool> outputs, open;
  void writeOutput(const DeviceInfo& d, bool on) override { outputs[d.id] = on; }
  bool readOutput(const DeviceInfo& d) const override {
    auto it = outputs.find(d.id);
    return it != outputs.end() && it->second;
  }
  bool isOpen(const DeviceInfo& d) const override {
    auto it = open.find(d.id);
    return it != open.end() && it->second;
  }
  bool out(const char* id) const { return readOutput(*devices::find(id)); }
};

class FakeClock : public IClock {
 public:
  time_t t = 0;
  bool synced = true;
  time_t now() const override { return t; }
  bool isSynced() const override { return synced; }
  uint32_t uptimeSeconds() const override { return 42; }
};

class MemStore : public ITaskStore {
 public:
  std::string data;
  int saves = 0;
  bool load(std::string& out) override {
    out = data;
    return true;
  }
  bool save(const std::string& d) override {
    data = d;
    ++saves;
    return true;
  }
};

// ================================================================= Utilidades

/** Instante epoch para una fecha/hora LOCAL (zona de Colombia, UTC-5). */
static time_t local(int y, int mo, int d, int h, int mi) {
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = -1;
  return mktime(&t);
}

static bool validate(const char* json, Plan& plan, std::vector<ValidationError>& errors) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  return PlanValidator::validatePlan(doc.as<JsonVariantConst>(), plan, errors);
}

static bool hasError(const std::vector<ValidationError>& errors, const char* code) {
  for (size_t i = 0; i < errors.size(); ++i)
    if (errors[i].code == code) return true;
  return false;
}

static std::string runPlan(Agent& a, const char* json, JsonDocument& out) {
  JsonDocument in;
  deserializeJson(in, json);
  a.handlePlanJson(in.as<JsonVariantConst>(), "prueba", false, out.to<JsonObject>());
  return out["outcome"].as<std::string>();
}

static size_t countEvents(Agent& a, const char* level) {
  JsonDocument d;
  a.writeEvents(d.to<JsonArray>(), 0, 100);
  size_t n = 0;
  for (JsonObject e : d.as<JsonArray>())
    if (e["level"] == level) ++n;
  return n;
}

void setUp() {
  // UTC-5 (Colombia). "COT5" es válido tanto en POSIX como en el CRT de Windows.
#if defined(_WIN32)
  _putenv("TZ=COT5");
#else
  setenv("TZ", "COT5", 1);
#endif
  tzset();
}
void tearDown() {}

// ===================================================================== Plan JSON

static const char* kScenario1 =
    "{\"version\":1,\"type\":\"plan\",\"summary\":\"s\",\"steps\":["
    "{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},\"when\":{\"type\":\"at\",\"time\":\"20:00\"}},"
    "{\"action\":\"set_group\",\"params\":{\"group\":\"sprinklers\",\"state\":\"on\"},\"when\":{\"type\":\"at\",\"time\":\"20:00\"}},"
    "{\"action\":\"notify\",\"params\":{\"message\":\"Hay puertas abiertas\",\"level\":\"alert\"},\"when\":{\"type\":\"at\",\"time\":\"20:00\"},"
    "\"condition\":{\"type\":\"any_open\",\"target\":\"doors\"}}]}";

void test_valid_plan_scenario1() {
  Plan p;
  std::vector<ValidationError> e;
  TEST_ASSERT_TRUE(validate(kScenario1, p, e));
  TEST_ASSERT_EQUAL(3, p.steps.size());
  TEST_ASSERT_EQUAL(20, p.steps[0].when.hour);
  TEST_ASSERT_TRUE(p.steps[2].condition.type == ConditionType::AnyOpen);
}

void test_rejects_unknown_action() {
  Plan p;
  std::vector<ValidationError> e;
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"format_flash\",\"params\":{}}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "unknown_action"));
}

void test_rejects_unknown_device_and_input_as_output() {
  Plan p;
  std::vector<ValidationError> e;
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_cocina\",\"state\":\"on\"}}]}",
      p, e));
  TEST_ASSERT_TRUE(hasError(e, "unknown_device"));
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"puerta_principal\",\"state\":\"on\"}}]}",
      p, e));
  TEST_ASSERT_TRUE(hasError(e, "device_not_output"));
}

void test_rejects_bad_types_and_times() {
  Plan p;
  std::vector<ValidationError> e;
  // Hora fuera de rango
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"at\",\"time\":\"25:00\"}}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "invalid_time"));
  // Formato sin cero inicial
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"at\",\"time\":\"8:00\"}}]}", p, e));
  // Número como texto
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"delay\",\"seconds\":\"900\"}}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "invalid_delay"));
  // Estado inválido
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"encendida\"}}]}",
      p, e));
  TEST_ASSERT_TRUE(hasError(e, "invalid_value"));
}

void test_rejects_unknown_fields() {
  Plan p;
  std::vector<ValidationError> e;
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"code\":\"rm -rf\",\"steps\":[{\"action\":\"list_tasks\",\"exec\":\"x\"}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "unknown_field"));
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\",\"pin\":4}}]}",
      p, e));
  TEST_ASSERT_TRUE(hasError(e, "unknown_param"));
}

void test_clarification_and_unsupported() {
  Plan p;
  std::vector<ValidationError> e;
  TEST_ASSERT_TRUE(validate("{\"type\":\"clarification\",\"question\":\"¿A qué hora?\"}", p, e));
  TEST_ASSERT_TRUE(p.type == PlanType::Clarification);
  TEST_ASSERT_FALSE(validate("{\"type\":\"clarification\",\"question\":\"\"}", p, e));
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"clarification\",\"question\":\"x\",\"steps\":[{\"action\":\"list_tasks\"}]}", p, e));
  TEST_ASSERT_TRUE(validate("{\"type\":\"unsupported\",\"summary\":\"No puedo abrir puertas\"}", p, e));
  TEST_ASSERT_FALSE(validate("{\"type\":\"maybe\"}", p, e));
}

void test_step_reference_and_schedulability() {
  Plan p;
  std::vector<ValidationError> e;
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"after_step\",\"step\":0,\"seconds\":10}}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "invalid_step_ref"));
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"list_tasks\",\"when\":{\"type\":\"delay\",\"seconds\":60}}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "not_schedulable"));
  TEST_ASSERT_FALSE(validate(
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"cancel_task\",\"params\":{}}]}", p, e));
  TEST_ASSERT_TRUE(hasError(e, "invalid_params"));
}

// ======================================================================= Texto

void test_ambiguity_guard() {
  std::string phrase;
  TEST_ASSERT_TRUE(text::findAmbiguousTime("Enciende los aspersores más tarde", phrase));
  TEST_ASSERT_EQUAL_STRING("mas tarde", phrase.c_str());
  TEST_ASSERT_TRUE(text::findAmbiguousTime("apaga las luces en un rato", phrase));
  TEST_ASSERT_FALSE(text::findAmbiguousTime("Enciende la luz y quince minutos después la otra", phrase));
  TEST_ASSERT_FALSE(text::findAmbiguousTime("Más tarde, a las 9:30, apaga todo", phrase));
  TEST_ASSERT_FALSE(text::findAmbiguousTime("¿Qué tareas hay para esta noche?", phrase));
  TEST_ASSERT_FALSE(text::findAmbiguousTime("Apaga la luz de la oficina", phrase));
}

void test_json_extraction() {
  std::string out;
  TEST_ASSERT_TRUE(text::extractJsonObject("```json\n{\"a\":{\"b\":\"}\"}}\n```", out));
  TEST_ASSERT_EQUAL_STRING("{\"a\":{\"b\":\"}\"}}", out.c_str());
  TEST_ASSERT_FALSE(text::extractJsonObject("no hay json aquí", out));
  TEST_ASSERT_FALSE(text::extractJsonObject("{\"a\":1", out));
}

void test_time_utils() {
  int h, m;
  TEST_ASSERT_TRUE(timeutil::parseHHMM("23:59", h, m));
  TEST_ASSERT_FALSE(timeutil::parseHHMM("24:00", h, m));
  int y, mo, d;
  TEST_ASSERT_TRUE(timeutil::parseDate("2028-02-29", y, mo, d));
  TEST_ASSERT_FALSE(timeutil::parseDate("2027-02-29", y, mo, d));
  time_t now = local(2026, 11, 6, 21, 0);
  time_t out;
  TEST_ASSERT_TRUE(timeutil::resolveLocalTime(now, 20, 0, "", out));  // ya pasó -> mañana
  TEST_ASSERT_EQUAL(local(2026, 11, 7, 20, 0), out);
  TEST_ASSERT_EQUAL_STRING("2026-11-07T20:00:00-05:00", timeutil::toIso8601(out).c_str());
}

void test_prompt_lists_catalog() {
  std::string s = prompt::systemPrompt();
  TEST_ASSERT_TRUE(s.find("luz_parqueadero") != std::string::npos);
  TEST_ASSERT_TRUE(s.find("set_group") != std::string::npos);
  TEST_ASSERT_TRUE(s.find("clarification") != std::string::npos);
}

// ======================================================================= Agente

void test_immediate_control() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument out;
  TEST_ASSERT_EQUAL_STRING("executed", runPlan(a,
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_bodega\",\"state\":\"on\"}}]}",
      out).c_str());
  TEST_ASSERT_TRUE(hw.out("luz_bodega"));
}

void test_invalid_plan_executes_nothing() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument out;
  // Primer paso válido, segundo inválido -> NADA se ejecuta (plan atómico).
  TEST_ASSERT_EQUAL_STRING("rejected", runPlan(a,
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_bodega\",\"state\":\"on\"}},"
      "{\"action\":\"set_device\",\"params\":{\"device\":\"luz_marte\",\"state\":\"on\"}}]}",
      out).c_str());
  TEST_ASSERT_FALSE(hw.out("luz_bodega"));
  TEST_ASSERT_TRUE(out["errors"].size() >= 1);
}

void test_conditional_uses_real_sensors() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  hw.outputs["luz_entrada"] = true;
  hw.outputs["luz_oficina"] = true;
  const char* plan =
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"read_access\",\"params\":{\"target\":\"doors\"}},"
      "{\"action\":\"set_group\",\"params\":{\"group\":\"lights\",\"state\":\"off\"},"
      "\"condition\":{\"type\":\"all_closed\",\"target\":\"doors\"}}]}";

  hw.open["puerta_bodega"] = true;  // una puerta abierta -> NO apaga
  JsonDocument o1;
  runPlan(a, plan, o1);
  TEST_ASSERT_EQUAL_STRING("skipped", o1["results"][1]["status"]);
  TEST_ASSERT_TRUE(hw.out("luz_entrada"));

  hw.open["puerta_bodega"] = false;  // todas cerradas -> apaga
  JsonDocument o2;
  runPlan(a, plan, o2);
  TEST_ASSERT_EQUAL_STRING("executed", o2["results"][1]["status"]);
  TEST_ASSERT_FALSE(hw.out("luz_entrada"));
  TEST_ASSERT_FALSE(hw.out("luz_oficina"));
}

void test_sequence_scenario3_and_tick() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument out;
  TEST_ASSERT_EQUAL_STRING("scheduled", runPlan(a,
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_oficina\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"at\",\"time\":\"18:00\"}},"
      "{\"action\":\"set_device\",\"params\":{\"device\":\"luz_parqueadero\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"after_step\",\"step\":0,\"seconds\":900}}]}",
      out).c_str());
  TEST_ASSERT_EQUAL(2, a.taskCount());
  TEST_ASSERT_EQUAL((uint32_t)local(2026, 11, 6, 18, 15), out["results"][1]["due"].as<uint32_t>());

  clk.t = local(2026, 11, 6, 17, 59);
  a.tick();
  TEST_ASSERT_FALSE(hw.out("luz_oficina"));
  clk.t = local(2026, 11, 6, 18, 0);
  a.tick();
  TEST_ASSERT_TRUE(hw.out("luz_oficina"));
  TEST_ASSERT_FALSE(hw.out("luz_parqueadero"));
  clk.t = local(2026, 11, 6, 18, 15);
  a.tick();
  TEST_ASSERT_TRUE(hw.out("luz_parqueadero"));
  TEST_ASSERT_EQUAL(0, a.taskCount());
}

void test_scenario1_alert_lists_open_doors() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 19, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument out;
  runPlan(a, kScenario1, out);
  TEST_ASSERT_EQUAL(3, a.taskCount());
  hw.open["puerta_principal"] = true;
  clk.t = local(2026, 11, 6, 20, 0);
  a.tick();
  TEST_ASSERT_TRUE(hw.out("luz_entrada"));
  TEST_ASSERT_TRUE(hw.out("aspersor_frontal"));
  TEST_ASSERT_TRUE(hw.out("aspersor_trasero"));
  TEST_ASSERT_EQUAL(1, countEvents(a, "alert"));
  TEST_ASSERT_TRUE(a.unacknowledgedAlerts() > 0);
}

void test_persistence_after_reboot() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;  // simula la NVS: sobrevive al "reinicio"
  {
    Agent before(hw, clk, st);
    JsonDocument out;
    runPlan(before,
            "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},"
            "\"when\":{\"type\":\"at\",\"time\":\"20:00\",\"repeat\":\"daily\"}}]}",
            out);
    TEST_ASSERT_TRUE(st.saves > 0);
  }
  // ---- reinicio: hardware nuevo (todo apagado), mismo almacenamiento
  FakeHardware hw2;
  clk.synced = false;  // tras reiniciar, aún sin NTP
  Agent after(hw2, clk, st);
  after.begin();
  TEST_ASSERT_EQUAL(1, after.taskCount());
  clk.t = local(2026, 11, 6, 20, 0);
  after.tick();  // sin hora válida no ejecuta
  TEST_ASSERT_FALSE(hw2.out("luz_entrada"));
  clk.synced = true;
  after.tick();
  TEST_ASSERT_TRUE(hw2.out("luz_entrada"));
  TEST_ASSERT_EQUAL(1, after.taskCount());  // diaria: se reprogramó para mañana
  JsonDocument tasks;
  after.writeTasks(tasks.to<JsonArray>());
  TEST_ASSERT_EQUAL((uint32_t)local(2026, 11, 7, 20, 0), tasks[0]["due"].as<uint32_t>());
}

void test_missed_task_is_not_executed() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument out;
  runPlan(a,
          "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_group\",\"params\":{\"group\":\"sprinklers\",\"state\":\"on\"},"
          "\"when\":{\"type\":\"at\",\"time\":\"18:00\"}}]}",
          out);
  clk.t = local(2026, 11, 6, 23, 0);  // el equipo estuvo apagado 5 h
  a.tick();
  TEST_ASSERT_FALSE(hw.out("aspersor_frontal"));
  TEST_ASSERT_EQUAL(0, a.taskCount());
  TEST_ASSERT_EQUAL(1, countEvents(a, "alert"));
}

void test_unsynced_clock_rejects_scheduling() {
  FakeHardware hw;
  FakeClock clk;
  clk.synced = false;
  clk.t = 1000;
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument o1;
  TEST_ASSERT_EQUAL_STRING("rejected", runPlan(a,
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"},"
      "\"when\":{\"type\":\"delay\",\"seconds\":60}}]}", o1).c_str());
  TEST_ASSERT_EQUAL_STRING("clock_not_synced", o1["errors"][0]["code"]);
  JsonDocument o2;
  TEST_ASSERT_EQUAL_STRING("executed", runPlan(a,
      "{\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":{\"device\":\"luz_entrada\",\"state\":\"on\"}}]}",
      o2).c_str());
}

void test_cancel_and_list() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument out;
  runPlan(a, kScenario1, out);
  uint32_t id = out["results"][0]["task_id"];
  TEST_ASSERT_TRUE(a.cancelTask(id));
  TEST_ASSERT_FALSE(a.cancelTask(id));
  TEST_ASSERT_EQUAL(2, a.taskCount());
  JsonDocument o2;
  runPlan(a, "{\"type\":\"plan\",\"steps\":[{\"action\":\"list_tasks\"},{\"action\":\"cancel_task\",\"params\":{\"all\":true}}]}", o2);
  TEST_ASSERT_EQUAL(2, o2["results"][0]["data"]["tasks"].size());
  TEST_ASSERT_EQUAL(0, a.taskCount());
}

void test_llm_text_handling() {
  FakeHardware hw;
  FakeClock clk;
  clk.t = local(2026, 11, 6, 17, 0);
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument o1;
  a.handleLlmText("Lo siento, no entendí.", "x", o1.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("no_json", o1["errors"][0]["code"]);
  JsonDocument o2;
  a.handleLlmText("{\"type\":\"plan\",\"steps\":[{\"action\":\"list_tasks\",}]}", "x", o2.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("invalid_json", o2["errors"][0]["code"]);
  JsonDocument o3;
  a.handleLlmText(
      "Claro:\n```json\n{\"version\":1,\"type\":\"plan\",\"steps\":[{\"action\":\"set_device\",\"params\":"
      "{\"device\":\"luz_oficina\",\"state\":\"on\"}}]}\n```",
      "x", o3.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("executed", o3["outcome"]);
  TEST_ASSERT_TRUE(hw.out("luz_oficina"));
}

void test_precheck_ambiguous() {
  FakeHardware hw;
  FakeClock clk;
  MemStore st;
  Agent a(hw, clk, st);
  JsonDocument o;
  TEST_ASSERT_TRUE(a.precheckText("Enciende los aspersores más tarde", o.to<JsonObject>()));
  TEST_ASSERT_EQUAL_STRING("clarification", o["outcome"]);
  JsonDocument o2;
  TEST_ASSERT_FALSE(a.precheckText("Enciende los aspersores a las 9 pm", o2.to<JsonObject>()));
}

void test_store_corruption_is_tolerated() {
  FakeHardware hw;
  FakeClock clk;
  MemStore st;
  st.data = "{\"next_id\":5,\"tasks\":[{\"id\":3,\"due\":1800000000,\"step\":{\"action\":\"set_device\",\"params\":"
            "{\"device\":\"luz_entrada\",\"state\":\"on\"}}},{\"id\":4,\"due\":1800000000,\"step\":{\"action\":\"hack\"}}]}";
  Agent a(hw, clk, st);
  a.begin();
  TEST_ASSERT_EQUAL(1, a.taskCount());  // la tarea inválida se descarta
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_plan_scenario1);
  RUN_TEST(test_rejects_unknown_action);
  RUN_TEST(test_rejects_unknown_device_and_input_as_output);
  RUN_TEST(test_rejects_bad_types_and_times);
  RUN_TEST(test_rejects_unknown_fields);
  RUN_TEST(test_clarification_and_unsupported);
  RUN_TEST(test_step_reference_and_schedulability);
  RUN_TEST(test_ambiguity_guard);
  RUN_TEST(test_json_extraction);
  RUN_TEST(test_time_utils);
  RUN_TEST(test_prompt_lists_catalog);
  RUN_TEST(test_immediate_control);
  RUN_TEST(test_invalid_plan_executes_nothing);
  RUN_TEST(test_conditional_uses_real_sensors);
  RUN_TEST(test_sequence_scenario3_and_tick);
  RUN_TEST(test_scenario1_alert_lists_open_doors);
  RUN_TEST(test_persistence_after_reboot);
  RUN_TEST(test_missed_task_is_not_executed);
  RUN_TEST(test_unsynced_clock_rejects_scheduling);
  RUN_TEST(test_cancel_and_list);
  RUN_TEST(test_llm_text_handling);
  RUN_TEST(test_precheck_ambiguous);
  RUN_TEST(test_store_corruption_is_tolerated);
  return UNITY_END();
}

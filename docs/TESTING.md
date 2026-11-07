# Informe de pruebas

Las pruebas se dividen en dos niveles:

1. **Automáticas (núcleo, en PC)** — 23 pruebas unitarias con Unity sobre la lógica real del
   firmware (`src/core/`) usando dobles de prueba para GPIO, reloj y NVS. Corren con
   `pio test -e native` y en GitHub Actions en cada *push*.
2. **Integración en el prototipo (ESP32-S3)** — protocolo manual con LED e interruptores.
   La tabla de la sección 3 es la plantilla para registrar los resultados en el dispositivo.

Además, `tools/validate_examples.py` verifica los planes de `examples/plans/` contra el
JSON Schema, y la compilación del firmware se verifica para ESP32-S3 en CI.

---

## 1. Cómo ejecutar

```bash
cd firmware
pio test -e native            # pruebas del núcleo (requiere gcc/g++)
pio run -e esp32s3            # compilación del firmware
cd ..
pip install jsonschema && python tools/validate_examples.py
```

Salida de referencia (`pio test -e native`):

```
test_valid_plan_scenario1                        [PASSED]
test_rejects_unknown_action                      [PASSED]
...
test_store_corruption_is_tolerated               [PASSED]
================= 23 test cases: 23 succeeded =================
```

---

## 2. Pruebas automáticas y requisitos que cubren

Archivo: [`firmware/test/test_core/test_main.cpp`](../firmware/test/test_core/test_main.cpp).
Zona horaria de prueba: UTC-5. «Ahora» simulado: 06/11/2026 17:00.

| # | Prueba | Requisito / criterio de aceptación | Qué verifica | Resultado |
|---|--------|-----------------------------------|--------------|:---:|
| 1 | `test_valid_plan_scenario1` | Interpretación | El JSON del escenario 1 se valida y produce 3 pasos tipados con hora y condición. | ✅ |
| 2 | `test_rejects_unknown_action` | Rechazar acciones desconocidas | `"action":"format_flash"` → `unknown_action`. | ✅ |
| 3 | `test_rejects_unknown_device_and_input_as_output` | Validar nombres de dispositivos | `luz_cocina` → `unknown_device`; encender `puerta_principal` → `device_not_output`. | ✅ |
| 4 | `test_rejects_bad_types_and_times` | Validar tipos y horarios | `"25:00"`, `"8:00"`, `"seconds":"900"`, `"state":"encendida"` → rechazados. | ✅ |
| 5 | `test_rejects_unknown_fields` | Validar esquema | Claves extra en plan, paso o `params` (`"code"`, `"exec"`, `"pin"`) → rechazadas. | ✅ |
| 6 | `test_clarification_and_unsupported` | Ambigüedad | `clarification` requiere pregunta y no admite pasos; `type` desconocido se rechaza. | ✅ |
| 7 | `test_step_reference_and_schedulability` | Coherencia | `after_step` hacia adelante, consultas programadas y `cancel_task` sin id → rechazados. | ✅ |
| 8 | `test_ambiguity_guard` | **Instrucción ambigua** | «más tarde», «en un rato» → ambiguo; «quince minutos después», «más tarde, a las 9:30», preguntas → no ambiguo. | ✅ |
| 9 | `test_json_extraction` | Robustez ante el LLM | Extrae JSON dentro de ```` ```json ```` con llaves en cadenas; rechaza texto sin JSON o incompleto. | ✅ |
| 10 | `test_time_utils` | Horarios / zona horaria | `HH:MM`, años bisiestos, «20:00 ya pasó → mañana», formato ISO 8601 con `-05:00`. | ✅ |
| 11 | `test_prompt_lists_catalog` | Catálogo explícito | El prompt se genera desde los catálogos (dispositivos y acciones). | ✅ |
| 12 | `test_immediate_control` | **Control inmediato** | Un plan válido enciende el LED de inmediato. | ✅ |
| 13 | `test_invalid_plan_executes_nothing` | **Rechazar JSON incorrecto** | Paso 1 válido + paso 2 inválido → **nada** se ejecuta (atomicidad). | ✅ |
| 14 | `test_conditional_uses_real_sensors` | **Secuencia condicional** evaluada localmente | Con una puerta abierta las luces NO se apagan (`skipped`); al cerrarla, sí. | ✅ |
| 15 | `test_sequence_scenario3_and_tick` | **Tarea horaria + secuencia** | 18:00 oficina y +900 s parqueadero; a las 17:59 nada, 18:00 solo oficina, 18:15 parqueadero. | ✅ |
| 16 | `test_scenario1_alert_lists_open_doors` | Alertas | A las 20:00 se encienden luz y aspersores y se genera **1 alerta** porque hay una puerta abierta. | ✅ |
| 17 | `test_persistence_after_reboot` | **Recuperación tras reinicio** | Tarea diaria guardada → «reinicio» con hardware nuevo y misma NVS → se recupera, espera al NTP, se ejecuta y se reprograma para el día siguiente. | ✅ |
| 18 | `test_missed_task_is_not_executed` | Reinicio largo | Tarea vencida hace 5 h → no se ejecuta, se elimina y se genera alerta. | ✅ |
| 19 | `test_unsynced_clock_rejects_scheduling` | Sin NTP | Plan programado → `clock_not_synced`; orden inmediata → se ejecuta. | ✅ |
| 20 | `test_cancel_and_list` | **Listar y cancelar tareas** | Cancelar por id, cancelar inexistente, `list_tasks` + `cancel_task all`. | ✅ |
| 21 | `test_llm_text_handling` | Falla del LLM / JSON inválido | Texto sin JSON → `no_json`; JSON roto → `invalid_json`; JSON en markdown → se ejecuta. | ✅ |
| 22 | `test_precheck_ambiguous` | **Instrucción ambigua** | «más tarde» → `clarification` local; «a las 9 pm» → pasa al LLM. | ✅ |
| 23 | `test_store_corruption_is_tolerated` | Persistencia robusta | Una tarea corrupta en la NVS se descarta; la válida se recupera. | ✅ |

**Resultado:** 23/23 aprobadas (GCC, `pio test -e native`). El firmware completo compila sin
advertencias para ESP32-S3 (`pio run -e esp32s3`).

---

## 3. Protocolo de integración en el prototipo

Ejecutar con el hardware de [HARDWARE.md](HARDWARE.md) y la interfaz web abierta.
Registrar en la columna «Obtenido» lo observado y adjuntar capturas/video.
Para pruebas horarias rápidas use `delay` o una hora 2–3 minutos en el futuro.

### 3.1 Control inmediato y consulta de accesos

| ID | Acción | Esperado | Obtenido |
|----|--------|----------|----------|
| I-1 | Botón «Encender» de *Luz de la entrada* | LED GPIO4 enciende; evento «Luz de la entrada encendida». | |
| I-2 | Orden: «Enciende la luz de la bodega ahora» | `executed`; LED GPIO7 enciende. | |
| I-3 | Abrir interruptor *Puerta bodega* | En < 1 s la UI la muestra ABIERTA; evento «Puerta de la bodega abierta». | |
| I-4 | Orden: «¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?» (escenario 4) | `executed`; mensaje lista exactamente los accesos abiertos reales y las tareas. | |

### 3.2 Tarea horaria y secuencias

| ID | Acción | Esperado | Obtenido |
|----|--------|----------|----------|
| T-1 | Escenario 1 con la hora actual + 2 min, con *Puerta principal* abierta | `scheduled` (3 tareas). A la hora: LED entrada + 2 aspersores encienden; alerta «Hay puertas abiertas (abiertas: Puerta principal)», LED rojo parpadea, pitido. | |
| T-2 | Repetir T-1 con todas las puertas cerradas | Se encienden las salidas; **no** hay alerta (paso omitido). | |
| T-3 | Escenario 3 (hora actual + 2 min, «dos minutos después» en lugar de quince) | 2 tareas; la segunda exactamente 2 min después de la primera. | |
| T-4 | Durante la espera de T-1, abrir/cerrar interruptores | El estado se actualiza en la UI en tiempo real (el planificador no bloquea el monitoreo). | |

### 3.3 Secuencia condicional

| ID | Acción | Esperado | Obtenido |
|----|--------|----------|----------|
| C-1 | Luces encendidas, *Puerta oficina* abierta. Orden: escenario 2 | `read_access` lista la puerta abierta; `set_group` **omitido** con motivo; luces siguen encendidas. | |
| C-2 | Cerrar todas las puertas; repetir | Todas las luces se apagan. | |

### 3.4 Instrucciones ambiguas o inválidas

| ID | Acción | Esperado | Obtenido |
|----|--------|----------|----------|
| A-1 | «Enciende los aspersores más tarde» | `clarification` (`source: local_guard`); **ninguna tarea creada**. | |
| A-2 | Responder «a las 7:30 p. m.» | La UI envía el contexto; `scheduled` a las 19:30. | |
| A-3 | «Abre la puerta principal» | `unsupported`; nada cambia. | |
| A-4 | «Enciende la luz del sótano» | `clarification`/`unsupported`, o `rejected` con `unknown_device`; nada cambia. | |
| A-5 | Panel «Plan JSON directo»: `examples/plans/invalid/hora_invalida.json` | HTTP 422, `invalid_time`; nada cambia. | |
| A-6 | Panel «Plan JSON directo»: texto `json_mal_formado.txt` | HTTP 400 `invalid_json`. | |
| A-7 | `accion_desconocida.json` | 422 `unknown_action`. | |

### 3.5 Recuperación tras reinicio

| ID | Acción | Esperado | Obtenido |
|----|--------|----------|----------|
| R-1 | Programar una tarea a +5 min y otra diaria; pulsar **RST** del ESP32 | Al arrancar: «Se recuperaron 2 tarea(s)…»; la UI las lista con los mismos ids. | |
| R-2 | Esperar la hora de la primera | Se ejecuta normalmente. | |
| R-3 | Programar tarea a +2 min, desconectar la alimentación 15 min, reconectar | No se ejecuta; alerta «Tarea #N NO se ejecutó…». | |

### 3.6 Fallos de red y API

| ID | Acción | Esperado | Obtenido |
|----|--------|----------|----------|
| F-1 | Clave de API inválida | `outcome: error`, `llm_http` 401; evento `warning`; nada se ejecuta. | |
| F-2 | Apagar el router con tareas programadas | Evento «Conexión Wi-Fi perdida»; tareas siguen ejecutándose a su hora; AP de respaldo activo a los 30 s. | |
| F-3 | Arrancar sin Wi-Fi | Red `Agente-ESP32` disponible; UI en 192.168.4.1; control manual funciona; órdenes programadas → `clock_not_synced`. | |

---

## 4. Casos de ejemplo documentados

### Caso de instrucción ambigua (A-1)

Entrada: `{"text": "Enciende los aspersores más tarde"}`

```json
{
  "outcome": "clarification",
  "question": "Mencionaste \"mas tarde\", que no indica un momento exacto. ¿A qué hora (por ejemplo 20:30) o en cuántos minutos quieres que lo haga?",
  "message": "Mencionaste \"mas tarde\", ...",
  "source": "local_guard"
}
```

`GET /api/tasks` → `{"tasks": [], "count": 0, ...}` (no se guardó nada).

### Caso de JSON inválido (A-5)

```json
{
  "outcome": "rejected",
  "message": "El plan no pasó la validación. No se ejecutó nada.",
  "errors": [{ "code": "invalid_time", "field": "steps[0].when.time",
               "message": "Hora inválida: '8 pm' (formato HH:MM, 00:00-23:59)" }]
}
```

### Caso de recuperación tras reinicio (R-1)

Consola serie después de pulsar RST:

```
[evento #1] info    system   Se recuperaron 2 tarea(s) de la memoria no volátil
[wifi] Conectado. IP: 192.168.1.60
[evento #4] info    time     Hora sincronizada por NTP: 2026-11-06T18:02:11-05:00
[evento #5] info    task     Tarea #4 ejecutada. Luz de la oficina encendida
```

(Verificado de forma automática por `test_persistence_after_reboot` y `test_missed_task_is_not_executed`.)

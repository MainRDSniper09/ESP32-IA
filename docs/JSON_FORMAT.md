# Formato JSON de las instrucciones (plan)

Este documento define el contrato entre el **modelo de lenguaje (LLM)** y el **firmware**.
El LLM traduce el español a este JSON; el firmware lo trata como **entrada no confiable**,
lo valida campo por campo (`src/core/PlanValidator.cpp`) y solo entonces lo ejecuta o lo
programa. El mismo formato se acepta directamente en `POST /api/plan` (sin LLM).

- Esquema formal: [`docs/schema/plan.schema.json`](schema/plan.schema.json) (JSON Schema 2020-12).
- Ejemplos válidos e inválidos: [`examples/plans/`](../examples/plans).

---

## 1. Estructura general

```json
{
  "version": 1,
  "type": "plan",
  "summary": "Frase corta que describe el plan",
  "steps": [ PASO, PASO, ... ]
}
```

| Campo      | Tipo   | Obligatorio | Descripción |
|------------|--------|-------------|-------------|
| `version`  | entero | no (si está, debe ser `1`) | Versión del formato. |
| `type`     | texto  | **sí** | `plan`, `clarification` o `unsupported`. |
| `summary`  | texto  | no | Resumen legible (se recorta a 200 bytes). Solo informativo: **no** se usa para decidir nada. |
| `question` | texto  | solo con `clarification` | Pregunta concreta para el usuario. |
| `steps`    | arreglo | solo con `plan` | Entre 1 y 8 pasos. |

Cualquier otra clave se rechaza con `unknown_field`.

### Los tres tipos de respuesta

| `type` | Cuándo lo usa el LLM | Qué hace el firmware |
|--------|----------------------|----------------------|
| `plan` | La orden es clara y se puede expresar con el catálogo. | Valida, resuelve horarios, ejecuta y/o programa. |
| `clarification` | Hay ambigüedad: «más tarde», «en un rato», falta el dispositivo... | **No guarda nada.** Devuelve la pregunta; la siguiente respuesta del usuario se envía con el contexto. |
| `unsupported` | La orden no corresponde a ninguna acción (p. ej. «abre la puerta»: las puertas solo se monitorean). | No ejecuta nada; muestra el `summary`. |

```json
{"version":1,"type":"clarification","question":"¿A qué hora exacta quieres encender los aspersores?"}
{"version":1,"type":"unsupported","summary":"Las puertas solo se monitorean; el sistema no puede abrirlas."}
```

---

## 2. Paso (`step`)

```json
{
  "action": "set_device",
  "params": { "device": "luz_entrada", "state": "on" },
  "when": { "type": "at", "time": "20:00" },
  "condition": { "type": "any_open", "target": "doors" }
}
```

| Campo       | Obligatorio | Descripción |
|-------------|-------------|-------------|
| `action`    | **sí** | Nombre de una función del catálogo (sección 3). |
| `params`    | según la acción | Parámetros de la acción. Claves desconocidas → `unknown_param`. |
| `when`      | no (por defecto: inmediato) | Cuándo ejecutar (sección 4). |
| `condition` | no | Condición evaluada **con los sensores reales al momento de ejecutar** (sección 5). |

---

## 3. Catálogo de acciones autorizadas

El LLM **solo puede elegir** entre estas funciones C++ (implementadas en `Agent::do*`).
No existe ninguna forma de ejecutar código arbitrario, cambiar pines o acceder a la red.

| `action` | Parámetros | Programable | Efecto |
|----------|------------|:-----------:|--------|
| `set_device` | `device` (id de luz/aspersor), `state` (`on`\|`off`) | ✅ | Enciende/apaga una salida. |
| `set_group` | `group` (`lights`\|`sprinklers`\|`all`), `state` (`on`\|`off`) | ✅ | Enciende/apaga un grupo. |
| `notify` | `message` (texto ≤ 160), `level` (`info`\|`warning`\|`alert`, por defecto `warning`) | ✅ | Notificación en la interfaz, LED de alerta, zumbador (nivel `alert`) y Telegram (opcional). |
| `read_access` | `target` (`doors`\|`windows`\|`accesses`\|id), `filter` (`all`\|`open`\|`closed`, por defecto `all`) | ❌ | Informa el estado de puertas/ventanas. |
| `list_tasks` | — | ❌ | Lista las tareas programadas. |
| `cancel_task` | `task_id` (entero ≥ 1) **o** `all: true` | ❌ | Cancela una tarea o todas. |
| `system_status` | — | ❌ | Resumen del sistema. |

«Programable ❌» significa que es una consulta inmediata: no admite `when` ni `condition`
(error `not_schedulable`).

### Dispositivos

| id | Tipo | GPIO | | id | Tipo | GPIO |
|----|------|------|-|----|------|------|
| `luz_entrada` | luz | 4 | | `puerta_principal` | puerta | 8 |
| `luz_oficina` | luz | 5 | | `puerta_bodega` | puerta | 9 |
| `luz_parqueadero` | luz | 6 | | `puerta_oficina` | puerta | 10 |
| `luz_bodega` | luz | 7 | | `ventana_oficina` | ventana | 11 |
| `aspersor_frontal` | aspersor | 15 | | `ventana_recepcion` | ventana | 12 |
| `aspersor_trasero` | aspersor | 16 | | | | |

### Grupos

- **Salidas:** `lights` (4 luces), `sprinklers` (2 aspersores), `all` (las 6 salidas).
- **Accesos:** `doors` (3 puertas), `windows` (2 ventanas), `accesses` (las 5).

> El catálogo vive en `src/core/DeviceCatalog.cpp`. Si se agrega un dispositivo allí,
> el prompt del LLM, la validación, `/api/catalog` y la interfaz web lo incluyen solos.
> (El esquema JSON de `docs/schema` es documentación y debe actualizarse a mano.)

---

## 4. Temporización (`when`)

| Forma | Significado |
|-------|-------------|
| `{"type":"now"}` | Inmediato (equivale a omitir `when`). |
| `{"type":"at","time":"HH:MM"}` | Hoy a esa hora local; si ya pasó, **mañana**. 24 h, siempre dos dígitos (`"08:05"`). |
| `{"type":"at","time":"HH:MM","date":"YYYY-MM-DD"}` | Fecha concreta. Si ya pasó → `time_in_past`. |
| `{"type":"at","time":"HH:MM","repeat":"daily"}` | Todos los días a esa hora. |
| `{"type":"delay","seconds":N}` | N segundos desde ahora (1 … 604 800 = 7 días). |
| `{"type":"after_step","step":I,"seconds":N}` | N segundos después del paso I (índice desde 0, **anterior** a este). Permite secuencias. |

**Resolución:** al aceptar el plan, cada `when` se convierte en un instante absoluto
(epoch). Ejemplo del escenario 3: paso 0 a las 18:00 → paso 1 (`after_step` 0 + 900 s) a las 18:15.

**Reloj:** cualquier paso no inmediato exige que la hora esté sincronizada por NTP;
si no, el plan completo se rechaza con `clock_not_synced` (las órdenes inmediatas sí se aceptan).

---

## 5. Condiciones (`condition`)

| Forma | Se cumple si... |
|-------|-----------------|
| `{"type":"all_closed","target":T}` | todos los accesos de T están cerrados. |
| `{"type":"any_open","target":T}` | al menos un acceso de T está abierto. |
| `{"type":"is_open","target":ID}` | esa puerta/ventana está abierta. |
| `{"type":"is_closed","target":ID}` | esa puerta/ventana está cerrada. |

`T` = `doors`, `windows`, `accesses` o el id de una puerta/ventana.

La condición se evalúa **localmente, en el instante de ejecución**, leyendo los
interruptores (con antirrebote). El LLM nunca «decide» si una puerta está abierta.
Si la condición no se cumple, el paso queda con estado `skipped` y el mensaje indica
el motivo (por ejemplo: «Condición no cumplida (si las puertas están cerradas): abiertas: Puerta de la bodega»).

En `notify`, cuando la condición se cumple, el firmware añade al mensaje la lista
real de accesos abiertos: «Hay puertas abiertas (abiertas: Puerta principal)».

---

## 6. Reglas de validación (resumen)

El plan es **atómico**: si cualquier paso tiene un error, **no se ejecuta ninguno**.

| Código | Causa |
|--------|-------|
| `invalid_root` | La raíz no es un objeto JSON. |
| `unsupported_version` | `version` distinto de 1. |
| `no_json` / `invalid_json` | La respuesta del LLM no contiene JSON o está mal formado. |
| `missing_field` | Falta un campo obligatorio (`type`, `steps`, `action`, `time`...). |
| `unknown_field` | Clave no permitida en el plan, paso, `when` o `condition`. |
| `unknown_action` | La acción no está en el catálogo. |
| `unknown_param` / `missing_param` | Parámetro no permitido / obligatorio ausente. |
| `invalid_type` | Tipo incorrecto (p. ej. `"900"` en lugar de `900`). |
| `invalid_value` | Valor fuera de la lista permitida (`state: "encendida"`). |
| `unknown_device` | Id de dispositivo inexistente. |
| `device_not_output` | Se intentó encender/apagar una puerta o ventana. |
| `unknown_target` | Objetivo de consulta/condición inválido. |
| `invalid_time` / `invalid_date` | Hora o fecha con formato o rango inválido. |
| `invalid_delay` | Segundos no enteros, negativos o > 7 días. |
| `too_long` | `notify.message` supera 160 caracteres. |
| `invalid_step_ref` | `after_step` apunta al mismo paso o a uno posterior. |
| `not_schedulable` | Se intentó programar una consulta. |
| `invalid_params` | Combinación inválida (`cancel_task` sin id ni `all`, `repeat` sin `at`...). |
| `too_many_steps` / `empty_plan` | Más de 8 pasos / ninguno. |
| `unexpected_steps` / `unexpected_field` | Pasos en una aclaración, `question` en un plan... |
| `time_in_past` | Fecha explícita ya vencida. |
| `clock_not_synced` | Hay pasos programados y la hora no está sincronizada. |
| `scheduler_full` | No hay espacio para más tareas (máx. 24). |

---

## 7. Ejemplos de los escenarios del enunciado

**Escenario 1** — «A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta»

```json
{"version":1,"type":"plan","summary":"A las 20:00 encender luz de entrada y aspersores, y avisar si hay puertas abiertas",
 "steps":[
  {"action":"set_device","params":{"device":"luz_entrada","state":"on"},"when":{"type":"at","time":"20:00"}},
  {"action":"set_group","params":{"group":"sprinklers","state":"on"},"when":{"type":"at","time":"20:00"}},
  {"action":"notify","params":{"message":"Hay puertas abiertas","level":"alert"},"when":{"type":"at","time":"20:00"},
   "condition":{"type":"any_open","target":"doors"}}]}
```

**Escenario 2** — «Revisa las puertas y, si todas están cerradas, apaga todas las luces»

```json
{"version":1,"type":"plan","summary":"Revisar puertas y apagar las luces si todas están cerradas",
 "steps":[
  {"action":"read_access","params":{"target":"doors","filter":"all"}},
  {"action":"set_group","params":{"group":"lights","state":"off"},"condition":{"type":"all_closed","target":"doors"}}]}
```

**Escenario 3** — «Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero»

```json
{"version":1,"type":"plan","summary":"18:00 luz de oficina; 15 min después luz del parqueadero",
 "steps":[
  {"action":"set_device","params":{"device":"luz_oficina","state":"on"},"when":{"type":"at","time":"18:00"}},
  {"action":"set_device","params":{"device":"luz_parqueadero","state":"on"},"when":{"type":"after_step","step":0,"seconds":900}}]}
```

**Escenario 4** — «¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?»

```json
{"version":1,"type":"plan","summary":"Consultar accesos abiertos y tareas programadas",
 "steps":[
  {"action":"read_access","params":{"target":"accesses","filter":"open"}},
  {"action":"list_tasks"}]}
```

**Ambigüedad** — «Enciende los aspersores más tarde» → el firmware ni siquiera consulta al LLM
(barrera determinista `findAmbiguousTime`) y responde con `outcome: "clarification"`. Si la
frase escapa a esa barrera, el LLM está instruido para responder `type: "clarification"`.

---

## 8. Formato de persistencia (NVS)

Las tareas pendientes se guardan como un blob JSON en la NVS (`Preferences`, espacio
`agente`, clave `tasks`). El campo `step` usa este mismo formato (sin `when`) y **vuelve a
pasar por el validador** al recuperarse tras un reinicio; las entradas dañadas se descartan.

```json
{"next_id":7,"tasks":[
  {"id":3,"due":1762477200,"created":1762470000,"repeat":"daily","origin":"A las 8:00 p. m. enciende...",
   "step":{"action":"set_device","params":{"device":"luz_entrada","state":"on"}}}]}
```

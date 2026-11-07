# Manual de la API REST

El ESP32-S3 expone una API HTTP con cuerpos **JSON (UTF-8)** en el puerto **80**.
La misma API la usa la interfaz web incluida; cualquier cliente (curl, Python,
Postman, una app móvil) puede usarla igual.

| | |
|---|---|
| **URL base** | `http://agente-esp32.local` (mDNS) o `http://<IP-del-ESP32>` · en modo sin Wi-Fi: `http://192.168.4.1` |
| **Formato** | `Content-Type: application/json; charset=utf-8` en solicitudes y respuestas |
| **Autenticación** | Opcional: `Authorization: Bearer <token>` si se definió `SECRET_DEVICE_API_TOKEN` |
| **CORS** | Habilitado (`Access-Control-Allow-Origin: *`) |
| **Tamaño máximo del cuerpo** | 4 096 bytes |
| **Cliente de ejemplo** | [`tools/agent_cli.py`](../tools/agent_cli.py) (solo biblioteca estándar de Python) |

> En los ejemplos se usa `$ESP=http://agente-esp32.local`. En Windows (PowerShell) reemplace
> `curl` por `curl.exe` y escape las comillas del JSON o use un archivo con `--data @archivo.json`.

---

## Índice

| Método | Ruta | Descripción |
|--------|------|-------------|
| GET | [`/api/health`](#get-apihealth) | Comprobación de vida (sin autenticación) |
| GET | [`/api/status`](#get-apistatus) | Estado general del sistema |
| GET | [`/api/catalog`](#get-apicatalog) | Acciones, dispositivos, grupos y límites permitidos |
| GET | [`/api/devices`](#get-apidevices) | Salidas y entradas con su estado actual |
| POST | [`/api/devices/set`](#post-apidevicesset) | Control manual de una salida |
| GET | [`/api/accesses`](#get-apiaccesses) | Estado de puertas y ventanas |
| POST | [`/api/command`](#post-apicommand) | **Instrucción en lenguaje natural** (asíncrona) |
| GET | [`/api/jobs?id=N`](#get-apijobsidn) | Resultado de una instrucción |
| POST | [`/api/plan`](#post-apiplan) | Ejecutar un plan JSON directamente (sin LLM) |
| POST | [`/api/plan/validate`](#post-apiplanvalidate) | Validar un plan sin ejecutarlo |
| GET | [`/api/tasks`](#get-apitasks) | Listar tareas programadas |
| DELETE | [`/api/tasks?id=N`](#delete-apitasks) | Cancelar una tarea (o todas con `all=1`) |
| POST | [`/api/tasks/cancel`](#post-apitaskscancel) | Cancelar (alternativa a DELETE) |
| GET | [`/api/events?since=N`](#get-apievents) | Eventos y alertas |
| POST | [`/api/events/ack`](#post-apieventsack) | Reconocer alertas |
| GET | `/` | Interfaz web |

---

## Conceptos clave

### El flujo de una instrucción

```
POST /api/command {"text": "..."}        ──►  202 {"job_id": 7}
                                                  │  (tarea de red en segundo plano)
                                                  ▼
                       barrera de ambigüedad → LLM → JSON → validación → ejecución/programación
                                                  │
GET /api/jobs?id=7  (cada ~1 s)          ──►  {"status":"running"} ... {"status":"done","result":{...}}
```

La llamada al LLM tarda entre 1 y 20 s. Por eso `/api/command` **no espera**: responde
`202 Accepted` con un `job_id` y el cliente consulta `/api/jobs` hasta que `status` sea
`done`. Mientras tanto el servidor web, el monitoreo de puertas y el planificador siguen
funcionando con normalidad.

### El objeto `CommandResult`

Lo devuelven `/api/jobs` (dentro de `result`), `/api/plan`, `/api/plan/validate` y
`/api/devices/set`. Es siempre la **verdad del firmware**, no lo que el LLM dijo que haría.

```json
{
  "outcome": "mixed",
  "message": "Luz de la bodega encendida | Programado para 06/11 18:00 (tarea #4)",
  "plan":    { "version": 1, "type": "plan", "summary": "...", "steps": [ ... ] },
  "results": [
    { "step": 0, "action": "set_device", "description": "encender Luz de la bodega",
      "status": "executed", "message": "Luz de la bodega encendida" },
    { "step": 1, "action": "set_device", "description": "encender Luz de la oficina",
      "status": "scheduled", "task_id": 4, "due": 1762470000,
      "due_local": "2026-11-06T18:00:00-05:00", "message": "Programado para 06/11 18:00 (tarea #4)" }
  ],
  "errors": [],
  "llm": { "provider": "anthropic", "model": "claude-opus-5-5", "latency_ms": 3120, "http_status": 200, "attempts": 1 },
  "llm_output": "{\"version\":1,\"type\":\"plan\", ...}"
}
```

| Campo | Descripción |
|-------|-------------|
| `outcome` | Resultado global (tabla siguiente). |
| `message` | Resumen legible construido con los resultados reales. |
| `question` | Solo en `clarification`: la pregunta para el usuario. |
| `source` | En `clarification`: `local_guard` (barrera del firmware) o `llm`. |
| `plan` | Plan **normalizado** tal como lo entendió el firmware (presente si el JSON fue válido). |
| `results[]` | Un elemento por paso: `status`, `message`, y según el caso `task_id`, `due`, `due_local`, `repeat`, `condition_met`, `data`. |
| `errors[]` | Errores de validación o de LLM: `{code, field, message}`. Ver [JSON_FORMAT.md §6](JSON_FORMAT.md#6-reglas-de-validación-resumen). |
| `llm` | Solo en `/api/command`: proveedor, modelo, latencia, código HTTP, intentos. |
| `llm_output` | Solo en `/api/command`: texto crudo devuelto por el LLM (máx. 1 500 caracteres), útil para auditoría. |

**Valores de `outcome`:**

| `outcome` | Significado | HTTP en `/api/plan` |
|-----------|-------------|:---:|
| `executed` | Todos los pasos eran inmediatos y se ejecutaron (o se omitieron por condición). | 200 |
| `scheduled` | Todos los pasos quedaron programados. | 200 |
| `mixed` | Algunos se ejecutaron y otros se programaron. | 200 |
| `valid` | Solo en `/api/plan/validate`: el plan es correcto (no se ejecutó nada). | 200 |
| `clarification` | Falta información; ver `question`. Nada se guardó. | 200 |
| `unsupported` | La orden no corresponde a ninguna acción. Nada se ejecutó. | 200 |
| `rejected` | El JSON o sus parámetros son inválidos. **Nada se ejecutó** (plan atómico). | 422 |
| `error` | Falló el LLM o la red. Nada se ejecutó. | — (va dentro del job) |

**Valores de `results[].status`:** `executed`, `scheduled`, `skipped` (condición no cumplida),
`failed` (p. ej. cancelar una tarea inexistente), `would_execute` / `would_schedule` (validación).

### Errores HTTP

Todos los errores «de transporte» usan el mismo formato:

```json
{ "error": { "code": "invalid_json", "message": "JSON mal formado: InvalidInput" } }
```

| HTTP | `code` | Causa |
|------|--------|-------|
| 400 | `empty_body`, `invalid_json`, `missing_text`, `text_too_long`, `invalid_state`, `unknown_target`, `invalid_filter`, `missing_id` | Solicitud mal formada |
| 401 | `unauthorized` | Falta o es incorrecto el token Bearer |
| 404 | `not_found`, `job_not_found`, `task_not_found` | Recurso inexistente |
| 413 | `body_too_large` | Cuerpo > 4 096 bytes |
| 422 | — (se devuelve un `CommandResult` con `outcome: "rejected"`) | Plan inválido |
| 503 | `busy` | Ya hay 6 instrucciones en cola/proceso |

---

## Referencia de rutas

### `GET /api/health`

Siempre responde (no requiere token). Útil para monitoreo.

```bash
curl $ESP/api/health
```
```json
{ "status": "ok", "firmware": "1.0.0", "uptime_s": 523, "auth_required": false }
```

---

### `GET /api/status`

Estado completo. **Nunca** incluye claves: solo indica si están configuradas.

```json
{
  "firmware": "1.0.0",
  "uptime_s": 523,
  "time":    { "synced": true, "timezone": "<-05>5", "epoch": 1762469940, "local": "2026-11-06T17:59:00-05:00" },
  "network": { "connected": true, "ssid": "Oficina", "ip": "192.168.1.60", "rssi": -58,
               "hostname": "agente-esp32", "ap_active": false, "ap_ssid": "", "ap_ip": "" },
  "llm":     { "provider": "anthropic", "model": "claude-opus-5-5", "configured": true, "jobs_pending": 0 },
  "notifications": { "local_led": true, "telegram": false, "telegram_sent": 0, "telegram_failed": 0 },
  "agent":   { "open_accesses": ["puerta_bodega"], "outputs_on_list": ["luz_entrada"],
               "outputs_on": 1, "accesses_open": 1, "tasks": 2, "tasks_capacity": 24,
               "alerts_unacknowledged": 0, "last_event_id": 31 },
  "memory":  { "heap_free": 214332, "heap_min": 180112 },
  "security":{ "api_token_required": false, "secrets_file_missing": false }
}
```

`time.synced = false` significa que el NTP aún no respondió: solo se aceptan órdenes inmediatas.

---

### `GET /api/catalog`

Describe lo que el sistema acepta. Se genera de las mismas tablas que usa el validador
y el prompt del LLM, así que siempre está sincronizado con el firmware.

```json
{
  "actions": [
    { "name": "set_device", "description": "Enciende o apaga UNA luz o aspersor.", "schedulable": true,
      "params": [
        { "name": "device", "type": "string", "required": true, "description": "id de una luz o aspersor del catálogo" },
        { "name": "state", "type": "enum", "required": true, "allowed": "on|off", "description": "on = encender, off = apagar" } ] },
    "..."
  ],
  "devices": [ { "id": "luz_entrada", "name": "Luz de la entrada", "kind": "light", "io": "output", "pin": 4 }, "..." ],
  "groups": { "outputs": ["lights", "sprinklers", "all"], "accesses": ["doors", "windows", "accesses"] },
  "conditions": ["all_closed", "any_open", "is_open", "is_closed"],
  "timing": ["now", "at", "delay", "after_step"],
  "limits": { "max_steps": 8, "max_tasks": 24, "max_delay_s": 604800, "max_command_chars": 400 }
}
```

---

### `GET /api/devices`

```json
{
  "devices": [
    { "id": "luz_entrada", "name": "Luz de la entrada", "kind": "light", "pin": 4, "io": "output", "on": true,  "state": "on" },
    { "id": "puerta_principal", "name": "Puerta principal", "kind": "door", "pin": 8, "io": "input", "open": false, "state": "closed" }
  ]
}
```

---

### `POST /api/devices/set`

Control manual (los botones de la interfaz). Internamente construye un plan de un paso
`set_device` y lo pasa **por el mismo validador** que las órdenes del LLM.

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `device` | texto | Id de una luz o aspersor |
| `state` | `"on"` \| `"off"` \| `true` \| `false` | Estado deseado |

```bash
curl -X POST $ESP/api/devices/set -H "Content-Type: application/json" \
     -d '{"device":"luz_oficina","state":"on"}'
```
```json
{ "outcome": "executed", "message": "Luz de la oficina encendida", "plan": { "...": "..." },
  "results": [ { "step": 0, "action": "set_device", "description": "encender Luz de la oficina",
                 "status": "executed", "message": "Luz de la oficina encendida" } ] }
```

Con `"device":"puerta_principal"` responde **422** con el error `device_not_output`.

---

### `GET /api/accesses`

| Parámetro | Valores | Por defecto |
|-----------|---------|-------------|
| `target` | `doors`, `windows`, `accesses` o id de puerta/ventana | `accesses` |
| `filter` | `all`, `open`, `closed` | `all` |

```bash
curl "$ESP/api/accesses?target=doors&filter=open"
```
```json
{ "accesses": [ { "id": "puerta_bodega", "name": "Puerta de la bodega", "kind": "door", "open": true } ] }
```

---

### `POST /api/command`

Envía una instrucción en lenguaje natural. **Asíncrona.**

| Campo | Tipo | Obligatorio | Descripción |
|-------|------|:-----------:|-------------|
| `text` | texto (≤ 400) | sí | La instrucción. |
| `context.request` | texto | no | Si `text` responde a una aclaración: la solicitud original. |
| `context.question` | texto | no | La pregunta de aclaración que se hizo. |

```bash
curl -X POST $ESP/api/command -H "Content-Type: application/json" \
     -d '{"text":"Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero"}'
```
**202 Accepted**
```json
{ "job_id": 7, "status": "queued", "poll": "/api/jobs?id=7" }
```

**Flujo de aclaración:**

```bash
# 1) Orden ambigua
curl -X POST $ESP/api/command -d '{"text":"Enciende los aspersores más tarde"}' -H "Content-Type: application/json"
# -> job.result = {"outcome":"clarification","question":"Mencionaste \"mas tarde\"... ¿A qué hora...?","source":"local_guard"}

# 2) Respuesta con contexto
curl -X POST $ESP/api/command -H "Content-Type: application/json" -d '{
  "text": "a las 7:30 p. m.",
  "context": {"request": "Enciende los aspersores más tarde",
              "question": "¿A qué hora (por ejemplo 20:30) o en cuántos minutos quieres que lo haga?"}}'
# -> outcome "scheduled"
```

Errores: `400 missing_text`, `400 text_too_long`, `503 busy`.

---

### `GET /api/jobs?id=N`

```json
{ "job_id": 7, "status": "running", "input": "Enciende la luz...", "elapsed_ms": 1840 }
```
Cuando termina:
```json
{
  "job_id": 7, "status": "done", "input": "Enciende la luz de la oficina a las 6:00 p. m. y, ...", "elapsed_ms": 3410,
  "result": {
    "outcome": "scheduled",
    "message": "Programado para 06/11 18:00 (tarea #4) | Programado para 06/11 18:15 (tarea #5)",
    "plan": { "version": 1, "type": "plan", "summary": "18:00 luz de oficina; 15 min después luz del parqueadero", "steps": [ "..." ] },
    "results": [
      { "step": 0, "action": "set_device", "description": "encender Luz de la oficina", "due": 1762470000,
        "due_local": "2026-11-06T18:00:00-05:00", "status": "scheduled", "task_id": 4, "message": "Programado para 06/11 18:00 (tarea #4)" },
      { "step": 1, "action": "set_device", "description": "encender Luz del parqueadero", "due": 1762470900,
        "due_local": "2026-11-06T18:15:00-05:00", "status": "scheduled", "task_id": 5, "message": "Programado para 06/11 18:15 (tarea #5)" }
    ],
    "llm": { "provider": "anthropic", "model": "claude-opus-5-5", "latency_ms": 3350, "http_status": 200, "attempts": 1 },
    "llm_output": "{\"version\":1,\"type\":\"plan\",...}"
  }
}
```

`status`: `queued` → `running` → `done`. Se conservan los últimos 6 jobs; uno más antiguo
devuelve `404 job_not_found`.

**Errores del LLM** (dentro de `result`, con `outcome: "error"`):

| `errors[0].code` | Causa | Qué hacer |
|------------------|-------|-----------|
| `llm_not_configured` | No hay clave en `agente_secrets.h` | Configurar la clave o usar `/api/plan` |
| `no_network` | Sin Wi-Fi | Esperar reconexión; el control manual sigue disponible |
| `llm_network` | Fallo de conexión/TLS/tiempo de espera | Se reintenta 1 vez automáticamente |
| `llm_http` | El proveedor respondió 4xx/5xx (clave inválida, sin saldo, sobrecarga) | Revisar `message` |
| `llm_refusal` | El modelo declinó la solicitud | Reformular |
| `llm_truncated` | Respuesta cortada por límite de tokens | Reformular más corto |
| `llm_bad_response` | Respuesta ilegible o vacía | Reintentar |

---

### `POST /api/plan`

Ejecuta un plan JSON **sin pasar por el LLM**. El cuerpo es exactamente el formato de
[JSON_FORMAT.md](JSON_FORMAT.md). Sirve para pruebas, automatizaciones externas y para
demostrar que el firmware rechaza planes inválidos.

```bash
curl -X POST $ESP/api/plan -H "Content-Type: application/json" \
     --data @examples/plans/03_escenario2_condicional.json
```
**200** (con la puerta de la bodega abierta):
```json
{
  "outcome": "executed",
  "message": "Abiertas: Puerta de la bodega. Cerradas: Puerta principal, Puerta de la oficina | Condición no cumplida (si las puertas están cerradas): abiertas: Puerta de la bodega. No se ejecutó: apagar las luces",
  "results": [
    { "step": 0, "action": "read_access", "description": "consultar las puertas", "status": "executed",
      "message": "Abiertas: Puerta de la bodega. Cerradas: Puerta principal, Puerta de la oficina",
      "data": { "accesses": [ { "id": "puerta_principal", "name": "Puerta principal", "kind": "door", "open": false }, "..." ],
                "open_count": 1, "closed_count": 2 } },
    { "step": 1, "action": "set_group", "description": "apagar las luces si las puertas están cerradas",
      "status": "skipped", "condition_met": false, "message": "Condición no cumplida (...)" }
  ]
}
```

**422** con un plan inválido:
```bash
curl -X POST $ESP/api/plan -H "Content-Type: application/json" \
     -d '{"type":"plan","steps":[{"action":"set_device","params":{"device":"luz_oficina","state":"on"},"when":{"type":"at","time":"8 pm"}}]}'
```
```json
{
  "outcome": "rejected",
  "message": "El plan no pasó la validación. No se ejecutó nada.",
  "errors": [ { "code": "invalid_time", "field": "steps[0].when.time",
                "message": "Hora inválida: '8 pm' (formato HH:MM, 00:00-23:59)" } ]
}
```

---

### `POST /api/plan/validate`

Igual que `/api/plan` pero **no ejecuta ni programa nada** (modo simulación). Resuelve los
horarios para mostrar cuándo se ejecutaría cada paso.

```json
{
  "outcome": "valid", "message": "Plan válido (simulación: no se ejecutó nada).",
  "results": [ { "step": 0, "action": "set_device", "description": "encender Luz de la oficina",
                 "due": 1762470000, "due_local": "2026-11-06T18:00:00-05:00", "status": "would_schedule" } ]
}
```

---

### `GET /api/tasks`

Tareas pendientes ordenadas por hora de ejecución.

```json
{
  "tasks": [
    { "id": 4, "description": "encender Luz de la oficina", "due": 1762470000, "due_local": "2026-11-06T18:00:00-05:00",
      "repeat": "none", "created": 1762466400, "origin": "Enciende la luz de la oficina a las 6:00 p. m. y, quince...",
      "step": { "action": "set_device", "params": { "device": "luz_oficina", "state": "on" } } }
  ],
  "count": 1,
  "capacity": 24
}
```

---

### `DELETE /api/tasks`

| Consulta | Efecto |
|----------|--------|
| `?id=4` | Cancela la tarea 4 → `{"cancelled":1,"id":4}` o `404 task_not_found` |
| `?all=1` | Cancela todas → `{"cancelled":3}` |

```bash
curl -X DELETE "$ESP/api/tasks?id=4"
```

### `POST /api/tasks/cancel`

Alternativa para clientes que no soportan DELETE. Cuerpo: `{"id": 4}` o `{"all": true}`.

---

### `GET /api/events`

Registro circular (últimos 40) de eventos: cambios de puertas/ventanas, encendidos,
tareas ejecutadas, alertas, red, NTP, fallos del LLM.

| Parámetro | Descripción | Por defecto |
|-----------|-------------|-------------|
| `since` | Devuelve solo eventos con `id` mayor a este | `0` |
| `limit` | Máximo de eventos (1-40) | `20` |

```json
{
  "events": [
    { "id": 30, "time": "2026-11-06T19:59:58-05:00", "uptime_s": 7311, "level": "info", "source": "access", "message": "Puerta principal abierta" },
    { "id": 31, "time": "2026-11-06T20:00:00-05:00", "uptime_s": 7313, "level": "alert", "source": "notify", "message": "Hay puertas abiertas (abiertas: Puerta principal)" }
  ],
  "last_id": 31,
  "alerts_unacknowledged": 1
}
```

`level`: `info`, `warning`, `alert`. `time` es `null` si el evento ocurrió antes de sincronizar la hora
(en ese caso use `uptime_s`). Los niveles `warning` y `alert` encienden el **LED de alerta** hasta que se
reconozcan, y se reenvían a Telegram si está configurado.

### `POST /api/events/ack`

Reconoce todas las alertas pendientes (apaga el parpadeo del LED). Sin cuerpo.

```json
{ "alerts_unacknowledged": 0 }
```

---

## Ejemplos completos

### Bash / curl: escenario 1 de principio a fin

```bash
ESP=http://agente-esp32.local
JOB=$(curl -s -X POST $ESP/api/command -H "Content-Type: application/json" \
  -d '{"text":"A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta"}' \
  | python -c "import sys,json;print(json.load(sys.stdin)['job_id'])")
until curl -s "$ESP/api/jobs?id=$JOB" | grep -q '"status":"done"'; do sleep 1; done
curl -s "$ESP/api/jobs?id=$JOB" | python -m json.tool
curl -s $ESP/api/tasks | python -m json.tool
```

### Python

```python
import json, time, urllib.request

ESP = "http://agente-esp32.local"

def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(ESP + path, data=data, method=method,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req) as r:
        return json.load(r)

job = call("POST", "/api/command", {"text": "¿Qué puertas o ventanas están abiertas?"})["job_id"]
while (res := call("GET", f"/api/jobs?id={job}"))["status"] != "done":
    time.sleep(1)
print(res["result"]["message"])
```

O directamente: `python tools/agent_cli.py say "¿Qué puertas o ventanas están abiertas?"`.

### JavaScript (navegador)

```js
const r = await fetch("/api/command", {method: "POST", headers: {"Content-Type": "application/json"},
                                        body: JSON.stringify({text: "Apaga todas las luces"})});
const {job_id} = await r.json();
let job; do { await new Promise(s => setTimeout(s, 800)); job = await (await fetch(`/api/jobs?id=${job_id}`)).json(); }
while (job.status !== "done");
console.log(job.result.outcome, job.result.message);
```

### Con token

```bash
curl -H "Authorization: Bearer mi-token-secreto" $ESP/api/status
python tools/agent_cli.py --token mi-token-secreto status
```

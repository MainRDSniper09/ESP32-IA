# Manual de la API

La página web se comunica con el ESP32 mediante una pequeña **API**: direcciones a las
que se envían **peticiones (Request)** y que devuelven **respuestas (Response)** en JSON.
Cualquier programa (navegador, `curl`, Python, Postman) puede usarla igual.

- **Dirección base:** `http://<IP-del-ESP32>` (la IP aparece en el Monitor Serie).
  Sin Wi-Fi: `http://192.168.4.1`.
- **Formato:** JSON en UTF-8.

| Método | Ruta | Para qué |
|--------|------|----------|
| GET | `/` | Abre la página web |
| GET | `/api/estado` | Estado de dispositivos, tareas y avisos |
| POST | `/api/orden` | Enviar una orden en lenguaje natural |
| POST | `/api/plan` | Enviar un plan JSON directo (sin LLM) |
| POST | `/api/cancelar` | Cancelar una tarea |
| POST | `/api/alertas/vistas` | Marcar las alertas como vistas (apaga el LED rojo) |

## Códigos de respuesta

| Código | Significado |
|:--:|-------------|
| 200 | Todo bien |
| 400 | La petición está mal (JSON mal escrito, falta `texto`...) |
| 404 | No existe (ruta o tarea) |
| 422 | El plan tenía errores: **no se ejecutó nada** |
| 502 | Falló la consulta al LLM |
| 503 | Sin internet: no se puede consultar el LLM |

---

## GET `/api/estado`

Devuelve todo lo que muestra la página. La página la consulta cada 2 segundos.

**Request:** sin cuerpo.

```bash
curl http://192.168.1.60/api/estado
```

**Response 200:**
```json
{
  "hora": "06/11 17:58",
  "wifi": "MiRed",
  "alertas_pendientes": 1,
  "dispositivos": [
    {"id": "luz_entrada", "nombre": "Luz de la entrada", "pin": 4, "tipo": "luz", "encendido": true},
    {"id": "puerta_principal", "nombre": "Puerta principal", "pin": 8, "tipo": "puerta", "abierto": false}
  ],
  "tareas": [
    {"id": 3, "cuando": "06/11 20:00", "accion": "encender", "objetivo": "luz_entrada", "condicion": "", "mensaje": ""}
  ],
  "avisos": [
    {"hora": "17:57:02", "texto": "Puerta de la bodega se abrió", "alerta": false}
  ]
}
```

| Campo | Significado |
|-------|-------------|
| `hora` | Hora actual, o `"sin sincronizar"` si aún no hay NTP |
| `wifi` | Red conectada, o modo respaldo |
| `alertas_pendientes` | Alertas sin revisar (LED rojo parpadeando si es > 0) |
| `dispositivos` | Luces/aspersores con `encendido`; puertas/ventanas con `abierto` |
| `tareas` | Tareas programadas pendientes |
| `avisos` | Últimos 20 avisos, del más reciente al más antiguo |

---

## POST `/api/orden`

Envía una orden en español. El ESP32 consulta al LLM, valida el JSON y lo ejecuta.
Tarda unos segundos (lo que tarde el LLM).

**Request:**
```json
{"texto": "Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero"}
```

```bash
curl -X POST http://192.168.1.60/api/orden -d "{\"texto\": \"Apaga todas las luces\"}"
```

**Response 200 — orden ejecutada o programada:**
```json
{
  "orden": "Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero",
  "json_del_llm": "{\"tipo\":\"plan\",\"mensaje\":\"18:00 oficina y 18:15 parqueadero\",\"acciones\":[...]}",
  "errores": [],
  "resultados": [
    "Tarea #4 programada para 06/11 18:00: encender luz_oficina",
    "Tarea #5 programada para 06/11 18:15: encender luz_parqueadero"
  ],
  "ok": true,
  "tipo": "plan",
  "mensaje": "18:00 oficina y 18:15 parqueadero"
}
```

**Response 200 — orden ambigua (se pide aclaración, no se guarda nada):**
```json
{"orden": "Enciende los aspersores más tarde", "ok": true, "tipo": "aclaracion",
 "pregunta": "Tu orden no dice una hora exacta. ¿A qué hora (por ejemplo 20:30) o en cuántos minutos?"}
```
Para responder, se envía de nuevo la orden con la aclaración, por ejemplo:
`{"texto": "Enciende los aspersores más tarde. Aclaración: a las 19:30"}` (la página lo hace sola).

**Response 200 — orden imposible:**
```json
{"ok": true, "tipo": "no_soportado", "mensaje": "Las puertas solo se vigilan; no se pueden abrir."}
```

**Response 422 — el LLM devolvió algo inválido:**
```json
{"ok": false, "tipo": "rechazado", "mensaje": "La orden tiene errores. No se ejecutó nada.",
 "errores": ["Acción 1: el dispositivo 'luz_cocina' no existe"], "json_del_llm": "..."}
```

**Response 502 / 503 — falla del LLM o sin internet:**
```json
{"ok": false, "mensaje": "La API respondió con error HTTP 401: invalid x-api-key. No se ejecutó nada."}
```

| Campo | Significado |
|-------|-------------|
| `ok` | `true` si la orden se procesó bien |
| `tipo` | `plan`, `aclaracion`, `no_soportado` o `rechazado` |
| `mensaje` | Resumen o explicación |
| `pregunta` | Solo en `aclaracion` |
| `resultados` | Qué se hizo, una línea por acción |
| `errores` | Por qué se rechazó |
| `json_del_llm` | El JSON exacto que respondió el LLM (para revisarlo) |

---

## POST `/api/plan`

Igual que `/api/orden`, pero **sin LLM**: se envía directamente el JSON del plan
(formato en [FORMATO_JSON.md](FORMATO_JSON.md)). Sirve para probar el sistema sin
internet o sin clave, y para comprobar que el ESP32 rechaza JSON incorrectos.
Los botones *Encender/Apagar* de la página usan esta ruta.

**Request:**
```json
{"tipo": "plan", "acciones": [{"accion": "encender", "dispositivo": "luz_bodega"}]}
```

```bash
curl -X POST http://192.168.1.60/api/plan -d @ejemplos/1_control_inmediato.json
```

**Response 200:**
```json
{"errores": [], "resultados": ["Luz de la bodega: encendido"], "ok": true, "tipo": "plan", "mensaje": ""}
```

**Response 422** (con `ejemplos/invalido_hora_mal_escrita.json`):
```json
{"errores": ["Acción 1: hora inválida, use el formato HH:MM de 24 horas (ej. \"20:00\")"],
 "ok": false, "tipo": "rechazado", "mensaje": "La orden tiene errores. No se ejecutó nada."}
```

---

## POST `/api/cancelar`

**Request:** `{"id": 4}`

```bash
curl -X POST http://192.168.1.60/api/cancelar -d "{\"id\": 4}"
```

**Response 200:** `{"ok": true, "mensaje": "Tarea #4 cancelada"}`
**Response 404:** `{"ok": false, "mensaje": "No existe la tarea #4"}`

---

## POST `/api/alertas/vistas`

Marca las alertas como revisadas: el LED rojo deja de parpadear. Sin cuerpo.

**Response 200:** `{"ok": true}`

---

## Ejemplo en Python

```python
import json, urllib.request

ESP = "http://192.168.1.60"

def enviar(ruta, datos):
    peticion = urllib.request.Request(ESP + ruta, data=json.dumps(datos).encode(), method="POST")
    with urllib.request.urlopen(peticion) as r:
        return json.load(r)

respuesta = enviar("/api/orden", {"texto": "¿Qué puertas están abiertas?"})
print(respuesta["resultados"])
```

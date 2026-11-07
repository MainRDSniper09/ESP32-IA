# Operación y comportamiento ante fallos

El enunciado pide documentar qué ocurre **sin Wi-Fi**, **cuando falla la API** y **después
de reiniciar**. Esta es la especificación del comportamiento implementado, con el archivo
donde vive cada regla.

## 1. Resumen

| Situación | Monitoreo puertas/ventanas | Control manual | Tareas ya programadas | Órdenes nuevas en lenguaje natural | Planes JSON directos |
|-----------|:---:|:---:|:---:|:---:|:---:|
| Normal | ✅ | ✅ | ✅ | ✅ | ✅ |
| Sin Wi-Fi (hora ya sincronizada) | ✅ | ✅ (vía AP) | ✅ | ❌ `no_network` | ✅ (vía AP) |
| Sin Wi-Fi desde el arranque (sin hora) | ✅ | ✅ (vía AP) | ⏸ esperan la hora | ❌ | ✅ solo inmediatos |
| Falla la API del LLM | ✅ | ✅ | ✅ | ❌ `llm_*` (nada se ejecuta) | ✅ |
| Tras reiniciar | ✅ | ✅ | ✅ recuperadas de NVS | ✅ | ✅ |

## 2. Sin Wi-Fi

*Implementación: `platform/NetworkManager.cpp`*

1. Al arrancar se intenta la red configurada durante **20 s**. Mientras tanto la tarea de
   E/S ya está corriendo (el monitoreo empieza antes que la red).
2. Si no conecta, se activa el **punto de acceso de respaldo** `Agente-ESP32`
   (`http://192.168.4.1`), de modo que la interfaz web y la API siguen disponibles localmente.
   Se registra un evento `warning` (LED de alerta).
3. El ESP32 sigue intentando conectarse (modo AP+STA, reintento cada 30 s). Al volver la red,
   el AP se apaga y se registra «Wi-Fi reconectado».
4. Si la red se pierde durante la operación se registra un `warning`. **Las tareas programadas
   se siguen ejecutando**: el reloj interno del ESP32 mantiene la hora mientras no se corte la
   alimentación.
5. Las instrucciones en lenguaje natural responden `outcome: "error"` con código `no_network`.
   **No se ejecuta nada.** El control manual y `POST /api/plan` siguen funcionando.
6. Telegram (si está configurado) acumula hasta 8 mensajes y los envía al reconectar.

## 3. Sin hora sincronizada (NTP)

*Implementación: `core/Agent.cpp` (`resolveTimes`, `tick`), `platform/Esp32Services.cpp`*

- La hora se considera válida cuando es posterior al 01/01/2024 (es decir, el SNTP respondió).
- Mientras no sea válida:
  - Las órdenes **inmediatas** se ejecutan normalmente.
  - Cualquier plan con pasos `at`, `delay` o `after_step` (con retardo) se **rechaza completo**
    con `clock_not_synced`: no tiene sentido programar «a las 20:00» sin saber qué hora es.
  - El planificador **no ejecuta** tareas: las recuperadas de la NVS esperan.
- Cuando el NTP responde se registra «Hora sincronizada por NTP: …» y el planificador arranca.

## 4. Falla de la API del LLM

*Implementación: `llm/LlmClient.cpp`, `platform/JobManager.cpp`*

| Falla | Comportamiento |
|-------|----------------|
| Sin clave configurada | `llm_not_configured`; aviso al arrancar. |
| Error de red / tiempo de espera (45 s) | `llm_network`. **1 reintento** automático con espera de 1,5 s. |
| HTTP 408, 429, 5xx (sobrecarga) | `llm_http`. 1 reintento automático. |
| HTTP 400/401/403/404 | `llm_http` sin reintento (es un error de configuración). |
| El modelo declina (`stop_reason: refusal`) | `llm_refusal`. Con Claude se pide además al servidor un modelo de respaldo automático (`fallbacks: "default"`). |
| Respuesta truncada | `llm_truncated`. |
| Respuesta sin JSON / JSON mal formado | `rejected` con `no_json` / `invalid_json`. |
| JSON válido pero con acciones/parámetros incorrectos | `rejected` con la lista de errores de validación. |

En **todos** los casos:
- **No se ejecuta ninguna acción** (el plan es atómico).
- Se registra un evento `warning` («Fallo al consultar el LLM: …») → LED de alerta.
- El resto del sistema (monitoreo, tareas, control manual) **no se ve afectado**: la llamada
  HTTPS corre en su propia tarea en el núcleo 0.

## 5. Reinicio (corte de energía, *watchdog*, reset)

*Implementación: `core/Agent.cpp` (`begin`, `tick`), `core/TaskScheduler.cpp`, `platform/Esp32Services.cpp`*

1. **Salidas:** arrancan todas **apagadas** (estado seguro). El estado de luces/aspersores
   no se persiste a propósito: tras un corte, nada se enciende solo.
2. **Tareas:** se cargan de la NVS. Cada tarea **vuelve a pasar por el validador**; las
   dañadas se descartan con un `warning`. Se registra «Se recuperaron N tarea(s)…».
3. Las tareas recuperadas **esperan a que el NTP sincronice** la hora.
4. Al sincronizar, para cada tarea vencida:
   - Vencida hace **≤ 10 min** (`AGENT_MISSED_TASK_GRACE_S`): se ejecuta normalmente (un reinicio corto no hace perder la tarea).
   - Vencida hace **> 10 min**: **no se ejecuta**; se marca como perdida y se genera una **alerta**
     («Tarea #4 NO se ejecutó (vencía 06/11 18:00, el equipo estaba apagado o sin hora)»).
     Ejemplo: no se encienden los aspersores a las 3 a. m. porque la luz volvió tarde.
   - Si era **diaria**, se reprograma para la siguiente ocurrencia futura.
5. Los ids de tarea continúan desde el último usado (no se reutilizan).
6. Los eventos y jobs (en RAM) se pierden; las tareas no.

La escritura en NVS ocurre solo cuando cambia la lista de tareas (crear, cancelar,
ejecutar) y se omite si el contenido es idéntico, para no desgastar la flash.

## 6. Entradas inválidas del usuario

| Entrada | Respuesta |
|---------|-----------|
| «Enciende los aspersores **más tarde**» | `clarification` desde la barrera local (sin llamar al LLM). |
| «Apaga la luz **del sótano**» (no existe) | El LLM responde `clarification` o `unsupported`; si inventa `luz_sotano` el validador lo rechaza con `unknown_device`. |
| «**Abre** la puerta principal» | `unsupported` (las puertas solo se monitorean). Si el LLM intentara `set_device` sobre una puerta: `device_not_output`. |
| Texto vacío o > 400 caracteres | HTTP 400. |
| 7 instrucciones simultáneas | La 7.ª recibe HTTP 503 `busy`. |

## 7. Indicadores físicos

| Indicador | Significado |
|-----------|-------------|
| LED verde fijo | Conectado a Wi-Fi |
| LED verde parpadeando (1 Hz) | Sin red / modo AP |
| LED rojo parpadeando (4 Hz) | Hay alertas (`warning`/`alert`) sin reconocer → botón «Reconocer alertas» o `POST /api/events/ack` |
| Pitido corto | Nueva alerta de nivel `alert` |

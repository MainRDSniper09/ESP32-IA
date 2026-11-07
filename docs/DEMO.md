# Guion del video de demostración

Duración sugerida: **5–7 minutos**. Grabe la pantalla (interfaz web + consola serie) y,
en una esquina o en tomas alternas, la protoboard con los LED e interruptores.

## Preparación

- [ ] Firmware grabado, `agente_secrets.h` configurado, hora sincronizada (chip verde «hora»).
- [ ] Interfaz web abierta en `http://agente-esp32.local`, consola serie visible (`pio device monitor`).
- [ ] Todas las puertas/ventanas **cerradas**, todas las salidas **apagadas**, sin tareas.
- [ ] Anotar la hora actual (H). Usaremos H+2 y H+4 min en lugar de 8:00 p. m. / 6:00 p. m.

## Escenas

| # | Tiempo | Qué mostrar | Qué decir |
|---|--------|-------------|-----------|
| 1 | 0:00 | Montaje y diagrama de arquitectura (`docs/ARCHITECTURE.md`). | Problema, objetivo y la idea clave: el LLM interpreta, el firmware valida y ejecuta. |
| 2 | 0:45 | **Control inmediato:** «Enciende la luz de la bodega ahora». Desplegar «Ver JSON». | Muestra el JSON que devolvió el LLM y el plan validado; el LED enciende. |
| 3 | 1:30 | **Consulta de accesos:** abrir el interruptor de la *Puerta bodega*; preguntar «¿Qué puertas o ventanas están abiertas?». | La respuesta sale de los sensores, no del LLM. |
| 4 | 2:15 | **Tarea horaria (escenario 1):** «A las H+2 enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta». Mostrar las 3 tareas en el panel. | Planificación sin bloquear: mientras tanto, abrir y cerrar interruptores y ver la UI actualizarse. |
| 5 | 3:00 | **Secuencia temporizada (escenario 3):** «Enciende la luz de la oficina a las H+3 y, un minuto después, la del parqueadero». | `after_step` en el JSON. |
| 6 | 3:30 | Llega H+2: LED entrada + aspersores encienden; alerta (LED rojo, pitido, aviso en pantalla) porque la puerta de la bodega sigue abierta. «Reconocer alertas». | Condición evaluada localmente al momento de ejecutar. |
| 7 | 4:15 | **Secuencia condicional (escenario 2):** con la puerta abierta, «Revisa las puertas y, si todas están cerradas, apaga todas las luces» → omitido. Cerrar la puerta y repetir → se apagan. | El LLM nunca decide el estado de las puertas. |
| 8 | 5:00 | **Ambigüedad:** «Enciende los aspersores más tarde» → pregunta → responder «en dos minutos». | No se guarda nada hasta aclarar. |
| 9 | 5:30 | **JSON inválido:** panel «Plan JSON directo» con `examples/plans/invalid/accion_desconocida.json` → 422. | Catálogo cerrado: acciones desconocidas se rechazan. |
| 10 | 6:00 | **Reinicio:** con tareas pendientes, pulsar RST. Consola: «Se recuperaron N tarea(s)». La tarea se ejecuta a su hora. | Persistencia en NVS. |
| 11 | 6:30 | **Gestión:** «¿Qué tareas quedaron programadas?» y cancelar una desde la UI. | Cierre. |

## Consejos

- Si el LLM tarda, el chat muestra «Procesando (n s)…»; aproveche para explicar la tarea de red en el núcleo 0.
- Para no depender de internet en la escena 9, todo el panel «Plan JSON directo» funciona sin LLM.
- Los JSON de `examples/plans/` sirven como respaldo si la conexión falla durante la grabación.

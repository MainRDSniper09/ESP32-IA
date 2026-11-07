# Informe de pruebas

Pruebas para demostrar que el sistema cumple los criterios de aceptación del taller.
Se hacen con el prototipo armado ([CONEXIONES.md](CONEXIONES.md)) y la página web abierta.

> Completa la columna **Resultado** al ejecutar cada prueba (✅ / ❌ y lo observado).
> Para no esperar hasta las 8 p. m., usa una hora 2 o 3 minutos en el futuro.

**Antes de empezar:** todas las puertas/ventanas cerradas, todas las luces apagadas, sin tareas,
y la hora sincronizada (arriba en la página debe aparecer la hora, no «sin sincronizar»).

## 1. Control inmediato

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 1.1 | Pulsar *Encender* en «Luz de la entrada» | El LED del pin 4 enciende y la página dice ENCENDIDO | |
| 1.2 | Orden: «Enciende la luz de la bodega» | Respuesta ✅ `plan`, resultado «Luz de la bodega: encendido», LED del pin 7 enciende | |
| 1.3 | Orden: «Apaga todas las luces» | Se apagan los 4 LED de luces | |

## 2. Consulta de accesos

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 2.1 | Abrir el interruptor de la puerta de la bodega | En menos de 1 s aparece ABIERTA y el aviso «Puerta de la bodega se abrió» | |
| 2.2 | Orden: «¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?» | Lista exactamente las abiertas reales y dice las tareas | |

## 3. Tarea horaria (escenario 1)

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 3.1 | Con la puerta principal **abierta**, orden: «A las HH:MM enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta» (HH:MM = dentro de 2 min) | Se crean 3 tareas en la página | |
| 3.2 | Mientras se espera, abrir y cerrar interruptores | La página se sigue actualizando (el sistema no se bloquea) | |
| 3.3 | Llega la hora | Encienden la luz de la entrada y los 2 aspersores; aparece la alerta «Hay puertas abiertas (abiertas: Puerta principal)» y parpadea el LED rojo | |
| 3.4 | Repetir con todas las puertas cerradas | Encienden las salidas pero **no** hay alerta («No se hizo ... porque la condición no se cumple») | |

## 4. Secuencia temporizada (escenario 3)

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 4.1 | Orden: «Enciende la luz de la oficina a las HH:MM y, dos minutos después, enciende la del parqueadero» | 2 tareas, la segunda 2 minutos después de la primera | |
| 4.2 | Esperar | Se encienden en ese orden y a esas horas | |

## 5. Secuencia condicional (escenario 2)

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 5.1 | Luces encendidas y una puerta **abierta**. Orden: «Revisa las puertas y, si todas están cerradas, apaga todas las luces» | Dice cuál puerta está abierta y «No se hizo 'apagar luces' porque la condición no se cumple». Las luces siguen encendidas | |
| 5.2 | Cerrar todas las puertas y repetir | Las luces se apagan | |

## 6. Instrucciones ambiguas o inválidas

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 6.1 | Orden: «Enciende los aspersores más tarde» | ✅ `aclaracion` con una pregunta. **No** se crea ninguna tarea | |
| 6.2 | Responder: «a las HH:MM» | Ahora sí se programa la tarea | |
| 6.3 | Orden: «Abre la puerta principal» | `no_soportado` o rechazo; nada cambia | |
| 6.4 | Orden: «Enciende la luz de la cocina» | Pregunta o rechazo («no existe»); nada cambia | |
| 6.5 | `curl -X POST http://IP/api/plan -d @ejemplos/invalido_accion_desconocida.json` | HTTP 422, «acción no permitida 'borrar_memoria'» | |
| 6.6 | Lo mismo con `invalido_hora_mal_escrita.json` | HTTP 422, «hora inválida» | |
| 6.7 | Lo mismo con `invalido_abrir_puerta.json` | HTTP 422, «es una puerta/ventana: solo se puede consultar» | |
| 6.8 | `curl -X POST http://IP/api/plan -d "{esto no es json"` | HTTP 400, «no es un JSON válido» | |

## 7. Gestión de tareas

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 7.1 | Programar 2 tareas y pulsar *Cancelar* en una | Desaparece de la lista; aviso «Tarea #N cancelada» | |
| 7.2 | Orden: «Cancela la tarea N» | Se cancela | |

## 8. Recuperación tras reinicio

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 8.1 | Programar una tarea para dentro de 5 minutos y pulsar el botón **RST** del ESP32 | Al volver: aviso «Se recuperaron 1 tarea(s)...» y la tarea sigue en la lista con el mismo número | |
| 8.2 | Esperar la hora | La tarea se ejecuta normalmente | |
| 8.3 | Programar una tarea a 2 min, desconectar el ESP32 15 min y volver a conectarlo | La tarea **no** se ejecuta tarde; aparece la alerta «La tarea #N ... NO se ejecutó» | |

## 9. Fallos de red y del LLM

| # | Qué hacer | Resultado esperado | Resultado |
|---|-----------|--------------------|-----------|
| 9.1 | Poner una clave de API incorrecta y enviar una orden | Error «HTTP 401 ...», alerta, nada se ejecuta | |
| 9.2 | Apagar el router con tareas programadas | Alerta «Se perdió el Wi-Fi»; las tareas se ejecutan igual a su hora | |
| 9.3 | Encender el ESP32 sin router | Aparece la red «Agente-ESP32»; en 192.168.4.1 los botones funcionan | |

## Verificación del código

- El sketch compila sin errores ni advertencias para **ESP32-S3** (core Arduino-ESP32 2.0.17,
  ArduinoJson 7): usa ~17 % de la RAM y ~29 % de la memoria de programa.
- Las pruebas de esta página se hacen en el prototipo físico y se registran arriba.

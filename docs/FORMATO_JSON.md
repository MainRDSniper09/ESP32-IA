# Formato JSON de las instrucciones

Es el "idioma" que comparten el LLM y el ESP32. El LLM traduce el español a este JSON;
el ESP32 lo revisa (`plan.cpp`) y, si está bien, lo ejecuta.

El mismo formato se puede enviar directamente a `POST /api/plan` sin usar el LLM
(útil para probar). Hay ejemplos listos en la carpeta [`ejemplos/`](../ejemplos).

## Estructura

```json
{
  "tipo": "plan",
  "mensaje": "Resumen corto de lo que se hará",
  "acciones": [
    { "accion": "encender", "dispositivo": "luz_entrada", "hora": "20:00" }
  ]
}
```

| Campo | Obligatorio | Valores |
|-------|:-----------:|---------|
| `tipo` | sí | `plan` · `aclaracion` · `no_soportado` |
| `mensaje` | depende | En `plan`: resumen. En `aclaracion`: **la pregunta**. En `no_soportado`: la explicación. |
| `acciones` | solo en `plan` | Lista de 1 a 6 acciones |

- **`plan`**: la orden es clara → se ejecuta.
- **`aclaracion`**: falta información (por ejemplo «más tarde») → se pregunta al usuario y **no se guarda nada**.
- **`no_soportado`**: la orden no se puede hacer (por ejemplo «abre la puerta», porque las puertas solo se vigilan).

## Campos de cada acción

| Campo | Tipo | Para qué sirve |
|-------|------|----------------|
| `accion` | texto | **Obligatorio.** Una de las 6 acciones de la tabla siguiente |
| `dispositivo` | texto | Qué encender/apagar o qué consultar |
| `hora` | texto `"HH:MM"` | Hora de 24 h (`"20:00"`, `"06:30"`). Si ya pasó hoy, será mañana |
| `minutos_despues` | número entero (0–1440) | Minutos a esperar: después de `hora`, o desde ahora si no hay `hora` |
| `condicion` | texto | Solo se ejecuta si se cumple (se revisa con los interruptores reales) |
| `mensaje` | texto (máx. 100) | Texto del aviso (solo para `avisar`) |
| `id` | número entero | Número de la tarea (solo para `cancelar_tarea`) |

Si una acción no tiene `hora` ni `minutos_despues`, se ejecuta **inmediatamente**.

### Las 6 acciones permitidas

| `accion` | Campos que usa | ¿Se puede programar? |
|----------|----------------|:--:|
| `encender` | `dispositivo` | ✅ |
| `apagar` | `dispositivo` | ✅ |
| `avisar` | `mensaje` | ✅ |
| `consultar_accesos` | `dispositivo`: `puertas`, `ventanas` o `todo` (por defecto `todo`) | ❌ (inmediata) |
| `listar_tareas` | — | ❌ (inmediata) |
| `cancelar_tarea` | `id` | ❌ (inmediata) |

### Dispositivos (`dispositivo`)

- **Uno solo:** `luz_entrada`, `luz_oficina`, `luz_parqueadero`, `luz_bodega`, `aspersor_frontal`, `aspersor_trasero`
- **Grupos:** `luces`, `aspersores`, `todo`
- **Solo para consultar:** `puertas`, `ventanas`, `todo`

Las puertas y ventanas (`puerta_principal`, `ventana_oficina`...) **no** se pueden encender ni apagar.

### Condiciones (`condicion`)

| Valor | Se cumple cuando... |
|-------|---------------------|
| `puertas_cerradas` | todas las puertas están cerradas |
| `alguna_puerta_abierta` | al menos una puerta está abierta |
| `ventanas_cerradas` | todas las ventanas están cerradas |
| `alguna_ventana_abierta` | al menos una ventana está abierta |

## Reglas de validación

El ESP32 **rechaza toda la orden** (no ejecuta nada) si encuentra cualquiera de estos errores:

| Error | Ejemplo |
|-------|---------|
| No es JSON o está mal escrito | `{"tipo": "plan", "acciones": [` |
| `tipo` desconocido | `"tipo": "quizas"` |
| Campo no permitido | `"pin": 4` |
| Acción no permitida | `"accion": "borrar_memoria"` |
| Dispositivo que no existe | `"dispositivo": "luz_cocina"` |
| Encender una puerta o ventana | `"accion": "encender", "dispositivo": "puerta_principal"` |
| Hora mal escrita | `"hora": "8 pm"` (debe ser `"20:00"`) |
| Tipo de dato incorrecto | `"minutos_despues": "15"` (debe ser `15`, sin comillas) |
| Condición desconocida | `"condicion": "si_llueve"` |
| Programar una consulta | `listar_tareas` con `"hora"` |
| Falta un dato | `encender` sin `dispositivo`, `avisar` sin `mensaje` |
| Más de 6 acciones, o ninguna | |
| Programar sin tener la hora del NTP | |
| No hay espacio para más tareas (máx. 20) | |

## Los escenarios del taller

**1.** «A las 8:00 p. m. enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta»
```json
{"tipo": "plan", "mensaje": "A las 20:00: luz de entrada, aspersores y aviso si hay puertas abiertas",
 "acciones": [
   {"accion": "encender", "dispositivo": "luz_entrada", "hora": "20:00"},
   {"accion": "encender", "dispositivo": "aspersores",  "hora": "20:00"},
   {"accion": "avisar", "mensaje": "Hay puertas abiertas", "hora": "20:00", "condicion": "alguna_puerta_abierta"}
 ]}
```

**2.** «Revisa las puertas y, si todas están cerradas, apaga todas las luces»
```json
{"tipo": "plan", "mensaje": "Revisar puertas y apagar luces si están cerradas",
 "acciones": [
   {"accion": "consultar_accesos", "dispositivo": "puertas"},
   {"accion": "apagar", "dispositivo": "luces", "condicion": "puertas_cerradas"}
 ]}
```

**3.** «Enciende la luz de la oficina a las 6:00 p. m. y, quince minutos después, enciende la del parqueadero»
```json
{"tipo": "plan", "mensaje": "18:00 oficina y 18:15 parqueadero",
 "acciones": [
   {"accion": "encender", "dispositivo": "luz_oficina",     "hora": "18:00"},
   {"accion": "encender", "dispositivo": "luz_parqueadero", "hora": "18:00", "minutos_despues": 15}
 ]}
```

**4.** «¿Qué puertas o ventanas están abiertas? ¿Qué tareas quedaron programadas?»
```json
{"tipo": "plan", "mensaje": "Consultar accesos y tareas",
 "acciones": [
   {"accion": "consultar_accesos", "dispositivo": "todo"},
   {"accion": "listar_tareas"}
 ]}
```

**Orden ambigua:** «Enciende los aspersores más tarde»
```json
{"tipo": "aclaracion", "mensaje": "¿A qué hora exacta quieres encender los aspersores?"}
```

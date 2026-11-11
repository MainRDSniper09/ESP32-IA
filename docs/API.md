# Manual de la API

La página web habla con la ESP32 mediante una pequeña **API**: direcciones a las que se
envía una **petición (Request)** y que devuelven una **respuesta (Response)** en JSON.
Cualquier programa (navegador, `curl`, PowerShell, Postman) puede usarla igual.

- **Dirección base:** `http://<IP-de-la-ESP32>` (aparece en el Monitor Serie). Sin Wi-Fi: `http://192.168.4.1`.
- **Formato:** JSON en UTF-8.

| Método | Ruta | Para qué |
|--------|------|----------|
| GET | `/` | La página web |
| GET | `/api/estado` | Estado de cada luz |
| POST | `/api/comando` | Enviar una orden en texto y recibir el análisis con porcentajes |

---

## GET `/api/estado`

**Request:** sin cuerpo.

```bash
curl http://192.168.1.60/api/estado
```

**Response 200:**
```json
{
  "luces": [
    {"nombre": "Recepción",      "pin": 4,  "encendida": false},
    {"nombre": "Oficina",        "pin": 5,  "encendida": true},
    {"nombre": "Sala de juntas", "pin": 6,  "encendida": false},
    {"nombre": "Cocina",         "pin": 7,  "encendida": false},
    {"nombre": "Baño",           "pin": 15, "encendida": false},
    {"nombre": "Bodega",         "pin": 16, "encendida": true}
  ]
}
```

---

## POST `/api/comando`

Envía una orden en texto. La ESP32 la interpreta, cambia los LED si la entendió y responde
con el análisis completo. Es la misma ruta que usa la página después de convertir la voz en texto.

**Request:**
```json
{"texto": "apaga la luz de la cosina"}
```

**PowerShell (Windows):**
```powershell
Invoke-RestMethod -Method Post -Uri http://192.168.1.60/api/comando -Body '{"texto": "apaga la luz de la cosina"}'
```

**curl (CMD, macOS, Linux):**
```bash
curl -X POST http://192.168.1.60/api/comando -d "{\"texto\": \"apaga la luz de la cosina\"}"
```

**Response 200:**
```json
{
  "texto": "apaga la luz de la cosina",
  "normalizado": "apaga la luz de la cosina",
  "palabras_clave": "apaga cosina",
  "entendido": true,
  "accion": "apagar",
  "confianza": 91,
  "umbral": 70,
  "mensaje": "Listo: apagué la luz de: Cocina",
  "encender": {"porcentaje": 43,  "dicha": "cosina", "conocida": "ilumina"},
  "apagar":   {"porcentaje": 100, "dicha": "apaga",  "conocida": "apaga"},
  "todas":    {"porcentaje": 33,  "dicha": "cosina", "conocida": "toda"},
  "lugares": [
    {"nombre": "Recepción",      "porcentaje": 29, "dicha": "apaga",  "conocida": "entrada"},
    {"nombre": "Oficina",        "porcentaje": 43, "dicha": "cosina", "conocida": "oficina"},
    {"nombre": "Sala de juntas", "porcentaje": 40, "dicha": "apaga",  "conocida": "sala"},
    {"nombre": "Cocina",         "porcentaje": 83, "dicha": "cosina", "conocida": "cocina"},
    {"nombre": "Baño",           "porcentaje": 22, "dicha": "apaga",  "conocida": "sanitario"},
    {"nombre": "Bodega",         "porcentaje": 38, "dicha": "cosina", "conocida": "deposito"}
  ],
  "afectadas": ["Cocina"]
}
```

Cada concepto muestra la palabra dicha que **más** se le parece, aunque el parecido sea bajo.
Solo cuentan para la decisión los que llegan al **70 %**: aquí *apagar* (100 %) y *Cocina* (83 %).

| Campo | Significado |
|-------|-------------|
| `texto` | La orden tal como llegó |
| `normalizado` | En minúsculas, sin tildes ni signos |
| `palabras_clave` | Las palabras que quedaron tras quitar las vacías («la», «de», «luz»...) |
| `entendido` | `true` si supo qué hacer **y** dónde; solo entonces cambia los LED |
| `accion` | `encender`, `apagar` o `desconocida` |
| `confianza` | Promedio entre el % de la acción y el de los lugares elegidos |
| `umbral` | % mínimo para aceptar una coincidencia (70) |
| `mensaje` | Explicación para el usuario |
| `encender`, `apagar`, `todas` | Mejor coincidencia de cada concepto: `porcentaje`, palabra `dicha` y palabra `conocida` |
| `lugares` | Lo mismo para cada habitación |
| `afectadas` | Habitaciones cuyo LED se cambió (vacío si no entendió) |

**Orden no entendida** (`{"texto": "enciende la luz"}`):
```json
{
  "entendido": false,
  "accion": "encender",
  "confianza": 50,
  "mensaje": "Entendí que quieres encender, pero no en qué lugar.",
  "afectadas": []
}
```
(Se muestran solo algunos campos.)

**Response 400**, si el cuerpo no es un JSON con `texto`:
```json
{"error": "Envía un JSON como {\"texto\": \"apaga la cocina\"}"}
```

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
| GET | `/api/modelo` | El modelo TF-IDF entrenado: corpus, vocabulario e IDF |
| POST | `/api/comando` | Enviar una orden en texto y recibir el análisis TF-IDF con porcentajes |

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

## GET `/api/modelo`

Devuelve lo que la ESP32 calculó al **entrenar** (al encender): cuántos documentos tiene el
corpus, cada palabra del vocabulario con el número de documentos que la contienen y su IDF,
y las frases del corpus.

**Request:** sin cuerpo.

```bash
curl http://192.168.1.60/api/modelo
```

**Response 200** (recortada):
```json
{
  "documentos": 44,
  "vocabulario": [
    {"palabra": "enciende", "documentos": 1,  "idf": 3.784},
    {"palabra": "luz",      "documentos": 19, "idf": 0.84},
    {"palabra": "sala",     "documentos": 3,  "idf": 2.686},
    {"palabra": "cocina",   "documentos": 2,  "idf": 3.091}
  ],
  "corpus": [
    {"frase": "enciende la luz",  "intencion": "encender"},
    {"frase": "apaga la luz",     "intencion": "apagar"},
    {"frase": "luz de la cocina", "intencion": "cocina"}
  ]
}
```

| Campo | Significado |
|-------|-------------|
| `documentos` | Total de frases del corpus (N) |
| `vocabulario[].documentos` | En cuántas frases aparece la palabra |
| `vocabulario[].idf` | `log(N / documentos)`: más alto = palabra más rara y más importante |
| `corpus` | Cada frase de ejemplo y su intención |

---

## POST `/api/comando`

Envía una orden en texto. La ESP32 la interpreta con TF-IDF, cambia los LED si la entendió
y responde con el análisis completo. Es la misma ruta que usa la página después de convertir
la voz en texto.

**Request:**
```json
{"texto": "apaga la luz de la cocina"}
```

**PowerShell (Windows):**
```powershell
Invoke-RestMethod -Method Post -Uri http://192.168.1.60/api/comando -Body '{"texto": "apaga la luz de la cocina"}'
```

**curl (CMD, macOS, Linux):**
```bash
curl -X POST http://192.168.1.60/api/comando -d "{\"texto\": \"apaga la luz de la cocina\"}"
```

**Response 200:**
```json
{
  "texto": "apaga la luz de la cocina",
  "normalizado": "apaga la luz de la cocina",
  "entendido": true,
  "accion": "apagar",
  "confianza": 71,
  "umbral": 30,
  "mensaje": "Listo: apagué la luz de: Cocina",
  "pesos": [
    {"palabra": "apaga",  "conocida": true, "tf": 0.333, "idf": 3.784, "tfidf": 1.261},
    {"palabra": "luz",    "conocida": true, "tf": 0.333, "idf": 0.84,  "tfidf": 0.28},
    {"palabra": "cocina", "conocida": true, "tf": 0.333, "idf": 3.091, "tfidf": 1.03}
  ],
  "encender": {"porcentaje": 4,  "frase": "enciende la luz"},
  "apagar":   {"porcentaje": 78, "frase": "apaga la luz"},
  "todas":    {"porcentaje": 0,  "frase": ""},
  "lugares": [
    {"nombre": "Recepción",      "porcentaje": 4,  "frase": "luz de la recepción"},
    {"nombre": "Oficina",        "porcentaje": 4,  "frase": "luz de la oficina"},
    {"nombre": "Sala de juntas", "porcentaje": 3,  "frase": "luz de la sala de juntas"},
    {"nombre": "Cocina",         "porcentaje": 65, "frase": "luz de la cocina"},
    {"nombre": "Baño",           "porcentaje": 4,  "frase": "luz del baño"},
    {"nombre": "Bodega",         "porcentaje": 4,  "frase": "luz de la bodega"}
  ],
  "afectadas": ["Cocina"]
}
```

Cada intención muestra la frase del corpus **más parecida** a la orden y su porcentaje de
similitud coseno. Solo cuentan para la decisión las que llegan al **30 %**: aquí *apagar*
(78 %) y *Cocina* (65 %). Las demás habitaciones dan 4 % porque solo comparten la palabra
«luz», que tiene un IDF muy bajo.

| Campo | Significado |
|-------|-------------|
| `texto` | La orden tal como llegó |
| `normalizado` | En minúsculas, sin tildes ni signos |
| `entendido` | `true` si supo qué hacer **y** dónde; solo entonces cambia los LED |
| `accion` | `encender`, `apagar` o `desconocida` |
| `confianza` | Promedio entre el % de la acción y el de los lugares elegidos |
| `umbral` | % mínimo de similitud para aceptar una intención (30) |
| `mensaje` | Explicación para el usuario |
| `pesos` | Cada palabra de la orden (sin las vacías) con su `tf`, `idf` y `tfidf`. `conocida: false` = no está en el corpus y se ignora |
| `encender`, `apagar`, `todas` | Mejor coincidencia de cada intención: `porcentaje` y `frase` del corpus |
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

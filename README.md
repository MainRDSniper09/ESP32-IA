# 🎙️ Asistente de voz con ESP32-S3 — Luces de una empresa

Proyecto final de **Inteligencia Artificial y Sistemas Embebidos** · **Parte 1**

Un asistente de voz que controla las luces de una empresa de **un solo piso**. Cada
habitación tiene su luz, representada por un **LED**. Le dices:

> 🗣️ «Apaga la luz de la cocina»

…y la ESP32 **interpreta la frase ella misma** (sin internet ni inteligencia artificial
externa) con el algoritmo **TF-IDF**: convierte la orden en un vector de pesos, calcula el
**porcentaje de similitud** (similitud coseno) con las frases de ejemplo que conoce, decide
qué quisiste decir y **apaga el LED de la cocina**. Un LED verde confirma que entendió.

```
 "apaga la luz de la cocina"

  palabra    TF      IDF     TF-IDF
  apaga      0.333   3.784   1.261
  luz        0.333   0.840   0.280     ← "luz" está en muchas frases: pesa poco
  cocina     0.333   3.091   1.030

  ENCENDER        ..........    4%
  APAGAR          #######...   78%  ~ "apaga la luz"       ← acción elegida
  Oficina         ..........    4%  ~ "luz de la oficina"
  Cocina          ######....   65%  ~ "luz de la cocina"   ← lugar elegido (llega al 30 %)
  ...
  OK  Listo: apagué la luz de: Cocina   (confianza 71 %)
```

---

## 📋 Contenido

1. [¿Qué hace?](#-qué-hace)
2. [¿Cómo funciona?](#-cómo-funciona)
3. [Conexiones](#-conexiones)
4. [Puesta en marcha rápida](#-puesta-en-marcha-rápida)
5. [Cómo usarlo](#️-cómo-usarlo)
6. [Organización del código](#-organización-del-código)
7. [Documentación](#-documentación)

---

## ✨ ¿Qué hace?

| Lo que dices | Lo que hace |
|--------------|-------------|
| «Apaga la luz de la cocina» | Apaga **un** LED |
| «Enciende la oficina y la recepción» | Enciende **varios** LED |
| «Prende todas las luces» / «Apaga las luces» | Enciende o apaga **todos** |
| «Desactiva el almacén» | Entiende **sinónimos** (almacén = bodega) |
| «Enciende la luz» | Pregunta **dónde** (LED rojo) |
| «Hola, ¿cómo estás?» | Dice que **no entendió** (LED rojo) |

Además muestra, para cada orden, la **tabla TF-IDF** (peso de cada palabra) y el
**porcentaje de similitud** con cada intención, tanto en la página web como en el Monitor Serie.

## 🧠 ¿Cómo funciona?

```mermaid
flowchart LR
    A([🗣️ Voz]) -->|micrófono del celular/PC| B[Navegador:<br/>voz → texto]
    B -->|"apaga la luz de la cocina"| C[ESP32-S3]
    K([⌨️ Monitor Serie]) --> C
    C --> D[1. Limpiar texto<br/>apaga · luz · cocina]
    D --> E[2. Vector TF-IDF<br/>de la orden]
    E --> E2[3. Similitud coseno<br/>con cada frase del corpus<br/>→ % de similitud]
    E2 --> F{4. ¿Llega al 30 %?}
    F -->|sí| G[💡 Cambiar LED<br/>+ LED verde]
    F -->|no| H[❓ Pedir más datos<br/>+ LED rojo]
```

1. **Voz a texto:** lo hace el navegador (Google Chrome) con su micrófono. La ESP32 recibe el texto.
2. **Limpieza:** minúsculas, sin tildes ni signos; se quitan palabras sin significado ("la", "de", "por favor"...).
3. **Entrenamiento (al encender):** la ESP32 tiene un **corpus** de 44 frases de ejemplo,
   cada una con su significado («apaga la luz» → apagar, «luz de la cocina» → cocina...).
   Calcula el **IDF** de cada palabra y el **vector TF-IDF** de cada frase.
4. **TF-IDF de la orden:**
   - **TF** = veces que aparece la palabra / total de palabras de la orden.
   - **IDF** = log(total de frases / frases que contienen la palabra). Las palabras comunes
     («luz») pesan poco; las distintivas («cocina», «apaga») pesan mucho.
   - **Peso = TF × IDF.**
5. **Similitud coseno:** compara el vector de la orden con el de cada frase del corpus →
   **porcentaje de similitud** de 0 % a 100 %.
6. **Decisión:**
   - **Qué hacer:** la acción (encender/apagar) con mayor porcentaje.
   - **Dónde:** las habitaciones que lleguen al **30 %**, o todas.
7. **Acción:** cambia los LED y parpadea el verde (entendió) o el rojo (no entendió).

➡️ Explicación completa, con un ejemplo calculado a mano y cómo "entrenarlo" con frases nuevas: **[docs/COMO_FUNCIONA.md](docs/COMO_FUNCIONA.md)**

## 🔌 Conexiones

![Diagrama de conexiones](docs/diagrama_conexiones.svg)

| GPIO | LED | Representa |
|:--:|-----|------------|
| 4 | 🟡 amarillo | Recepción |
| 5 | 🟡 amarillo | Oficina |
| 6 | 🟡 amarillo | Sala de juntas |
| 7 | 🟡 amarillo | Cocina |
| 15 | 🟡 amarillo | Baño |
| 16 | 🟡 amarillo | Bodega |
| 17 | 🟢 verde | Orden entendida |
| 18 | 🔴 rojo | No entendí |

Cada LED: `GPIO → resistencia de 220 Ω → pata larga del LED · pata corta → GND`

**Materiales:** 1 ESP32-S3 · 6 LED amarillos · 1 verde · 1 rojo · 8 resistencias de 220 Ω ·
protoboard · cables · cable USB de datos.

➡️ Detalles: **[docs/CONEXIONES.md](docs/CONEXIONES.md)**

## 🚀 Puesta en marcha rápida

> 📘 ¿Primera vez? Sigue el **[tutorial paso a paso](docs/TUTORIAL.md)**.

1. **Arduino IDE 2** → instala la placa **esp32** (Espressif, versión 2.0.17) y la librería **ArduinoJson** (v7).
2. Abre `AgenteESP32/AgenteESP32.ino`.
3. Crea en esa carpeta un archivo **`credenciales.h`** con tu Wi-Fi (no se sube a GitHub):
   ```cpp
   #define WIFI_NOMBRE  "MiRed"
   #define WIFI_CLAVE   "MiClave"
   ```
4. Placa **ESP32S3 Dev Module**, tu puerto COM → **Subir**.
5. Abre el **Monitor Serie (115200)** y copia la dirección que aparece, por ejemplo `http://192.168.1.60`.
6. Ábrela en **Google Chrome** (en la misma red Wi-Fi), pulsa 🎤 y habla.

**No necesitas ninguna clave de API ni pagar nada:** toda la interpretación ocurre dentro de la ESP32.

## 🖥️ Cómo usarlo

Tienes tres formas de darle órdenes. Las tres pasan por el **mismo intérprete**:

| Forma | Cómo |
|-------|------|
| 🎤 **Voz** | En la página, pulsa el micrófono y habla (Google Chrome) |
| ⌨️ **Texto en la página** | Escribe la orden y pulsa *Enviar*, o toca una habitación del plano |
| 🔌 **Monitor Serie** | Escribe la orden en el Arduino IDE y pulsa Enter (no necesita Wi-Fi) |

La página muestra un **plano de la empresa** (las habitaciones se iluminan cuando su LED está
encendido), la **tabla TF-IDF** y los **porcentajes de similitud** de la última orden, y el
**modelo entrenado** (vocabulario e IDF de cada palabra).

También se puede usar desde otros programas con la **API**:

```bash
curl -X POST http://192.168.1.60/api/comando -d "{\"texto\": \"apaga la luz de la cocina\"}"
```

➡️ Manual de la API: **[docs/API.md](docs/API.md)**

## 📁 Organización del código

```
ESP32-IA/
├── AgenteESP32/
│   └── AgenteESP32.ino      ← TODO el programa (C++ para Arduino IDE)
├── docs/                    ← documentación y diagrama
├── README.md
└── LICENSE
```

`AgenteESP32.ino` está dividido en 11 secciones comentadas:

| # | Sección | Qué contiene |
|:--:|---------|--------------|
| 1 | Wi-Fi | Nombre y clave de la red (o `credenciales.h`) y red de respaldo |
| 2 | Habitaciones y pines | Tabla con cada habitación, su intención y su pin |
| 3 | Corpus de entrenamiento | Frases de ejemplo con su intención, palabras vacías, umbral del 30 % |
| 4 | Preparar el texto | `normalizar()`, `separarPalabras()`, `contarVeces()` |
| 5 | **El algoritmo TF-IDF** | `construirVocabulario()`, `calcularIDF()`, `calcularVectorTFIDF()`, `similitudCoseno()`, `entrenarModelo()` |
| 6 | El intérprete | `interpretar()`: de texto a decisión (acción, lugares y confianza) |
| 7 | Encender / apagar los LED | Cambiar LED y parpadeo verde/rojo |
| 8 | Monitor Serie | Mostrar el modelo entrenado, recibir órdenes y mostrar sus tablas |
| 9 | Página web | HTML, CSS y JavaScript (micrófono, plano, tablas, modelo) |
| 10 | Servidor web y API | Rutas `/`, `/api/estado`, `/api/modelo`, `/api/comando` |
| 11 | `setup()` y `loop()` | Arranque (incluye entrenar el modelo) y ciclo principal |

## 📚 Documentación

| Documento | Contenido |
|-----------|-----------|
| [TUTORIAL.md](docs/TUTORIAL.md) | Paso a paso: instalar, conectar, subir, abrir la página, activar el micrófono y probar |
| [COMO_FUNCIONA.md](docs/COMO_FUNCIONA.md) | El algoritmo TF-IDF explicado: limpieza, TF, IDF, similitud coseno, decisión, cómo enseñarle frases |
| [CONEXIONES.md](docs/CONEXIONES.md) | Materiales, pines y cómo conectar los LED |
| [API.md](docs/API.md) | Peticiones y respuestas JSON de la API |
| [PRUEBAS.md](docs/PRUEBAS.md) | Lista de pruebas con los porcentajes de similitud esperados |

## 🗺️ Próximas partes

Esta es la **parte 1**: interpretar la orden y controlar los LED. El proyecto se irá
ampliando por partes.

## 📄 Licencia

[MIT](LICENSE) — libre para usar, modificar y compartir. © 2026 Juan Nicolás Barreto Sánchez

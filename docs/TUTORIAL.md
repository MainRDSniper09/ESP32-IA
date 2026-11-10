# Tutorial: de la caja a la página web funcionando

Este tutorial lleva paso a paso desde una ESP32-S3 recién sacada de la caja hasta controlar
los LED escribiendo órdenes en español en la página web. No necesitas experiencia previa.

**Tiempo estimado:** 1 a 2 horas la primera vez.

---

## Índice

0. [Qué necesitas](#0-qué-necesitas)
1. [Instalar el Arduino IDE y el soporte para ESP32](#1-instalar-el-arduino-ide-y-el-soporte-para-esp32)
2. [Conectar la ESP32 al computador](#2-conectar-la-esp32-al-computador)
3. [Prueba rápida: encender un LED](#3-prueba-rápida-encender-un-led)
4. [Armar el circuito completo](#4-armar-el-circuito-completo)
5. [Conseguir la clave de la API (gratis con Gemini)](#5-conseguir-la-clave-de-la-api-gratis-con-gemini)
6. [Descargar el proyecto y poner tus claves](#6-descargar-el-proyecto-y-poner-tus-claves)
7. [Subir el programa a la ESP32](#7-subir-el-programa-a-la-esp32)
8. [Leer el Monitor Serie](#8-leer-el-monitor-serie)
9. [Abrir la página web](#9-abrir-la-página-web)
10. [Recorrido: probar todo paso a paso](#10-recorrido-probar-todo-paso-a-paso)
11. [Hacer peticiones a la API sin la página](#11-hacer-peticiones-a-la-api-sin-la-página)
12. [Problemas comunes](#12-problemas-comunes)

---

## 0. Qué necesitas

**Hardware**

| Cantidad | Elemento |
|:--:|----------|
| 1 | ESP32-S3 DevKitC-1 (o similar) |
| 1 | Cable USB **de datos** (algunos cables solo cargan; esos no sirven) |
| 7 | LED: 4 amarillos o blancos, 2 azules, 1 rojo |
| 7 | Resistencias de 220 Ω (o 330 Ω) |
| 5 | Interruptores deslizantes (o pulsadores) |
| 1 | Protoboard y cables dupont macho-macho |

**Software y cuentas**

- Un computador con Windows, macOS o Linux.
- Una red **Wi-Fi de 2,4 GHz** con internet (la ESP32 no se conecta a redes de 5 GHz).
- Una **cuenta de Google**, para la clave gratuita de la API de Gemini (no pide tarjeta).

---

## 1. Instalar el Arduino IDE y el soporte para ESP32

1. Descarga e instala el **Arduino IDE 2** desde <https://www.arduino.cc/en/software>.
2. Ábrelo y ve a **Archivo → Preferencias**.
3. En **URLs adicionales de gestor de placas** pega esta dirección y pulsa *Aceptar*:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
4. Ve a **Herramientas → Placa → Gestor de placas**.
5. Busca **esp32** e instala el de **Espressif Systems**. En la lista de versiones elige
   **2.0.17**, que es la versión con la que se probó el proyecto. La descarga pesa varios cientos de MB.
6. Ve a **Herramientas → Gestionar bibliotecas**.
7. Busca **ArduinoJson** e instala la de **Benoit Blanchon** (versión 7.x).

---

## 2. Conectar la ESP32 al computador

La ESP32-S3 DevKitC-1 tiene **dos conectores USB**. Usa el que dice **UART** (o **COM**).
Así el Monitor Serie funciona sin cambiar ningún ajuste.

1. Conecta la placa con el cable USB. Debe encenderse un LED rojo de alimentación en la placa.
2. Comprueba que el computador la reconoce:
   - **Windows:** abre el *Administrador de dispositivos*. En *Puertos (COM y LPT)* debe
     aparecer algo como *USB-SERIAL CH343 (COM5)* o *Silicon Labs CP210x (COM5)*.
     Anota el número de COM.
   - **macOS/Linux:** aparece como `/dev/cu.usbserial-...` o `/dev/ttyUSB0`.
3. Si no aparece nada:
   - Prueba otro cable: lo más común es que sea un cable que solo carga.
   - Instala el driver del chip USB de tu placa, [CH343](https://www.wch-ic.com/downloads/CH343SER_EXE.html)
     o [CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).

---

## 3. Prueba rápida: encender un LED

Antes del proyecto completo conviene comprobar que la placa, el cable y el IDE funcionan.

**Conecta un solo LED:**

```
GPIO 4 ──── resistencia 220 Ω ──── pata LARGA del LED
                                   pata CORTA del LED ──── GND
```

**Crea un sketch nuevo** (*Archivo → Nuevo*), pega este código y súbelo (pasos del punto 7):

```cpp
void setup() {
  pinMode(4, OUTPUT);
}

void loop() {
  digitalWrite(4, HIGH);   // enciende
  delay(500);
  digitalWrite(4, LOW);    // apaga
  delay(500);
}
```

Si el LED parpadea, todo está listo. Si no:
- Revisa que la pata larga del LED vaya hacia la resistencia.
- Revisa que la pata corta llegue a un pin **GND** de la placa.

---

## 4. Armar el circuito completo

Guíate por el diagrama: [`diagrama_conexiones.svg`](diagrama_conexiones.svg).

### 4.1 Preparar la protoboard

1. Coloca la ESP32 a lo largo del centro de la protoboard, de modo que cada fila de pines
   quede en un lado distinto del canal central.
2. Con un cable, une un pin **GND** de la placa a la línea azul (−) de la protoboard.
   Esa línea será el **riel de GND**. Si usas las líneas de ambos lados, únelas entre sí.

### 4.2 Conectar los LED (salidas)

Cada LED se conecta igual. Primero va el pin, luego la resistencia, luego el LED y al final GND:

```
GPIO ── resistencia 220 Ω ── pata larga del LED · pata corta ── riel GND
```

| GPIO | LED | Representa |
|:--:|-----|------------|
| 4 | amarillo | Luz de la entrada |
| 5 | amarillo | Luz de la oficina |
| 6 | amarillo | Luz del parqueadero |
| 7 | amarillo | Luz de la bodega |
| 15 | azul | Aspersor frontal |
| 16 | azul | Aspersor trasero |
| 17 | rojo | LED de alerta |

### 4.3 Conectar los interruptores (entradas)

Cada interruptor va **entre el pin y GND**. No lleva resistencia, porque la ESP32 usa una interna.

```
GPIO ──── interruptor ──── riel GND
```

Con un interruptor deslizante de 3 patas: la pata del **centro** va al GPIO y una de las de
los **extremos** va a GND.

| GPIO | Representa |
|:--:|------------|
| 8 | Puerta principal |
| 9 | Puerta de la bodega |
| 10 | Puerta de la oficina |
| 11 | Ventana de la oficina |
| 12 | Ventana de la recepción |

**Cómo se interpreta cada interruptor:**
- **Cerrado** (une el pin con GND) = la puerta está **CERRADA**.
- **Abierto** = la puerta está **ABIERTA**.

Si al final del montaje alguna puerta aparece siempre abierta, revisa su cable a GND.

---

## 5. Conseguir la clave de la API (gratis con Gemini)

El programa usa por defecto **Gemini de Google**, que tiene un nivel **gratuito**:

1. Entra a <https://aistudio.google.com/apikey> e inicia sesión con tu cuenta de Google.
2. Acepta los términos si te los pide y pulsa **Create API key**.
3. Copia la clave. Empieza por `AIza...`.
4. **No compartas la clave ni la subas a GitHub.**

Sobre el nivel gratuito:
- Tiene un **límite de peticiones por minuto y por día**. Para probar el proyecto sobra; si lo
  superas, la página mostrará un error `HTTP 429` y basta con esperar un poco.
- Google puede usar lo que envíes en el nivel gratuito para mejorar sus productos.
  No escribas datos personales o privados en las órdenes.

<details>
<summary>¿Prefieres usar Claude (de pago)?</summary>

1. Crea una clave en <https://console.anthropic.com> → **API Keys** (empieza por `sk-ant-...`)
   y agrega saldo en **Billing**.
2. En la **sección 2** del programa cambia `#define PROVEEDOR_LLM  1` por `#define PROVEEDOR_LLM  2`.
3. Pon esa clave en `LLM_API_KEY`.

</details>

---

## 6. Descargar el proyecto y poner tus claves

1. Entra a <https://github.com/MainRDSniper09/ESP32-IA>.
2. Pulsa el botón verde **Code → Download ZIP** y descomprime el archivo.
3. En el Arduino IDE ve a **Archivo → Abrir** y elige `AgenteESP32/AgenteESP32.ino`.

   > El archivo `.ino` debe estar dentro de una carpeta con su mismo nombre (`AgenteESP32`).
   > Si el IDE ofrece moverlo a una carpeta nueva, acepta.

4. **Crea el archivo de claves dentro del IDE.** Así evitas que el Bloc de notas lo guarde como `.txt`:
   - Pulsa el botón **⋯** (o la flecha ▾) a la derecha de las pestañas → **Nueva pestaña**.
   - Escribe el nombre `credenciales.h` y pulsa *Aceptar*.
   - Pega esto con **tus** datos:
     ```cpp
     #define WIFI_NOMBRE  "NombreDeTuRed"
     #define WIFI_CLAVE   "ClaveDeTuRed"
     #define LLM_API_KEY  "AIza-pega-aqui-tu-clave"
     ```
   - Guarda con **Ctrl + S**.

   El programa detecta `credenciales.h` automáticamente. Ese archivo está en `.gitignore`,
   así que **no se sube a GitHub** aunque publiques la carpeta.

5. **Opcional — tu zona horaria.** Busca en el código la **sección 2** y cambia
   `ZONA_HORARIA` si no estás en Colombia (UTC-5). Por ejemplo, México es `"CST6"`.

---

## 7. Subir el programa a la ESP32

1. **Herramientas → Placa → esp32 → ESP32S3 Dev Module**.
2. **Herramientas → Puerto**: elige el COM que anotaste en el paso 2.
3. Revisa estos ajustes en el menú **Herramientas**; los demás se dejan como están:

   | Ajuste | Valor |
   |--------|-------|
   | USB CDC On Boot | **Disabled** si conectaste el USB "UART/COM" · **Enabled** si usas el USB nativo |
   | Upload Speed | 921600 (si falla, prueba 115200) |

4. Pulsa **✓ Verificar**. La primera compilación tarda 1 o 2 minutos. Debe terminar con algo como
   *"El Sketch usa 959833 bytes (73%)..."*. El porcentaje depende de la partición elegida.
5. Pulsa **→ Subir**. Verás un avance `Writing at 0x... (10 %)` hasta `Hard resetting via RTS pin...`.

**Si sale "Failed to connect to ESP32":**
1. Mantén presionado el botón **BOOT** de la placa.
2. Pulsa **→ Subir**.
3. Suelta BOOT cuando aparezca `Connecting...`.
4. Al terminar, pulsa **RST** una vez.

---

## 8. Leer el Monitor Serie

El Monitor Serie muestra lo que la ESP32 va haciendo. Ahí encontrarás la dirección de la página.

1. **Herramientas → Monitor Serie**. Abajo a la derecha elige **115200 baudio**.
2. Pulsa el botón **RST** de la placa para ver el arranque desde el principio:

```
=== Agente ESP32-S3 ===
Conectando a Wi-Fi.......
Conectado. Abre en el navegador: http://192.168.1.60
[aviso] Wi-Fi conectado (192.168.1.60)
[aviso] Hora sincronizada por internet: 10/11 17:58
```

**Anota la dirección** (`http://192.168.1.60` en el ejemplo; la tuya será distinta).

Si en cambio aparece esto, la ESP32 no pudo conectarse a tu red:

```
Sin Wi-Fi. Conéctate a la red 'Agente-ESP32' y abre http://192.168.4.1
```

Revisa el nombre y la clave en `credenciales.h`, y que la red sea de 2,4 GHz. Mientras tanto,
puedes conectarte a la red **Agente-ESP32** (clave `agente1234`) y abrir `http://192.168.4.1`.
Allí funciona todo excepto las órdenes escritas, porque sin internet no hay LLM.

---

## 9. Abrir la página web

1. Conecta tu celular o computador a la **misma red Wi-Fi** que la ESP32.
2. Abre el navegador y escribe la dirección del paso 8, por ejemplo `http://192.168.1.60`.
3. Arriba deben aparecer la **hora** y el **nombre de la red**. Si dice *"sin sincronizar"*,
   espera unos segundos.

La página está guardada dentro de la ESP32: no hay que instalar nada más.
Tiene cuatro zonas:

| Zona | Para qué sirve |
|------|----------------|
| **1. Escribe una orden** | Cuadro de texto, botones de ejemplo y la respuesta del sistema |
| **💡 Luces y aspersores** | Estado de cada LED y botón para encender/apagar |
| **🚪 Puertas y ventanas** | Estado de cada interruptor, en tiempo real |
| **⏰ Tareas** y **🔔 Avisos** | Lo programado (con botón *Cancelar*) y las notificaciones |

---

## 10. Recorrido: probar todo paso a paso

Antes de empezar:
- Pon todos los interruptores en **cerrado**.
- Apaga todos los LED.

### Prueba 1 — Botones (sin LLM)
1. En *Luces y aspersores* pulsa **Encender** en *Luz de la entrada*.
2. ✅ El LED del GPIO 4 se enciende y la página dice **ENCENDIDO**.
3. Pulsa **Apagar** para dejarlo como estaba.

### Prueba 2 — Interruptores
1. Abre el interruptor de la *Puerta de la bodega* (GPIO 9).
2. ✅ En menos de 2 segundos aparece **ABIERTA** y en *Avisos* sale *"Puerta de la bodega se abrió"*.
3. Vuelve a cerrarlo.

### Prueba 3 — Tu primera orden en español
1. Escribe en el cuadro: `Enciende la luz de la oficina` y pulsa **Enviar orden**.
2. Durante unos segundos verás *"Pensando..."*: la ESP32 está consultando al LLM.
3. ✅ La respuesta dice `✅ plan`, *"Luz de la oficina: encendido"* y el LED del GPIO 5 se enciende.
4. Al final de la respuesta aparece **"JSON que devolvió el LLM"**. Así entendió el LLM tu orden:
   ```json
   {"tipo":"plan","mensaje":"Encender la luz de la oficina",
    "acciones":[{"accion":"encender","dispositivo":"luz_oficina"}]}
   ```

### Prueba 4 — Apagar un grupo
Escribe: `Apaga todas las luces`

✅ Se apagan los 4 LED amarillos.

### Prueba 5 — Consultar puertas y tareas (escenario 4 del taller)
1. Abre el interruptor de la puerta principal.
2. Pulsa el ejemplo **"¿Qué puertas o ventanas están abiertas?..."** y luego **Enviar orden**.
3. ✅ La respuesta dice *"Abiertas: Puerta principal..."* y lista las tareas.
   La información sale de los interruptores reales, no de lo que el LLM imagina.

### Prueba 6 — Tarea programada (escenario 1)
Usa una hora **2 minutos en el futuro**. Si son las 17:58, escribe 18:00.
1. Deja la puerta principal **abierta**.
2. Escribe: `A las 18:00 enciende la luz de la entrada y los aspersores; avísame si alguna puerta está abierta`
3. ✅ Aparecen **3 tareas** en *Tareas programadas*.
4. Mientras esperas, mueve los interruptores: la página se sigue actualizando. El sistema no se bloquea.
5. A la hora indicada:
   - ✅ Se encienden el LED de la entrada y los 2 LED azules.
   - ✅ Aparece la alerta roja *"Hay puertas abiertas (abiertas: Puerta principal)"*.
   - ✅ El **LED rojo parpadea**.
6. Pulsa **Marcar alertas como vistas**: el LED rojo deja de parpadear.

### Prueba 7 — Secuencia temporizada (escenario 3)
1. Escribe: `Enciende la luz de la oficina a las 18:05 y, dos minutos después, enciende la del parqueadero`
2. ✅ Se crean 2 tareas: una a las 18:05 y otra a las 18:07.

### Prueba 8 — Orden con condición (escenario 2)
1. Enciende las luces. Deja **una puerta abierta**.
2. Escribe: `Revisa las puertas y, si todas están cerradas, apaga todas las luces`
3. ✅ Dice cuál puerta está abierta y *"No se hizo 'apagar luces' porque la condición no se cumple"*.
   Las luces siguen encendidas.
4. Cierra todas las puertas y repite la orden.
5. ✅ Ahora sí se apagan.

### Prueba 9 — Orden ambigua
1. Escribe: `Enciende los aspersores más tarde`
2. ✅ El sistema **pregunta** a qué hora y no guarda nada. Debajo del cuadro aparece la pregunta en rojo.
3. Responde, por ejemplo: `a las 18:10`
4. ✅ Ahora sí se programa la tarea.

### Prueba 10 — Cancelar una tarea
- Pulsa **Cancelar** al lado de una tarea, o escribe `Cancela la tarea 3`.
- ✅ Desaparece de la lista.

### Prueba 11 — Recuperación tras reinicio
1. Programa una tarea para dentro de 5 minutos.
2. Pulsa el botón **RST** de la placa.
3. Recarga la página.
4. ✅ En *Avisos* sale *"Se recuperaron 1 tarea(s)..."* y la tarea sigue en la lista.
5. ✅ A su hora, la tarea se ejecuta normalmente.

Para el informe de pruebas, anota los resultados en [PRUEBAS.md](PRUEBAS.md).

---

## 11. Hacer peticiones a la API sin la página

La página web solo envía peticiones HTTP a la ESP32. Puedes hacer lo mismo desde una
terminal; sirve para probar y para mostrar cómo funciona el *Request/Response* en JSON.
El detalle completo está en [API.md](API.md).

En los ejemplos, cambia `192.168.1.60` por la IP de tu ESP32.

### Desde Windows (PowerShell)

```powershell
# Ver el estado de todo
Invoke-RestMethod http://192.168.1.60/api/estado

# Enviar una orden en lenguaje natural (consulta al LLM)
Invoke-RestMethod -Method Post -Uri http://192.168.1.60/api/orden -Body '{"texto": "Enciende la luz de la bodega"}'

# Enviar un plan JSON directo (SIN LLM): útil para probar la validación
Invoke-RestMethod -Method Post -Uri http://192.168.1.60/api/plan -Body (Get-Content -Raw ejemplos\1_control_inmediato.json)

# Probar un JSON inválido: debe responder con error 422 y no ejecutar nada
Invoke-RestMethod -Method Post -Uri http://192.168.1.60/api/plan -Body (Get-Content -Raw ejemplos\invalido_accion_desconocida.json)

# Cancelar la tarea 3
Invoke-RestMethod -Method Post -Uri http://192.168.1.60/api/cancelar -Body '{"id": 3}'
```

### Desde macOS / Linux / CMD (curl)

```bash
curl http://192.168.1.60/api/estado
curl -X POST http://192.168.1.60/api/orden -d "{\"texto\": \"Enciende la luz de la bodega\"}"
curl -X POST http://192.168.1.60/api/plan -d @ejemplos/1_control_inmediato.json
curl -X POST http://192.168.1.60/api/plan -d @ejemplos/invalido_accion_desconocida.json
```

**Ejemplo de respuesta** a la orden *"Enciende la luz de la bodega"*:

```json
{
  "orden": "Enciende la luz de la bodega",
  "json_del_llm": "{\"tipo\":\"plan\",\"mensaje\":\"Encender la luz de la bodega\",\"acciones\":[{\"accion\":\"encender\",\"dispositivo\":\"luz_bodega\"}]}",
  "errores": [],
  "resultados": ["Luz de la bodega: encendido"],
  "ok": true,
  "tipo": "plan",
  "mensaje": "Encender la luz de la bodega"
}
```

---

## 12. Problemas comunes

| Problema | Solución |
|----------|----------|
| No aparece ningún puerto COM | Usa un cable USB **de datos**; instala el driver CH343 o CP210x; prueba el otro conector USB de la placa |
| `Failed to connect to ESP32` al subir | Mantén **BOOT** presionado al pulsar *Subir* y suéltalo al ver `Connecting...` |
| `ArduinoJson.h: No such file or directory` | Instala la librería ArduinoJson (paso 1) |
| `credenciales.h` no se detecta | Debe estar en la **misma carpeta** que el `.ino`, con extensión `.h` (no `.h.txt`) |
| El Monitor Serie no muestra nada | Selecciona 115200 baudios y pulsa **RST**. Si usas el USB nativo, pon *USB CDC On Boot: Enabled* |
| Se queda en `Conectando a Wi-Fi.....` y pasa a modo respaldo | Revisa nombre/clave (distingue mayúsculas); la red debe ser de **2,4 GHz** |
| La página no abre | El celular/PC debe estar en la **misma red**. En redes de universidad los equipos a veces no se ven entre sí: usa el punto de acceso de tu celular para ambos |
| `HTTP 401` en la respuesta | La clave de la API está mal copiada |
| `HTTP 429` (Gemini) | Superaste el límite gratuito por minuto o por día: espera un momento |
| *"El reloj aún no está sincronizado"* | Espera unos segundos tras encender; se necesita internet para obtener la hora |
| Un LED no enciende | Está al revés (la pata larga va hacia la resistencia) o falta su conexión a GND |
| Una puerta aparece siempre ABIERTA | Su interruptor no llega a GND o está en la posición abierta |
| La ESP32 se reinicia sola | Usa otro puerto USB del computador o un cable más corto (falta de corriente) |

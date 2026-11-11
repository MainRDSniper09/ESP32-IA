# Tutorial: de la caja al asistente de voz funcionando

Paso a paso desde una ESP32-S3 recién sacada de la caja hasta apagar un LED diciendo
«apaga la luz de la cocina». No necesitas experiencia previa ni claves de API.

**Tiempo estimado:** 45 a 90 minutos la primera vez.

---

## Índice

0. [Qué necesitas](#0-qué-necesitas)
1. [Instalar el Arduino IDE y el soporte para ESP32](#1-instalar-el-arduino-ide-y-el-soporte-para-esp32)
2. [Conectar la ESP32 al computador](#2-conectar-la-esp32-al-computador)
3. [Armar el circuito](#3-armar-el-circuito)
4. [Poner tu Wi-Fi](#4-poner-tu-wi-fi)
5. [Subir el programa](#5-subir-el-programa)
6. [Primera prueba: el Monitor Serie](#6-primera-prueba-el-monitor-serie)
7. [Abrir la página web](#7-abrir-la-página-web)
8. [Activar el micrófono](#8-activar-el-micrófono)
9. [Probar el asistente](#9-probar-el-asistente)
10. [Problemas comunes](#10-problemas-comunes)

---

## 0. Qué necesitas

- **Hardware:** ESP32-S3, 6 LED amarillos, 1 verde, 1 rojo, 8 resistencias de 220 Ω,
  protoboard, cables y un cable USB **de datos** (algunos cables solo cargan).
- **Software:** un computador con el Arduino IDE 2 y **Google Chrome** (en el PC o en un celular Android).
- **Red:** un Wi-Fi de **2,4 GHz** (la ESP32 no se conecta a redes de 5 GHz).

---

## 1. Instalar el Arduino IDE y el soporte para ESP32

1. Descarga e instala el **Arduino IDE 2** desde <https://www.arduino.cc/en/software>.
2. **Archivo → Preferencias** → en *URLs adicionales de gestor de placas* pega:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **Herramientas → Placa → Gestor de placas** → busca **esp32** (de *Espressif Systems*) →
   elige la versión **2.0.17** → *Instalar*.
4. **Herramientas → Gestionar bibliotecas** → busca **ArduinoJson** (de *Benoit Blanchon*) →
   instala la versión 7.

---

## 2. Conectar la ESP32 al computador

La ESP32-S3 DevKitC-1 tiene **dos conectores USB**. Usa el que dice **UART** (o **COM**).

1. Conéctala: se enciende un LED de la placa.
2. **Windows:** en el *Administrador de dispositivos* → *Puertos (COM y LPT)* debe aparecer
   algo como *USB-SERIAL CH343 (COM5)*. Anota el número.
3. Si no aparece: cambia el cable o instala el driver
   [CH343](https://www.wch-ic.com/downloads/CH343SER_EXE.html) /
   [CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).

---

## 3. Armar el circuito

Sigue [CONEXIONES.md](CONEXIONES.md) y el [diagrama](diagrama_conexiones.svg). Resumen:

| GPIO | LED | | GPIO | LED |
|:--:|-----|-|:--:|-----|
| 4 | amarillo — Recepción | | 15 | amarillo — Baño |
| 5 | amarillo — Oficina | | 16 | amarillo — Bodega |
| 6 | amarillo — Sala de juntas | | 17 | **verde** — entendido |
| 7 | amarillo — Cocina | | 18 | **rojo** — no entendí |

Cada uno: `GPIO → resistencia 220 Ω → pata larga del LED · pata corta → GND`.

> Puedes probar el programa sin LED: la página web y el Monitor Serie muestran todo igual.

---

## 4. Poner tu Wi-Fi

1. Descarga el proyecto: <https://github.com/MainRDSniper09/ESP32-IA> → **Code → Download ZIP** → descomprime.
2. En el Arduino IDE: **Archivo → Abrir** → `AgenteESP32/AgenteESP32.ino`.
3. Crea el archivo del Wi-Fi **desde el IDE**, para que no quede como `.txt`:
   - Botón **⋯** (o ▾) a la derecha de las pestañas → **Nueva pestaña** → nombre `credenciales.h`.
   - Escribe tus datos:
     ```cpp
     #define WIFI_NOMBRE  "NombreDeTuRed"
     #define WIFI_CLAVE   "ClaveDeTuRed"
     ```
   - **Ctrl + S**.

`credenciales.h` no se sube a GitHub. Si no lo creas, la ESP32 crea su propia red
**Asistente-ESP32** (ver paso 7).

---

## 5. Subir el programa

1. **Herramientas → Placa → esp32 → ESP32S3 Dev Module**.
2. **Herramientas → Puerto** → el COM del paso 2.
3. **Herramientas → USB CDC On Boot** → *Disabled* (si usas el USB "UART").
4. Pulsa **→ Subir**. Termina con `Hard resetting via RTS pin...`.

**Si sale `Failed to connect to ESP32`:** mantén presionado **BOOT**, pulsa *Subir* y suelta
BOOT cuando veas `Connecting...`. Al final pulsa **RST**.

---

## 6. Primera prueba: el Monitor Serie

Esta prueba no necesita Wi-Fi ni navegador.

1. **Herramientas → Monitor Serie** → **115200 baudio** (abajo a la derecha).
2. Pulsa **RST** en la placa. Verás:
   ```
   === Asistente de voz ESP32-S3 ===
   Conectando a Wi-Fi.......
   Conectado. Abre en el navegador: http://192.168.1.60
   Listo. Escribe una orden aquí (ej: apaga la luz de la cocina) y pulsa Enter.
   ```
   **Anota la dirección.**
3. En la barra de texto del Monitor Serie escribe `prende todas las luces` y pulsa **Enter**.
   - ✅ Se encienden los 6 LED amarillos.
   - ✅ El verde parpadea 3 veces.
   - ✅ Aparece la tabla de porcentajes:

   ```
   ==================================================
   Orden recibida : "prende todas las luces"
   Palabras clave : prende todas
   --------------------------------------------------
   ENCENDER      ########## 100%  prende ~ prende
   APAGAR        ##........  20%  todas ~ desactivar
   TODAS         ########## 100%  todas ~ todas
   Recepción     ##........  29%  todas ~ entrada
   ...           (una línea por cada habitación)
   --------------------------------------------------
   Confianza: 100%  (umbral para aceptar: 70%)
   OK  Listo: encendí la luz de: TODAS las habitaciones
   ==================================================
   ```
4. Escribe `apaga la cosina` (con el error a propósito):
   - ✅ Se apaga solo el LED de la cocina (GPIO 7).
   - ✅ La tabla muestra `Cocina ########.. 83%  cosina ~ cocina`.

---

## 7. Abrir la página web

1. Conecta tu celular o PC a la **misma red Wi-Fi** que la ESP32.
2. Abre **Google Chrome** y escribe la dirección del paso 6 (por ejemplo `http://192.168.1.60`).

> **Si la ESP32 no se conectó a tu Wi-Fi**, el Monitor Serie dice
> `Sin Wi-Fi. Conéctate a la red 'Asistente-ESP32'...`. Conecta el celular a esa red
> (clave `asistente123`) y abre `http://192.168.4.1`. Todo funciona igual, salvo el
> micrófono, porque el reconocimiento de voz de Chrome necesita internet. Escribe las órdenes.

La página tiene:
- **🎤 Micrófono** y un cuadro para escribir, con frases de ejemplo.
- **🏢 Plano de la empresa:** cada habitación se ilumina si su LED está encendido.
  Tocar una habitación también la enciende o apaga.
- **🧠 Lo que entendió el asistente:** la tabla de porcentajes de la última orden.

---

## 8. Activar el micrófono

Por seguridad, Chrome solo deja usar el micrófono en páginas seguras (`https://`). La página
de la ESP32 es `http://`, así que hay que **autorizarla una vez**:

1. En Chrome (PC o Android) abre la dirección:
   ```
   chrome://flags/#unsafely-treat-insecure-origin-as-secure
   ```
2. En el cuadro de texto escribe la dirección de tu ESP32, por ejemplo `http://192.168.1.60`.
3. Cambia la opción a **Enabled** y pulsa **Relaunch** (reiniciar Chrome).
4. Vuelve a la página, pulsa 🎤 y acepta el permiso del micrófono.

**Alternativa sin configurar nada:** toca el cuadro de texto y usa el **micrófono del
teclado** para dictar (en Android, el 🎤 del teclado de Google; en Windows, **Win + H**).
Luego pulsa *Enviar*.

> Si la IP de la ESP32 cambia, repite el paso 2 con la nueva dirección.

---

## 9. Probar el asistente

Pulsa 🎤 y di cada frase (o escríbela). Observa los LED, el plano y la tabla:

| # | Di | Debe pasar |
|---|----|-----------|
| 1 | «Prende todas las luces» | 6 LED amarillos encendidos · verde parpadea |
| 2 | «Apaga la luz de la cocina» | Se apaga solo Cocina (GPIO 7) · Cocina 100 % |
| 3 | «Apaga la oficina y la recepción» | Se apagan dos LED a la vez |
| 4 | «Enciende la luz del baño» | Se enciende Baño (GPIO 15) |
| 5 | «Desactiva el almacén» | Se apaga Bodega: «almacén» es sinónimo |
| 6 | «Enciende la luz» | Nada cambia · **rojo** parpadea · «no en qué lugar» |
| 7 | «Hola, ¿cómo estás?» | Nada cambia · **rojo** parpadea · «No entendí» |
| 8 | «Apaga las luces» | Se apagan todas |

En la tabla, las barras **verdes** son las que superaron el **70 %** y se usaron para decidir.
La lista completa de pruebas, con los porcentajes esperados, está en [PRUEBAS.md](PRUEBAS.md).

---

## 10. Problemas comunes

| Problema | Solución |
|----------|----------|
| No aparece el puerto COM | Cable **de datos**; driver CH343/CP210x; prueba el otro USB de la placa |
| `Failed to connect to ESP32` | Mantén **BOOT** al pulsar *Subir* |
| `ArduinoJson.h: No such file or directory` | Instala la librería ArduinoJson (paso 1) |
| El Monitor Serie no muestra nada | 115200 baudios y pulsa **RST**. Con el USB nativo: *USB CDC On Boot → Enabled* |
| El Monitor Serie no reacciona a lo que escribo | Abajo, junto a los baudios, elige **"Nueva línea"** (New Line) |
| Se queda en `Conectando a Wi-Fi.....` | Revisa nombre/clave en `credenciales.h`; la red debe ser de **2,4 GHz** |
| La página no abre | El celular/PC debe estar en la **misma red**. En redes de universidad los equipos a veces no se ven: usa el punto de acceso del celular |
| 🎤: «El navegador bloqueó el micrófono» | Haz el paso 8, o usa el dictado del teclado |
| 🎤: «Este navegador no reconoce voz» | Usa **Google Chrome** (Firefox no tiene reconocimiento de voz) |
| Entiende mal una palabra | Mira en la tabla qué escuchó. Agrega esa palabra al vocabulario ([COMO_FUNCIONA.md §7](COMO_FUNCIONA.md#7-entrenar-al-asistente-con-palabras-nuevas)) |
| Un LED no enciende | Está al revés (pata larga hacia la resistencia) o le falta GND |

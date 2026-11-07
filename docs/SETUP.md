# Instalación, configuración y ejecución

## 1. Requisitos

| Herramienta | Versión | Para qué |
|-------------|---------|----------|
| [VS Code](https://code.visualstudio.com/) + extensión **PlatformIO IDE** | reciente | Compilar y grabar (recomendado) |
| *o* [Arduino IDE 2](https://www.arduino.cc/en/software) | 2.3+ | Alternativa |
| Core **esp32** de Espressif | 2.0.17 (verificado); 3.x previsto | Soporte del ESP32-S3 |
| Librería **ArduinoJson** (Benoit Blanchon) | 7.x | JSON |
| Python 3.9+ | opcional | Herramientas (`tools/`) |
| Clave de API de un LLM | — | Interpretación en lenguaje natural |

Sin clave de API el sistema igual funciona: control manual, monitoreo, tareas y planes JSON
directos (`POST /api/plan`). Solo la entrada en lenguaje natural requiere el LLM.

## 2. Configurar credenciales

```bash
cd firmware/AgenteESP32/src/config
cp agente_secrets.example.h agente_secrets.h      # Windows: copy agente_secrets.example.h agente_secrets.h
```

Edite `agente_secrets.h`:

```cpp
#define SECRET_WIFI_SSID      "MiRed"
#define SECRET_WIFI_PASSWORD  "MiClave"
#define SECRET_AP_PASSWORD    "agente1234"           // red de respaldo (mín. 8 caracteres)

#define SECRET_LLM_PROVIDER   LLM_PROVIDER_ANTHROPIC
#define SECRET_LLM_API_KEY    "sk-ant-..."           // https://console.anthropic.com
#define SECRET_LLM_MODEL      "claude-opus-5-5"
#define SECRET_LLM_ENDPOINT   ""                     // vacío = endpoint por defecto
#define SECRET_LLM_ROOT_CA    ""                     // ver SECURITY_NOTES.md

#define SECRET_DEVICE_API_TOKEN ""                   // opcional: protege la API REST
#define SECRET_TELEGRAM_BOT_TOKEN ""                 // opcional
#define SECRET_TELEGRAM_CHAT_ID   ""                 // opcional
```

`agente_secrets.h` está en `.gitignore`: **nunca** se sube al repositorio.

### Otros proveedores de LLM

Cualquier servicio compatible con `/v1/chat/completions`:

| Proveedor | `SECRET_LLM_ENDPOINT` | `SECRET_LLM_MODEL` (ejemplo) |
|-----------|-----------------------|------------------------------|
| OpenAI | *(vacío)* | el modelo de su cuenta |
| Groq | `https://api.groq.com/openai/v1/chat/completions` | un modelo de su catálogo |
| OpenRouter | `https://openrouter.ai/api/v1/chat/completions` | `proveedor/modelo` |
| Google Gemini | `https://generativelanguage.googleapis.com/v1beta/openai/chat/completions` | un modelo Gemini |
| Ollama (local, sin clave) | `http://192.168.1.50:11434/v1/chat/completions` | `llama3.1`, `qwen2.5`... |

con `#define SECRET_LLM_PROVIDER LLM_PROVIDER_OPENAI_COMPAT`.

### Zona horaria

En `config/AppConfig.h` (`AGENT_TIMEZONE`), formato POSIX. Por defecto Colombia `"<-05>5"`.
Ejemplos: México centro `"CST6"`, Perú `"<-05>5"`, Chile `"<-04>4<-03>,M9.1.6/24,M4.1.6/24"`,
España `"CET-1CEST,M3.5.0,M10.5.0/3"`.

## 3. Compilar y grabar

### Opción A — PlatformIO (recomendada)

```bash
cd firmware
pio run -e esp32s3                 # compilar
pio run -e esp32s3 -t upload       # grabar (conecte el puerto USB "UART"/"COM")
pio device monitor                 # consola serie a 115200 baudios
```

En VS Code: abra la carpeta `firmware/` y use los botones ✓ (Build) y → (Upload) de la barra inferior.
PlatformIO descarga automáticamente la plataforma y ArduinoJson.

La compilación verificada usa la plataforma `espressif32` 6.x (core Arduino-ESP32 **2.0.17**). El código
está escrito para ser compatible con el core 3.x (Arduino IDE), pero esa combinación no se ha verificado.

### Opción B — Arduino IDE 2

1. *Archivo → Preferencias → URLs adicionales*:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. *Herramientas → Placa → Gestor de placas*: instale **esp32** de Espressif.
3. *Herramientas → Gestionar bibliotecas*: instale **ArduinoJson** (Benoit Blanchon) 7.x.
4. Abra `firmware/AgenteESP32/AgenteESP32.ino` (el IDE compila también la carpeta `src/`). Versión verificada del core: **2.0.17** (selecciónela en el gestor de placas si la 3.x da problemas).
5. *Herramientas*:
   - Placa: **ESP32S3 Dev Module**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
   - USB CDC On Boot: *Disabled* si usa el puerto UART; *Enabled* si usa el puerto USB nativo
6. Subir (→) y abrir el Monitor Serie a **115200**.

## 4. Primer arranque

En la consola serie verá algo así:

```
==============================================
  Agente ESP32-S3 v1.0.0
==============================================
[evento #1] info    system   Sin tareas guardadas en memoria no volátil
[wifi] Conectando a "MiRed"...
[wifi] Conectado. IP: 192.168.1.60  RSSI: -58 dBm
[evento #2] info    network  Wi-Fi conectado
[mdns] http://agente-esp32.local
[http] Servidor web en el puerto 80
[evento #3] info    system   Sistema iniciado (firmware 1.0.0)
[evento #4] info    time     Hora sincronizada por NTP: 2026-11-06T17:58:12-05:00
```

Abra **http://agente-esp32.local** (o la IP) en un navegador de la misma red.

> Si `.local` no resuelve (algunas redes Windows/Android), use la IP impresa en la consola.

Si no hay Wi-Fi, el equipo crea la red **Agente-ESP32** (clave `SECRET_AP_PASSWORD`);
conéctese a ella y abra **http://192.168.4.1**.

## 5. Simulación en Wokwi (sin hardware)

1. Instale la extensión **Wokwi Simulator** en VS Code (requiere licencia gratuita).
2. En `agente_secrets.h` use `SECRET_WIFI_SSID "Wokwi-GUEST"` y `SECRET_WIFI_PASSWORD ""`.
3. `cd firmware && pio run -e esp32s3`
4. Abra `firmware/diagram.json` y ejecute **Wokwi: Start Simulator**.
5. La interfaz web del ESP32 simulado queda en **http://localhost:8180** (reenvío definido en `wokwi.toml`).

Los interruptores deslizantes del diagrama simulan puertas/ventanas; los LED, luces y aspersores.

## 6. Pruebas unitarias (en el PC)

El núcleo se prueba sin ESP32 (necesita `gcc`/`g++` en el PATH):

```bash
cd firmware
pio test -e native
```

Las mismas pruebas corren en GitHub Actions en cada *push* (ver `.github/workflows/ci.yml`).
Detalles en [TESTING.md](TESTING.md).

## 7. Herramientas

```bash
python tools/build_web.py                 # regenera WebPage.h tras editar web/index.html
python tools/validate_examples.py         # valida examples/plans con el JSON Schema (pip install jsonschema)
python tools/agent_cli.py --host 192.168.1.60 status
python tools/agent_cli.py say "Apaga todas las luces"
```

## 8. Solución de problemas

| Síntoma | Causa probable / solución |
|---------|---------------------------|
| No aparece el puerto COM | Use un cable USB **con datos**; instale el driver CH343/CP210x; pruebe el otro puerto USB de la placa. |
| Monitor serie vacío | Con el puerto USB nativo active *USB CDC On Boot*; verifique 115200 baudios. |
| `[!] No existe src/config/agente_secrets.h` | Copie la plantilla (paso 2). |
| La hora dice «sin sincronizar» | El NTP necesita internet. Las órdenes programadas se rechazan hasta sincronizar. |
| `llm_http` 401 | Clave de API incorrecta. |
| `llm_http` 400 | Modelo mal escrito, o modelo antiguo que no admite `effort`: defina `AGENT_LLM_EFFORT ""`. |
| `llm_network` | Sin salida a internet, firewall, o TLS; revise `SECRET_LLM_ROOT_CA`. |
| Una puerta siempre aparece abierta | Interruptor mal cableado o suelto (falla segura). Verifique la conexión a GND. |
| El ESP32 se reinicia en bucle | Revise que ningún interruptor esté en GPIO 0/3/45/46. |

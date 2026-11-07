# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/); versionado [SemVer](https://semver.org/lang/es/).

## [1.0.0] — 2026-11-06

### Añadido
- Firmware para ESP32-S3 (Arduino/C++): interpretación de órdenes en lenguaje natural mediante un LLM remoto (Claude o API compatible con OpenAI).
- Formato JSON de planes (`version: 1`) con tipos `plan`, `clarification`, `unsupported`.
- Validador estricto (esquema, tipos, dispositivos, horarios) y catálogo cerrado de 7 acciones.
- Planificador no bloqueante: inmediato, `at` (con repetición diaria), `delay`, `after_step`.
- Condiciones evaluadas localmente con los sensores (`all_closed`, `any_open`, `is_open`, `is_closed`).
- Persistencia de tareas en NVS con revalidación al arrancar y detección de tareas perdidas.
- Barrera determinista de expresiones temporales ambiguas.
- Alertas: interfaz web, LED, zumbador y Telegram opcional.
- API REST e interfaz web embebida.
- Wi-Fi con punto de acceso de respaldo, mDNS, NTP con zona horaria.
- 23 pruebas unitarias del núcleo, CI en GitHub Actions, simulación en Wokwi.
- Documentación: arquitectura, API, formato JSON, hardware, instalación, fallos, pruebas, seguridad, demo.

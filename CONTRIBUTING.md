# Cómo contribuir

¡Gracias por su interés! Este es un proyecto académico abierto; se aceptan mejoras de
código, documentación, pruebas y traducciones.

## Flujo de trabajo

1. Abra un *issue* describiendo el problema o la propuesta (use las plantillas).
2. Haga un *fork* y cree una rama: `git checkout -b feat/nombre-corto` o `fix/...`.
3. Haga cambios pequeños y enfocados; un tema por *pull request*.
4. Verifique localmente:
   ```bash
   cd firmware
   pio run -e esp32s3          # debe compilar sin advertencias nuevas
   pio test -e native          # todas las pruebas deben pasar
   cd ..
   python tools/build_web.py --check
   python tools/validate_examples.py
   ```
5. Abra el *pull request* contra `main` y complete la plantilla.

## Convenciones

- **Commits:** [Conventional Commits](https://www.conventionalcommits.org/es/) — `feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `chore:`.
- **C++:** estilo de `.clang-format` (Google, 120 columnas). Núcleo en **C++11** sin `Arduino.h`.
- **Idioma:** comentarios, mensajes al usuario y documentación en español; identificadores en inglés.
- **Arquitectura:** la lógica nueva va en `src/core/` con pruebas; el código específico del ESP32 va en `src/platform/`.
- **Interfaz web:** edite `web/index.html` y ejecute `python tools/build_web.py`. No edite `WebPage.h` a mano.
- **Secretos:** nunca incluya claves. `agente_secrets.h` está ignorado por git.

## Agregar un dispositivo o una acción

Ver [docs/ARCHITECTURE.md § 7](docs/ARCHITECTURE.md#7-cómo-extender-el-sistema). Actualice también
`docs/JSON_FORMAT.md`, `docs/schema/plan.schema.json` y las pruebas.

#!/usr/bin/env python3
"""
Genera firmware/AgenteESP32/src/web/WebPage.h a partir de web/index.html.

La interfaz web se edita como un archivo HTML normal (con resaltado de sintaxis
y la posibilidad de abrirlo en el navegador) y este script la incrusta en el
firmware como una cadena en memoria flash (PROGMEM).

Uso (desde la raíz del repositorio):
    python tools/build_web.py           # regenera el encabezado
    python tools/build_web.py --check   # falla si el encabezado está desactualizado (CI)
"""
from __future__ import annotations

import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "web" / "index.html"
DST = ROOT / "firmware" / "AgenteESP32" / "src" / "web" / "WebPage.h"
DELIM = "AGENTHTML"

HEADER = """\
/**
 * @file WebPage.h
 * @brief Interfaz web incrustada en flash.
 *
 * ARCHIVO GENERADO AUTOMÁTICAMENTE: NO LO EDITE A MANO.
 * Fuente: web/index.html  ->  python tools/build_web.py
 */
#pragma once

#include <Arduino.h>

// {size} bytes
static const char kIndexHtml[] PROGMEM = R"{delim}(
"""

FOOTER = """){delim}";
"""


def render() -> str:
    html = SRC.read_text(encoding="utf-8")
    if f"){DELIM}" in html:
        sys.exit(f"El HTML contiene el delimitador reservado ){DELIM}")
    return (
        HEADER.format(size=len(html.encode("utf-8")), delim=DELIM)
        + html
        + FOOTER.format(delim=DELIM)
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="solo verificar que WebPage.h esté actualizado")
    args = parser.parse_args()

    content = render()
    if args.check:
        current = DST.read_text(encoding="utf-8") if DST.exists() else ""
        if current != content:
            print("WebPage.h está desactualizado: ejecute python tools/build_web.py", file=sys.stderr)
            return 1
        print("WebPage.h está actualizado")
        return 0

    DST.write_text(content, encoding="utf-8", newline="\n")
    print(f"Generado {DST.relative_to(ROOT)} ({len(content.encode('utf-8'))} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

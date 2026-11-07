#!/usr/bin/env python3
"""
Verifica los planes de ejemplo contra el esquema docs/schema/plan.schema.json.

- examples/plans/*.json          deben ser VÁLIDOS.
- examples/plans/invalid/*       deben ser RECHAZADOS (por el esquema o, en los
                                 casos marcados como semánticos, solo por el firmware).

Requiere:  pip install jsonschema
Uso:       python tools/validate_examples.py
"""
from __future__ import annotations

import json
import pathlib
import sys

try:
    from jsonschema import Draft202012Validator
except ImportError:  # pragma: no cover
    sys.exit("Instale la dependencia:  pip install jsonschema")

ROOT = pathlib.Path(__file__).resolve().parent.parent
SCHEMA = json.loads((ROOT / "docs" / "schema" / "plan.schema.json").read_text(encoding="utf-8"))
EXAMPLES = ROOT / "examples" / "plans"

# Reglas que el esquema JSON no puede expresar; solo el firmware las detecta.
SEMANTIC_ONLY = {"referencia_paso_futuro.json"}


def main() -> int:
    validator = Draft202012Validator(SCHEMA)
    failures = 0

    for path in sorted(EXAMPLES.glob("*.json")):
        errors = list(validator.iter_errors(json.loads(path.read_text(encoding="utf-8"))))
        status = "OK " if not errors else "ERR"
        print(f"[{status}] válido esperado   {path.name}")
        for e in errors:
            print(f"        - {'/'.join(map(str, e.path))}: {e.message}")
        failures += bool(errors)

    for path in sorted((EXAMPLES / "invalid").iterdir()):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            print(f"[OK ] rechazo esperado  {path.name} (JSON mal formado)")
            continue
        rejected = not validator.is_valid(data)
        if path.name in SEMANTIC_ONLY:
            print(f"[OK ] rechazo esperado  {path.name} (regla semántica: lo rechaza el firmware)")
            continue
        print(f"[{'OK ' if rejected else 'ERR'}] rechazo esperado  {path.name}")
        failures += not rejected

    print("\nTodo correcto" if not failures else f"\n{failures} ejemplo(s) con problemas")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

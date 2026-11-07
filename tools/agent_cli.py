#!/usr/bin/env python3
"""
Cliente de línea de comandos para la API REST del Agente ESP32-S3.
Solo usa la biblioteca estándar de Python (sin dependencias).

Ejemplos:
    python tools/agent_cli.py status
    python tools/agent_cli.py say "Enciende la luz de la oficina a las 6:00 p. m."
    python tools/agent_cli.py plan examples/plans/03_escenario2_condicional.json
    python tools/agent_cli.py validate examples/plans/invalid/hora_invalida.json
    python tools/agent_cli.py set luz_entrada on
    python tools/agent_cli.py tasks
    python tools/agent_cli.py cancel 3          # o: cancel all
    python tools/agent_cli.py events --since 0
    python tools/agent_cli.py ack

Opciones globales:
    --host   dirección del ESP32 (por defecto agente-esp32.local o $AGENT_HOST)
    --token  token Bearer si la API está protegida (o $AGENT_TOKEN)
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request


class Api:
    def __init__(self, host: str, token: str | None):
        self.base = host if host.startswith("http") else f"http://{host}"
        self.token = token

    def call(self, method: str, path: str, body=None):
        data = None
        headers = {}
        if body is not None:
            data = body if isinstance(body, bytes) else json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        req = urllib.request.Request(self.base + path, data=data, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=15) as r:
                return r.status, json.loads(r.read() or b"null")
        except urllib.error.HTTPError as e:
            raw = e.read()
            try:
                return e.code, json.loads(raw)
            except json.JSONDecodeError:
                return e.code, {"raw": raw.decode("utf-8", "replace")}


def show(status: int, data) -> None:
    print(f"HTTP {status}")
    print(json.dumps(data, indent=2, ensure_ascii=False))


def cmd_say(api: Api, args) -> int:
    body = {"text": args.text}
    if args.context_request:
        body["context"] = {"request": args.context_request, "question": args.context_question or ""}
    status, data = api.call("POST", "/api/command", body)
    if status != 202:
        show(status, data)
        return 1
    job = data["job_id"]
    print(f"Job #{job} encolado; esperando al LLM", end="", flush=True)
    for _ in range(150):
        time.sleep(0.8)
        print(".", end="", flush=True)
        status, data = api.call("GET", f"/api/jobs?id={job}")
        if status == 200 and data.get("status") == "done":
            print()
            result = data["result"]
            print(f"\n[{result.get('outcome', '?').upper()}] {result.get('message', '')}")
            for r in result.get("results", []):
                print(f"  - {r.get('description')}: {r.get('status')} {r.get('message', '')}")
            for e in result.get("errors", []):
                print(f"  ! {e.get('code')} {e.get('field')}: {e.get('message')}")
            if result.get("outcome") == "clarification":
                print("\nResponda con:")
                print(f'  agent_cli.py say "<respuesta>" --context-request "{args.text}" '
                      f'--context-question "{result.get("question", "")}"')
            if args.verbose:
                show(status, data)
            return 0
    print("\nTiempo de espera agotado")
    return 1


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default=os.environ.get("AGENT_HOST", "agente-esp32.local"))
    p.add_argument("--token", default=os.environ.get("AGENT_TOKEN"))
    sub = p.add_subparsers(dest="cmd", required=True)

    sub.add_parser("status")
    sub.add_parser("catalog")
    sub.add_parser("devices")
    sub.add_parser("tasks")
    sub.add_parser("ack")
    s = sub.add_parser("say", help="instrucción en lenguaje natural")
    s.add_argument("text")
    s.add_argument("--context-request")
    s.add_argument("--context-question")
    s.add_argument("-v", "--verbose", action="store_true")
    for name in ("plan", "validate"):
        s = sub.add_parser(name, help="enviar un archivo de plan JSON")
        s.add_argument("file")
    s = sub.add_parser("set", help="control manual")
    s.add_argument("device")
    s.add_argument("state", choices=["on", "off"])
    s = sub.add_parser("cancel")
    s.add_argument("id", help="id de la tarea o 'all'")
    s = sub.add_parser("events")
    s.add_argument("--since", type=int, default=0)
    s = sub.add_parser("accesses")
    s.add_argument("--filter", default="all", choices=["all", "open", "closed"])

    args = p.parse_args()
    api = Api(args.host, args.token)

    if args.cmd == "say":
        return cmd_say(api, args)
    if args.cmd in ("plan", "validate"):
        with open(args.file, "rb") as f:
            raw = f.read()
        path = "/api/plan" if args.cmd == "plan" else "/api/plan/validate"
        show(*api.call("POST", path, raw))
        return 0
    routes = {
        "status": ("GET", "/api/status", None),
        "catalog": ("GET", "/api/catalog", None),
        "devices": ("GET", "/api/devices", None),
        "tasks": ("GET", "/api/tasks", None),
        "ack": ("POST", "/api/events/ack", None),
    }
    if args.cmd in routes:
        show(*api.call(*routes[args.cmd]))
    elif args.cmd == "set":
        show(*api.call("POST", "/api/devices/set", {"device": args.device, "state": args.state}))
    elif args.cmd == "cancel":
        q = "all=1" if args.id == "all" else f"id={args.id}"
        show(*api.call("DELETE", f"/api/tasks?{q}"))
    elif args.cmd == "events":
        show(*api.call("GET", f"/api/events?since={args.since}&limit=40"))
    elif args.cmd == "accesses":
        show(*api.call("GET", f"/api/accesses?filter={args.filter}"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

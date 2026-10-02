"""Status line do Claude Code que guarda o percentual oficial do limite para o agente.

Configurar em ~/.claude/settings.json:

    "statusLine": {"type": "command", "command": "python3 /caminho/para/agent/claude_statusline.py"}

Para manter um status line que você já usa, passe o comando dele depois de "--":

    python3 .../claude_statusline.py -- ~/meu-statusline.sh

Grava só os campos de rate_limits, sessão e modelo (mais seen_at) em
~/Library/Caches/mac-monitor/claude/<session_id>.json. Nunca falha: qualquer
erro vira uma linha vazia. Só usa a biblioteca padrão, para iniciar rápido.
"""

import json
import os
import re
import subprocess
import sys
import tempfile
import time

CACHE_DIR = os.path.expanduser("~/Library/Caches/mac-monitor/claude")


def _pick(data):
    limits = data.get("rate_limits") or {}

    def window(name):
        w = limits.get(name)
        if not isinstance(w, dict):
            return None
        return {"used_percentage": w.get("used_percentage"), "resets_at": w.get("resets_at")}

    model = data.get("model") or {}
    return {
        "session_id": data.get("session_id"),
        "model": model.get("display_name"),
        "five_hour": window("five_hour"),
        "seven_day": window("seven_day"),
        "seen_at": time.time(),
    }


def _save(info):
    session = re.sub(r"[^A-Za-z0-9_-]", "", str(info.get("session_id") or ""))
    if not session:
        return
    os.makedirs(CACHE_DIR, exist_ok=True)
    # arquivo temporário + rename: o agente nunca lê um arquivo pela metade
    fd, tmp = tempfile.mkstemp(dir=CACHE_DIR, prefix=".tmp-")
    try:
        with os.fdopen(fd, "w") as f:
            json.dump(info, f)
        os.replace(tmp, os.path.join(CACHE_DIR, session + ".json"))
    except Exception:
        os.unlink(tmp)
        raise


def _own_line(info):
    parts = [info["model"]] if info.get("model") else []
    five = info.get("five_hour") or {}
    if isinstance(five.get("used_percentage"), (int, float)):
        parts.append(f"{round(five['used_percentage'])}% do bloco")
    return " · ".join(parts)


def _previous_line(command, raw):
    try:
        out = subprocess.run(command, shell=True, input=raw, capture_output=True, timeout=2)
        return out.stdout.decode("utf-8", "replace").rstrip("\n")
    except Exception:
        return ""


def main():
    raw = b""
    try:
        raw = sys.stdin.buffer.read()
        info = _pick(json.loads(raw))
        _save(info)
        line = _own_line(info)
    except Exception:
        line = ""

    if "--" in sys.argv:
        previous = " ".join(sys.argv[sys.argv.index("--") + 1:])
        if previous:
            line = _previous_line(previous, raw)
    print(line)


if __name__ == "__main__":
    main()

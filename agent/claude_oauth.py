"""Percentual do bloco de 5 h e da semana pelo endpoint de uso do Claude Code.

GET https://api.anthropic.com/api/oauth/usage é o que o /usage do Claude Code
consulta. NÃO é documentado: pode mudar ou sumir sem aviso, e por isso é só a
primeira fonte; sem ele o agente volta ao status line e ao bloco estimado.

O token OAuth é lido do Keychain (item "Claude Code-credentials", criado pelo
próprio Claude Code) a cada consulta e descartado logo depois. Ele nunca é
renovado aqui (isso invalidaria o token do Claude Code), nunca vai para log,
mensagem de erro ou para o ESP32. Token vencido: espera o Claude Code renovar.

Roda numa thread própria para a requisição HTTP não travar o laço de 1 s.
"""

import datetime
import json
import subprocess
import threading
import time
import urllib.error
import urllib.request

USAGE_URL = "https://api.anthropic.com/api/oauth/usage"
KEYCHAIN_SERVICE = "Claude Code-credentials"
TIMEOUT_S = 10


class OAuthUsageError(Exception):
    """Falha sem detalhes sensíveis: a mensagem pode ir para o log."""


def _read_token():
    try:
        out = subprocess.run(["security", "find-generic-password", "-s", KEYCHAIN_SERVICE, "-w"],
                             capture_output=True, text=True, timeout=30)
    except Exception:
        raise OAuthUsageError("Keychain indisponível")
    if out.returncode != 0:
        raise OAuthUsageError("credencial do Claude Code não encontrada no Keychain (ou acesso negado)")
    try:
        oauth = json.loads(out.stdout)["claudeAiOauth"]
        token, expires_ms = oauth["accessToken"], oauth.get("expiresAt")
    except Exception:
        raise OAuthUsageError("formato inesperado da credencial no Keychain")
    if expires_ms and expires_ms / 1000 <= time.time():
        raise OAuthUsageError("token vencido, aguardando o Claude Code renovar")
    return token


def _parse_time(value):
    """'2026-10-03T01:00:00.087930+00:00' → epoch."""
    try:
        return datetime.datetime.fromisoformat(value).timestamp()
    except (TypeError, ValueError):
        return None


def _parse_window(data, name):
    w = data.get(name)
    if not isinstance(w, dict):
        return None
    pct, resets_at = w.get("utilization"), _parse_time(w.get("resets_at"))
    if not isinstance(pct, (int, float)) or resets_at is None:
        return None
    # utilization vem de 0 a 100 (bate com limits[].percent e com o /usage)
    return {"used_percentage": pct, "resets_at": resets_at}


def fetch_usage():
    """Uma consulta. Devolve {"five_hour": ..., "seven_day": ...} no formato do status line."""
    request = urllib.request.Request(USAGE_URL, headers={
        "Authorization": "Bearer " + _read_token(),
        "anthropic-beta": "oauth-2025-04-20",
        "User-Agent": "mac-monitor",
    })
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_S) as response:
            data = json.loads(response.read())
    except urllib.error.HTTPError as e:
        raise OAuthUsageError(f"HTTP {e.code}")
    except Exception as e:
        raise OAuthUsageError(type(e).__name__)
    if not isinstance(data, dict):
        raise OAuthUsageError("resposta inesperada")
    return {"five_hour": _parse_window(data, "five_hour"),
            "seven_day": _parse_window(data, "seven_day")}


class OAuthUsage:
    """Consulta o endpoint a cada interval_s numa thread e guarda o último resultado."""

    def __init__(self, interval_s):
        self._interval = max(60, interval_s)  # o mesmo dado que o /usage; não precisa de mais
        self._lock = threading.Lock()
        self._result = None
        self._fetched_at = 0.0
        self._last_error = None
        threading.Thread(target=self._run, name="claude-oauth", daemon=True).start()

    def latest(self):
        """Último resultado, se for recente (até 2 intervalos); senão None."""
        with self._lock:
            if self._result and time.time() - self._fetched_at <= 2 * self._interval:
                return self._result
            return None

    def _run(self):
        while True:
            try:
                result = fetch_usage()
                with self._lock:
                    self._result, self._fetched_at = result, time.time()
                if self._last_error is not None:
                    print("Claude Code: uso via API restabelecido")
                self._last_error = None
            except OAuthUsageError as e:
                # avisa só quando o motivo muda, para não encher o log
                if str(e) != self._last_error:
                    print(f"Claude Code: uso via API indisponível ({e}), usando o status line")
                    self._last_error = str(e)
            time.sleep(self._interval)

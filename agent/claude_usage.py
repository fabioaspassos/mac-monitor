"""Uso do Claude Code para a tela Claude (docs/claude/claude-code-dados-agente.md).

Junta três fontes:
- transcrições JSONL em ~/.claude/projects: tokens por minuto, sessões, bloco estimado
- endpoint de uso do Claude Code (claude_oauth.py): percentual do bloco de 5 h e da
  semana da conta inteira, mesmo sem sessão aberta
- arquivos gravados por claude_statusline.py: os mesmos percentuais, quando o endpoint
  falha (só atualizam com uma sessão aberta)

e monta a linha {"t": "cc", ...} que vai para o ESP32 a cada send_interval_s
(ou logo depois de uma mudança em b5.pct).
"""

import calendar
import glob
import json
import logging
import os
import time

from claude_oauth import OAuthUsage

BLOCK_S = 5 * 3600
DAY_S = 24 * 3600
# Se o relógio de parede andar bem mais que o monotônico entre duas leituras,
# o Mac dormiu: tudo é recalculado a partir dos arquivos.
SLEEP_JUMP_S = 60

log = logging.getLogger(__name__)


def _parse_ts(ts):
    """'2026-09-22T02:08:22.911Z' (UTC) → epoch em segundos."""
    try:
        return calendar.timegm((int(ts[0:4]), int(ts[5:7]), int(ts[8:10]),
                                int(ts[11:13]), int(ts[14:16]), int(ts[17:19])))
    except (TypeError, ValueError):
        return None


def _projects_dirs(projects_dir):
    env = os.environ.get("CLAUDE_CONFIG_DIR")
    if env:
        return [os.path.join(os.path.expanduser(p.strip()), "projects") for p in env.split(",") if p.strip()]
    return [os.path.expanduser(projects_dir)]


def _local_midnight(now):
    lt = time.localtime(now)
    return time.mktime((lt.tm_year, lt.tm_mon, lt.tm_mday, 0, 0, 0, 0, 0, -1))


def _window(status, name, now):
    """(percentual, resets_at) de uma janela do status line ou da API, se ainda estiver valendo."""
    w = (status or {}).get(name)
    if not isinstance(w, dict):
        return None
    pct, resets_at = w.get("used_percentage"), w.get("resets_at")
    if not isinstance(pct, (int, float)) or not isinstance(resets_at, (int, float)):
        return None
    if resets_at <= now:
        return None
    return max(0, min(100, round(pct))), resets_at


class ClaudeUsage:
    def __init__(self, cfg):
        self._projects = _projects_dirs(cfg["projects_dir"])
        self._status_dir = os.path.expanduser(cfg["status_cache_dir"])
        self._send_interval = cfg["send_interval_s"]
        self._scan_interval = cfg["scan_interval_s"]
        self._active_s = cfg["active_window_min"] * 60
        self._rate_min = max(1, cfg["rate_window_min"])
        self._count_cache_read = cfg["count_cache_read"]
        self._oauth = OAuthUsage(cfg["oauth_interval_s"]) if cfg["oauth_usage"] else None

        self._last_scan = None   # time.monotonic() da última leitura
        self._last_wall = None   # time.time() da mesma leitura
        self._last_send = None
        self._last_pct = None
        self._last_error = None
        self._reset()

    def _reset(self):
        self._offsets = {}    # caminho → bytes já lidos
        self._seen = {}       # "message.id:requestId" → minuto, para não contar duplicatas
        self._minutes = {}    # minuto (epoch) → [tokens, leituras de cache]
        self._sessions = {}   # sessionId → timestamp da última resposta

    def poll(self):
        """Chamar a cada volta do laço. Devolve a mensagem "cc" quando é hora de enviar."""
        mono = time.monotonic()
        if self._last_scan is not None and mono - self._last_scan < self._scan_interval:
            return None
        now = time.time()
        if self._last_wall is not None and (now - self._last_wall) - (mono - self._last_scan) > SLEEP_JUMP_S:
            self._reset()
        self._last_scan, self._last_wall = mono, now

        try:
            self._scan(now)
            msg = self._build(now)
            self._last_error = None
        except Exception as e:
            # um formato inesperado não pode derrubar o agente; avisa uma vez só
            if str(e) != self._last_error:
                log.warning("Claude Code: erro ao ler o uso (%s)", e)
                self._last_error = str(e)
            return None

        pct = msg["b5"]["pct"] if msg.get("b5") else None
        if self._last_send is None or pct != self._last_pct or mono - self._last_send >= self._send_interval:
            self._last_send, self._last_pct = mono, pct
            return msg
        return None

    # ---- transcrições ----

    def _scan(self, now):
        cutoff = now - DAY_S
        for root in self._projects:
            for path in glob.glob(os.path.join(root, "**", "*.jsonl"), recursive=True):
                try:
                    st = os.stat(path)
                except OSError:
                    continue
                offset = self._offsets.get(path)
                if offset is None:
                    # na partida, só os arquivos mexidos nas últimas 24 h; dos outros
                    # lê apenas o que for acrescentado daqui em diante
                    offset = 0 if st.st_mtime >= cutoff else st.st_size
                if st.st_size < offset:
                    offset = 0  # arquivo reescrito; as duplicatas são descartadas pelo id
                if st.st_size > offset:
                    offset = self._read(path, offset, cutoff)
                self._offsets[path] = offset

        self._minutes = {m: v for m, v in self._minutes.items() if m >= cutoff}
        self._seen = {k: m for k, m in self._seen.items() if m >= cutoff}
        self._sessions = {s: t for s, t in self._sessions.items() if t >= cutoff}

    def _read(self, path, offset, cutoff):
        """Lê as linhas completas a partir de offset; devolve o novo offset."""
        try:
            with open(path, "rb") as f:
                f.seek(offset)
                data = f.read()
        except OSError:
            return offset
        end = data.rfind(b"\n")
        if end < 0:
            return offset  # linha ainda sendo escrita
        for raw in data[:end].split(b"\n"):
            if b'"usage"' not in raw:
                continue  # filtro barato antes do json.loads
            try:
                self._add(json.loads(raw), cutoff)
            except Exception:
                continue  # linha corrompida ou num formato desconhecido
        return offset + end + 1

    def _add(self, entry, cutoff):
        if entry.get("type") != "assistant":
            return
        message = entry.get("message") or {}
        usage = message.get("usage")
        if not isinstance(usage, dict):
            return
        ts = _parse_ts(entry.get("timestamp"))
        if ts is None or ts < cutoff:
            return

        minute = ts - ts % 60
        msg_id, req_id = message.get("id"), entry.get("requestId")
        if msg_id and req_id:
            key = f"{msg_id}:{req_id}"
            if key in self._seen:
                return
            self._seen[key] = minute

        tokens = (int(usage.get("input_tokens") or 0) + int(usage.get("output_tokens") or 0)
                  + int(usage.get("cache_creation_input_tokens") or 0))
        cache = int(usage.get("cache_read_input_tokens") or 0)
        if self._count_cache_read:
            tokens += cache

        bucket = self._minutes.setdefault(minute, [0, 0])
        bucket[0] += tokens
        bucket[1] += cache

        session = entry.get("sessionId")
        if session and ts > self._sessions.get(session, 0):
            self._sessions[session] = ts

    def _estimated_block(self, now):
        """Início do bloco estimado (lógica do ccusage) ou None se não houver bloco ativo.

        Um bloco começa na hora cheia da primeira resposta depois de mais de 5 h
        sem atividade (ou depois do fim do bloco anterior) e dura 5 h.
        """
        start = last = None
        for minute in sorted(self._minutes):
            if start is None or minute - last > BLOCK_S or minute >= start + BLOCK_S:
                start = minute - minute % 3600
            last = minute
        if start is None or now >= start + BLOCK_S or now - last > BLOCK_S:
            return None
        return start

    # ---- status line ----

    def _read_status(self, now):
        """Arquivo de status mais recente e sessões vistas na janela de atividade."""
        latest, sessions = None, set()
        for path in glob.glob(os.path.join(self._status_dir, "*.json")):
            try:
                with open(path) as f:
                    data = json.load(f)
                seen = float(data.get("seen_at") or 0)
            except Exception:
                continue
            if now - seen > DAY_S:
                try:
                    os.remove(path)  # sessão antiga
                except OSError:
                    pass
                continue
            if now - seen <= self._active_s and data.get("session_id"):
                sessions.add(data["session_id"])
            if latest is None or seen > latest["seen_at"]:
                data["seen_at"] = seen
                latest = data
        return latest, sessions

    # ---- mensagem ----

    def _build(self, now):
        status, active = self._read_status(now)
        api = self._oauth.latest() if self._oauth else None
        if not self._offsets and status is None and api is None:
            return {"v": 1, "t": "cc", "ok": 0, "src": "none", "b5": None}

        # o endpoint vale mais que o status line: é atual mesmo sem sessão aberta
        five, src = _window(api, "five_hour", now), "api"
        if not five:
            five, src = _window(status, "five_hour", now), "sl"
        if five:
            pct, resets_at = five
            start = resets_at - BLOCK_S
            b5 = {"pct": pct, "rst": int(resets_at - now)}
        else:
            start = self._estimated_block(now)
            if start is not None:
                src, b5 = "est", {"pct": None, "rst": int(start + BLOCK_S - now)}
            else:
                src, b5 = "none", None

        w7 = None
        week = _window(api, "seven_day", now) or _window(status, "seven_day", now)
        if week:
            pct, resets_at = week
            lt = time.localtime(resets_at)
            # wd/wh: dia da semana (0=domingo) e hora local do reinício, para "qui 21h"
            w7 = {"pct": pct, "rst": int(resets_at - now), "wd": (lt.tm_wday + 1) % 7, "wh": lt.tm_hour}

        midnight = _local_midnight(now)
        rate_from = now - now % 60 - (self._rate_min - 1) * 60
        hours = [0] * 24
        tb = td = tc = recent = 0
        for minute, (tokens, cache) in self._minutes.items():
            if minute >= midnight:
                td += tokens
                tc += cache
                hours[time.localtime(minute).tm_hour] += tokens
            if start is not None and minute >= start:
                tb += tokens
            if minute >= rate_from:
                recent += tokens

        for session, ts in self._sessions.items():
            if now - ts <= self._active_s:
                active.add(session)

        return {
            "v": 1, "t": "cc", "ok": 1, "src": src, "b5": b5, "w7": w7,
            "tb": tb, "td": td, "tc": tc,
            "rate": round(recent / self._rate_min),
            "ses": len(active),
            "hr": time.localtime(now).tm_hour,
            "h": [round(v / 1000) for v in hours],
        }

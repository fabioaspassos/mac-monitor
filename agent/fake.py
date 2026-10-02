"""Manda linhas de teste para a placa (USB ou Bluetooth), uma por segundo, sem depender da carga do Mac.

Uso:
    python fake.py            # cenário normal com variação, para os gráficos terem forma
    python fake.py normal     # linha fixa do protótipo
    python fake.py alta       # linha fixa de carga alta
    python fake.py nucleos    # alterna 4, 10, 14 e 20 núcleos a cada 5 s
    python fake.py claude     # tela Claude: bloco oficial do status line (linha "cc" a cada 10 s)
    python fake.py estimado   # tela Claude: sem rate_limits, bloco estimado
    python fake.py ocioso     # tela Claude: sem bloco ativo
    python fake.py semclaude  # tela Claude: Claude Code não encontrado
    python fake.py alta --ble # qualquer cenário, via Bluetooth LE
"""

import json
import math
import random
import sys
import time

from transport import TransportError, open_transport

NORMAL = {"v": 1, "cpu": 37.5, "cores": [62, 40, 18, 9, 71, 58, 44, 36, 25, 17], "mem": 71.2,
          "swap": 0.8, "press": 1, "gpu": 18, "rx": 125000, "tx": 32000, "disk": 64.1,
          "rd": 0, "wr": 52000}

ALTA = {"v": 1, "cpu": 91, "cores": [96, 94, 91, 88, 97, 93, 89, 90, 86, 92], "mem": 89,
        "swap": 3.4, "press": 2, "gpu": 76, "rx": 2400000, "tx": 410000, "disk": 64,
        "rd": 18000000, "wr": 6200000}


# Linha "t":"cc" de exemplo (seção 5 de docs/claude/claude-code-dados-agente.md)
CLAUDE = {"v": 1, "t": "cc", "ok": 1, "src": "sl", "b5": {"pct": 62, "rst": 6480},
          "w7": {"pct": 41, "rst": 302400, "wd": 4, "wh": 21}, "tb": 412000, "td": 1840000,
          "tc": 9300000, "rate": 3200, "ses": 2, "hr": 15,
          "h": [0, 0, 0, 0, 0, 0, 0, 0, 12, 48, 96, 130, 84, 22, 110, 62, 0, 0, 0, 0, 0, 0, 0, 0]}

CLAUDE_SCENARIOS = {
    "claude": CLAUDE,
    "estimado": dict(CLAUDE, src="est", b5={"pct": None, "rst": 6480}, w7=None),
    "ocioso": dict(CLAUDE, src="none", b5=None, tb=0, rate=0, ses=0),
    "semclaude": {"v": 1, "t": "cc", "ok": 0, "src": "none", "b5": None},
}


def claude_line(name, t):
    """Linha "cc" a cada 10 s, com o reinício descontando o tempo que passou."""
    if t % 10:
        return None
    line = dict(CLAUDE_SCENARIOS[name])
    if line.get("b5"):
        line["b5"] = dict(line["b5"], rst=max(0, line["b5"]["rst"] - t))
    return line


def wave(t):
    """Variação suave em volta do cenário normal, com picos de vez em quando."""
    line = dict(NORMAL)
    cpu_peak = max(0.0, math.sin(t / 20)) ** 4 * 50
    line["cpu"] = round(30 + 6 * math.sin(t / 3) + cpu_peak + random.uniform(-3, 3), 1)
    line["gpu"] = round(max(0, 15 + 4 * math.sin(t / 5) + max(0.0, math.sin(t / 27 + 1)) ** 6 * 40))
    line["cores"] = [max(0, min(100, round(line["cpu"] + random.uniform(-25, 25)))) for _ in range(10)]
    line["mem"] = round(70 + 3 * math.sin(t / 40), 1)
    line["rx"] = round(40000 + 30000 * math.sin(t / 4) + max(0.0, math.sin(t / 30)) ** 8 * 900000)
    line["tx"] = round(15000 + 10000 * math.sin(t / 6) + max(0.0, math.sin(t / 17 + 2)) ** 10 * 150000)
    line["wr"] = round(max(0, 50000 + 40000 * math.sin(t / 7)))
    return line


def cores_cycle(t):
    line = dict(NORMAL)
    n = [4, 10, 14, 20][(t // 5) % 4]
    line["cores"] = [round(50 + 45 * math.sin(t / 2 + i)) for i in range(n)]
    return line


SCENARIOS = {
    "onda": wave,
    "normal": lambda t: NORMAL,
    "alta": lambda t: ALTA,
    "nucleos": cores_cycle,
}
SCENARIOS.update({name: lambda t: NORMAL for name in CLAUDE_SCENARIOS})


def main():
    args = [a for a in sys.argv[1:] if a != "--ble"]
    name = args[0] if args else "onda"
    scenario = SCENARIOS[name]
    conn = open_transport(ble="--ble" in sys.argv)
    t = 0
    while True:
        line = json.dumps(scenario(t)) + "\n"
        cc = claude_line(name, t) if name in CLAUDE_SCENARIOS else None
        if cc:
            line += json.dumps(cc, separators=(",", ":")) + "\n"
        try:
            conn.write(line.encode("utf-8"))
        except TransportError:
            print("Perdeu a conexão, reconectando...")
            conn.close()
            conn.open()
            continue
        t += 1
        time.sleep(1)


if __name__ == "__main__":
    main()

"""Manda linhas de teste pela serial, uma por segundo, sem depender da carga do Mac.

Uso:
    python fake.py            # cenário normal com variação, para os gráficos terem forma
    python fake.py normal     # linha fixa do protótipo
    python fake.py alta       # linha fixa de carga alta
    python fake.py nucleos    # alterna 4, 10, 14 e 20 núcleos a cada 5 s
"""

import json
import math
import random
import sys
import time

import serial

from agent import open_connection

NORMAL = {"v": 1, "cpu": 37.5, "cores": [62, 40, 18, 9, 71, 58, 44, 36, 25, 17], "mem": 71.2,
          "swap": 0.8, "press": 1, "gpu": 18, "rx": 125000, "tx": 32000, "disk": 64.1,
          "rd": 0, "wr": 52000}

ALTA = {"v": 1, "cpu": 91, "cores": [96, 94, 91, 88, 97, 93, 89, 90, 86, 92], "mem": 89,
        "swap": 3.4, "press": 2, "gpu": 76, "rx": 2400000, "tx": 410000, "disk": 64,
        "rd": 18000000, "wr": 6200000}


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


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "onda"
    scenario = SCENARIOS[name]
    conn = open_connection()
    t = 0
    while True:
        line = json.dumps(scenario(t)) + "\n"
        try:
            conn.write(line.encode("utf-8"))
        except serial.SerialException:
            print("Perdeu a conexão, reconectando...")
            conn.close()
            conn = open_connection()
            continue
        t += 1
        time.sleep(1)


if __name__ == "__main__":
    main()

import json
import time

import serial
from serial.tools import list_ports

from metrics import Collector

INTERVAL = 1.0
BAUD_RATE = 115200
CH340_VID_PID = (0x1A86, 0x7523)


def find_port():
    for port in list_ports.comports():
        if (port.vid, port.pid) == CH340_VID_PID:
            return port.device
    return None


def open_connection():
    while True:
        device = find_port()
        if device:
            try:
                conn = serial.Serial(device, BAUD_RATE, timeout=1)
                time.sleep(2)  # dá tempo do ESP32 reiniciar após abrir a porta
                print(f"Conectado em {device}")
                return conn
            except serial.SerialException:
                pass
        print("Placa não encontrada, tentando de novo em 2s...")
        time.sleep(2)


def main():
    collector = Collector()
    conn = open_connection()

    while True:
        start = time.monotonic()
        line = json.dumps(collector.collect()) + "\n"

        try:
            conn.write(line.encode("utf-8"))
        except serial.SerialException:
            print("Perdeu a conexão, reconectando...")
            conn.close()
            conn = open_connection()
            continue

        time.sleep(max(0, INTERVAL - (time.monotonic() - start)))


if __name__ == "__main__":
    main()
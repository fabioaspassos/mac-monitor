import argparse
import json
import time

from metrics import Collector
from transport import TransportError, open_transport

INTERVAL = 1.0


def parse_args():
    parser = argparse.ArgumentParser(description="Envia as métricas do Mac para o ESP32.")
    parser.add_argument("--ble", action="store_true", help="usa Bluetooth LE em vez da USB serial")
    return parser.parse_args()


def main():
    args = parse_args()
    collector = Collector()
    conn = open_transport(args.ble)

    while True:
        start = time.monotonic()
        line = json.dumps(collector.collect()) + "\n"

        try:
            conn.write(line.encode("utf-8"))
        except TransportError:
            print("Perdeu a conexão, reconectando...")
            conn.close()
            conn.open()
            continue

        time.sleep(max(0, INTERVAL - (time.monotonic() - start)))


if __name__ == "__main__":
    main()

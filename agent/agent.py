import argparse
import json
import logging
import time

from claude_usage import ClaudeUsage
from config import load_config
from metrics import Collector
from transport import TransportError, open_transport

INTERVAL = 1.0

log = logging.getLogger(__name__)


def parse_args():
    parser = argparse.ArgumentParser(description="Envia as métricas do Mac para o ESP32.")
    parser.add_argument("--ble", action="store_true", help="usa Bluetooth LE em vez da USB serial")
    return parser.parse_args()


def main():
    args = parse_args()
    # com hora: sob o launchd a saída vai para ~/Library/Logs/mac-monitor/agent.log
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s",
                        datefmt="%Y-%m-%d %H:%M:%S")
    config = load_config()
    collector = Collector()
    claude = ClaudeUsage(config["claude"]) if config["claude"]["enabled"] else None
    conn = open_transport(args.ble)

    while True:
        start = time.monotonic()
        line = json.dumps(collector.collect()) + "\n"
        # uso do Claude Code: linha separada ("t":"cc"), a cada 10 s
        cc = claude.poll() if claude else None
        if cc:
            line += json.dumps(cc, separators=(",", ":")) + "\n"

        try:
            conn.write(line.encode("utf-8"))
        except TransportError:
            log.warning("Perdeu a conexão, reconectando...")
            conn.close()
            conn.open()
            continue

        time.sleep(max(0, INTERVAL - (time.monotonic() - start)))


if __name__ == "__main__":
    main()

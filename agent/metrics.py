import re
import subprocess
import time

import psutil
from Foundation import NSProcessInfo

# NSProcessInfoThermalState: 0=nominal, 1=fair, 2=serious, 3=critical.
# É a única leitura térmica que a Apple expõe sem privilégios de root em
# Apple Silicon - não existe API pública para a temperatura exata em °C.
_PROCESS_INFO = NSProcessInfo.processInfo()


def read_cpu_thermal():
    try:
        return int(_PROCESS_INFO.thermalState())
    except Exception:
        return 0


def read_memory_pressure():
    try:
        out = subprocess.run(
            ["sysctl", "-n", "kern.memorystatus_vm_pressure_level"],
            capture_output=True, text=True, timeout=1,
        )
        return int(out.stdout.strip())
    except Exception:
        return 1  # assume normal se a leitura falhar


_GPU_RE = re.compile(rb'"Device Utilization %"\s*=\s*(\d+)')


def read_gpu():
    try:
        out = subprocess.run(
            ["ioreg", "-r", "-d", "1", "-c", "IOAccelerator"],
            capture_output=True, timeout=1,
        )
        match = _GPU_RE.search(out.stdout)
        return int(match.group(1)) if match else 0
    except Exception:
        return 0


class RateTracker:
    """Guarda a leitura anterior de um contador acumulado e devolve bytes/s."""

    def __init__(self):
        self._last = None
        self._last_time = None

    def rate(self, value):
        now = time.monotonic()
        if self._last is None:
            delta = 0
        else:
            elapsed = max(now - self._last_time, 0.001)
            delta = max(value - self._last, 0) / elapsed
        self._last, self._last_time = value, now
        return round(delta)


class Collector:
    def __init__(self):
        self._net_rx = RateTracker()
        self._net_tx = RateTracker()
        self._disk_rd = RateTracker()
        self._disk_wr = RateTracker()
        psutil.cpu_percent(percpu=True)  # descarta a primeira leitura, que sempre vem 0

    def collect(self):
        cores = psutil.cpu_percent(percpu=True)
        cpu_total = sum(cores) / len(cores) if cores else 0.0

        vm = psutil.virtual_memory()
        swap = psutil.swap_memory()
        net = psutil.net_io_counters()
        disk_io = psutil.disk_io_counters()
        disk_usage = psutil.disk_usage("/")

        return {
            "v": 1,
            "cpu": round(cpu_total, 1),
            "cores": [round(c) for c in cores],
            "mem": round(vm.percent, 1),
            "swap": round(swap.percent, 1),
            "press": read_memory_pressure(),
            "temp": read_cpu_thermal(),
            "gpu": read_gpu(),
            "rx": self._net_rx.rate(net.bytes_recv),
            "tx": self._net_tx.rate(net.bytes_sent),
            "disk": round(disk_usage.percent, 1),
            "rd": self._disk_rd.rate(disk_io.read_bytes),
            "wr": self._disk_wr.rate(disk_io.write_bytes),
        }
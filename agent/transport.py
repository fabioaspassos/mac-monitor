"""Meio físico até o ESP32: USB serial (padrão) ou Bluetooth LE.

Os dois transportes têm a mesma cara: `open()` bloqueia até conectar (tentando
de novo a cada 2 s), `write()` levanta `TransportError` se a conexão cair.
"""

import asyncio
import time

import serial
from serial.tools import list_ports

BAUD_RATE = 115200
CH340_VID_PID = (0x1A86, 0x7523)

# Nordic UART Service: o Mac escreve as linhas JSON na característica RX
BLE_NAME = "Mac Monitor"
NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
BLE_SCAN_TIMEOUT = 5.0

RETRY_DELAY = 2


class TransportError(Exception):
    pass


class SerialTransport:
    def __init__(self):
        self.conn = None

    @staticmethod
    def find_port():
        for port in list_ports.comports():
            if (port.vid, port.pid) == CH340_VID_PID:
                return port.device
        return None

    def open(self):
        while True:
            device = self.find_port()
            if device:
                try:
                    self.conn = serial.Serial(device, BAUD_RATE, timeout=1)
                    time.sleep(2)  # dá tempo do ESP32 reiniciar após abrir a porta
                    print(f"Conectado em {device}")
                    return
                except serial.SerialException:
                    pass
            print(f"Placa não encontrada, tentando de novo em {RETRY_DELAY}s...")
            time.sleep(RETRY_DELAY)

    def write(self, data):
        try:
            self.conn.write(data)
        except serial.SerialException as e:
            raise TransportError(e) from e

    def close(self):
        if self.conn:
            self.conn.close()
            self.conn = None


class BleTransport:
    # bleak é assíncrono; um event loop próprio mantém o resto do agent síncrono
    def __init__(self):
        self.loop = asyncio.new_event_loop()
        self.client = None
        self.rx_char = None
        self.connected = False

    def _on_disconnect(self, _client):
        self.connected = False

    async def _connect(self):
        # imports aqui para o modo USB não depender do bleak instalado
        from bleak import BleakClient, BleakScanner

        def match(device, adv):
            return NUS_SERVICE_UUID in adv.service_uuids or device.name == BLE_NAME

        device = await BleakScanner.find_device_by_filter(match, timeout=BLE_SCAN_TIMEOUT)
        if device is None:
            return False
        client = BleakClient(device, disconnected_callback=self._on_disconnect)
        await client.connect()
        self.client = client
        self.rx_char = client.services.get_characteristic(NUS_RX_UUID)
        if self.rx_char is None:
            await client.disconnect()
            return False
        self.connected = True
        print(f"Conectado em {device.name or device.address} (Bluetooth)")
        return True

    def open(self):
        from bleak.exc import BleakError

        while True:
            try:
                if self.loop.run_until_complete(self._connect()):
                    return
            except (BleakError, asyncio.TimeoutError, OSError):
                self.close()
            print(f"Placa não encontrada, tentando de novo em {RETRY_DELAY}s...")
            time.sleep(RETRY_DELAY)

    async def _write(self, data):
        # a linha (~300 bytes) não cabe num pacote; o firmware remonta pelo "\n"
        size = self.rx_char.max_write_without_response_size
        for i in range(0, len(data), size):
            await self.client.write_gatt_char(self.rx_char, data[i:i + size], response=False)

    def write(self, data):
        from bleak.exc import BleakError

        if not self.connected:
            raise TransportError("desconectado")
        try:
            self.loop.run_until_complete(self._write(data))
        except (BleakError, OSError) as e:
            raise TransportError(e) from e

    def close(self):
        if self.client:
            try:
                self.loop.run_until_complete(self.client.disconnect())
            except Exception:
                pass
            self.client = None
        self.rx_char = None
        self.connected = False


def open_transport(ble=False):
    transport = BleTransport() if ble else SerialTransport()
    transport.open()
    return transport

#include "ble_link.h"

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

#include "line_reader.h"

namespace {

// Nordic UART Service: o Mac escreve na característica RX (tem que bater com agent/transport.py)
constexpr const char* DEVICE_NAME = "Mac Monitor";
constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";

// Folga para algumas linhas caso o LVGL segure o loop, como o RX da serial
constexpr size_t STREAM_BYTES = 1024;
uint8_t streamStorage[STREAM_BYTES + 1];
StaticStreamBuffer_t streamStruct;
StreamBufferHandle_t stream;

LineReader reader;

// onWrite roda na task do NimBLE: só repassa os bytes para o loop() consumir
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue value = c->getValue();
    xStreamBufferSend(stream, value.data(), value.size(), 0);
  }
} rxCallbacks;

}  // namespace

void ble_link_begin() {
  stream = xStreamBufferCreateStatic(STREAM_BYTES, 1, streamStorage, &streamStruct);

  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setMTU(247);  // linhas de ~300 bytes em 2 pacotes em vez de 15

  NimBLEServer* server = NimBLEDevice::createServer();
  server->advertiseOnDisconnect(true);  // volta a anunciar quando o Mac cai

  NimBLEService* service = server->createService(NUS_SERVICE_UUID);
  NimBLECharacteristic* rx = service->createCharacteristic(
      NUS_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(&rxCallbacks);
  service->start();

  // UUID de 128 bits + nome não cabem juntos nos 31 bytes do anúncio;
  // com scan response ligado o nome vai nela
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->start();
}

bool ble_link_poll(Metrics& m) {
  char c;
  while (xStreamBufferReceive(stream, &c, 1, 0) == 1) {
    if (reader.feed(c, m)) return true;
  }
  return false;
}

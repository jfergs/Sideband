#include <Arduino.h>
#include <BluetoothSerial.h>
#include <NimBLEDevice.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "sideband_config.example.h"
#endif

namespace {

BluetoothSerial radioSerial;
NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *bleTx = nullptr;

constexpr char BLE_SERVICE_UUID[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char BLE_RX_UUID[] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char BLE_TX_UUID[] = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

uint32_t radioToBleCount = 0;
uint32_t bleToRadioCount = 0;
bool bleConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
    (void)server;
    (void)connInfo;
    bleConnected = true;
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override {
    (void)connInfo;
    (void)reason;
    bleConnected = false;
    server->startAdvertising();
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &connInfo) override {
    (void)connInfo;
    std::string value = characteristic->getValue();
    if (value.empty()) {
      return;
    }

    radioSerial.write(reinterpret_cast<const uint8_t *>(value.data()), value.size());
    bleToRadioCount++;
  }
};

void setupBle() {
  NimBLEDevice::init(SIDEBAND_BLE_NAME);
  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  NimBLEService *service = bleServer->createService(BLE_SERVICE_UUID);
  NimBLECharacteristic *bleRx = service->createCharacteristic(
      BLE_RX_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  bleTx = service->createCharacteristic(
      BLE_TX_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  bleRx->setCallbacks(new RxCallbacks());
  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(BLE_SERVICE_UUID);
  advertising->enableScanResponse(true);
  advertising->start();
}

void relayRadioToBle() {
  if (!bleConnected || bleTx == nullptr) {
    while (radioSerial.available()) {
      radioSerial.read();
    }
    return;
  }

  uint8_t buffer[128];
  size_t len = 0;
  while (radioSerial.available() && len < sizeof(buffer)) {
    buffer[len++] = static_cast<uint8_t>(radioSerial.read());
  }

  if (len > 0) {
    bleTx->setValue(buffer, len);
    bleTx->notify();
    radioToBleCount++;
  }
}

void printStatus() {
  static uint32_t lastStatusMs = 0;
  uint32_t now = millis();
  if (now - lastStatusMs < 5000) {
    return;
  }
  lastStatusMs = now;

  Serial.printf(
      "SIDEBAND status=running ble=%s radio_to_ble=%lu ble_to_radio=%lu\n",
      bleConnected ? "connected" : "advertising",
      static_cast<unsigned long>(radioToBleCount),
      static_cast<unsigned long>(bleToRadioCount));
}

}  // namespace

void setup() {
  Serial.begin(SIDEBAND_SERIAL_BAUD);
  delay(300);

  Serial.println("SIDEBAND boot");
  Serial.println("Primary bridge target: Bluetooth Classic capable ESP32");

  radioSerial.begin(SIDEBAND_DEVICE_NAME, true);
  setupBle();
}

void loop() {
  relayRadioToBle();
  printStatus();
  delay(5);
}

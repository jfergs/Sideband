#include <Arduino.h>
#include <BluetoothSerial.h>
#include <NimBLEDevice.h>
#ifdef SIDEBAND_IOS_DISCOVERY_HID
#include <NimBLEHIDDevice.h>
#endif
#ifdef SIDEBAND_HAS_TFT
#include <TFT_eSPI.h>
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "sideband_config.example.h"
#endif

namespace {

BluetoothSerial radioSerial;
NimBLEServer *bleServer = nullptr;
NimBLECharacteristic *bleTx = nullptr;
#ifdef SIDEBAND_HAS_TFT
TFT_eSPI tft;
#endif

constexpr char BLE_SERVICE_UUID[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char BLE_RX_UUID[] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char BLE_TX_UUID[] = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char BLE_DEVICE_INFO_SERVICE_UUID[] = "180a";
constexpr char BLE_MANUFACTURER_UUID[] = "2a29";
constexpr char BLE_MODEL_UUID[] = "2a24";
constexpr char BLE_FIRMWARE_UUID[] = "2a26";
constexpr char BLE_HID_SERVICE_UUID[] = "1812";
constexpr char BLE_SHORT_ADVERTISED_NAME[] = "SDBAND";
constexpr uint16_t BLE_APPEARANCE_GENERIC_COMPUTER = 128;
constexpr uint8_t BUTTON_NEXT_PIN = 0;
constexpr uint8_t BUTTON_SELECT_PIN = 35;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 45;
constexpr uint32_t DISPLAY_REFRESH_MS = 250;

uint32_t radioToBleCount = 0;
uint32_t bleToRadioCount = 0;
bool bleConnected = false;
bool displayDirty = true;

enum class ClientMode : uint8_t {
  Ble,
  Usb,
  Wifi,
};

ClientMode selectedMode = ClientMode::Ble;
ClientMode activeMode = ClientMode::Ble;

const char *modeName(ClientMode mode) {
  switch (mode) {
    case ClientMode::Ble:
      return "BLE";
    case ClientMode::Usb:
      return "USB-C";
    case ClientMode::Wifi:
      return "Wi-Fi";
  }
  return "?";
}

ClientMode nextMode(ClientMode mode) {
  switch (mode) {
    case ClientMode::Ble:
      return ClientMode::Usb;
    case ClientMode::Usb:
      return ClientMode::Wifi;
    case ClientMode::Wifi:
      return ClientMode::Ble;
  }
  return ClientMode::Ble;
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
    (void)server;
    (void)connInfo;
    bleConnected = true;
    displayDirty = true;
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override {
    (void)connInfo;
    (void)reason;
    bleConnected = false;
    displayDirty = true;
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

#ifndef SIDEBAND_BLE_ONLY
    radioSerial.write(reinterpret_cast<const uint8_t *>(value.data()), value.size());
#endif
    bleToRadioCount++;
    displayDirty = true;
  }
};

#ifdef SIDEBAND_IOS_DISCOVERY_HID
void setupHidDiscoveryProfile() {
  static uint8_t keyboardReportMap[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop)
      0x09, 0x06,  // Usage (Keyboard)
      0xa1, 0x01,  // Collection (Application)
      0x85, 0x01,  // Report ID (1)
      0x05, 0x07,  // Usage Page (Keyboard)
      0x19, 0xe0,  // Usage Minimum (Keyboard Left Control)
      0x29, 0xe7,  // Usage Maximum (Keyboard Right GUI)
      0x15, 0x00,  // Logical Minimum (0)
      0x25, 0x01,  // Logical Maximum (1)
      0x75, 0x01,  // Report Size (1)
      0x95, 0x08,  // Report Count (8)
      0x81, 0x02,  // Input (Data, Variable, Absolute)
      0x95, 0x01,  // Report Count (1)
      0x75, 0x08,  // Report Size (8)
      0x81, 0x01,  // Input (Constant)
      0x95, 0x06,  // Report Count (6)
      0x75, 0x08,  // Report Size (8)
      0x15, 0x00,  // Logical Minimum (0)
      0x25, 0x65,  // Logical Maximum (101)
      0x05, 0x07,  // Usage Page (Keyboard)
      0x19, 0x00,  // Usage Minimum (Reserved)
      0x29, 0x65,  // Usage Maximum (Keyboard Application)
      0x81, 0x00,  // Input (Data, Array)
      0xc0         // End Collection
  };

  NimBLEHIDDevice hid(bleServer);
  hid.setManufacturer(SIDEBAND_BLE_MANUFACTURER);
  hid.setPnp(0x02, 0x1209, 0x5dbd, 0x0100);
  hid.setHidInfo(0x00, 0x01);
  hid.setBatteryLevel(100);
  hid.setReportMap(keyboardReportMap, sizeof(keyboardReportMap));
  hid.getInputReport(1);
}
#endif

void setupBle() {
  NimBLEDevice::deinit(true);
  NimBLEDevice::init(SIDEBAND_BLE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
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

#ifdef SIDEBAND_IOS_DISCOVERY_HID
  setupHidDiscoveryProfile();
#else
  NimBLEService *deviceInfoService = bleServer->createService(BLE_DEVICE_INFO_SERVICE_UUID);
  NimBLECharacteristic *manufacturer = deviceInfoService->createCharacteristic(
      BLE_MANUFACTURER_UUID,
      NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *model = deviceInfoService->createCharacteristic(
      BLE_MODEL_UUID,
      NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *firmware = deviceInfoService->createCharacteristic(
      BLE_FIRMWARE_UUID,
      NIMBLE_PROPERTY::READ);
  manufacturer->setValue(SIDEBAND_BLE_MANUFACTURER);
  model->setValue(SIDEBAND_BLE_MODEL);
  firmware->setValue(SIDEBAND_VERSION);
#endif

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advertisementData;
  NimBLEAdvertisementData scanResponseData;

  advertisementData.setFlags(BLE_HS_ADV_F_DISC_GEN);
  advertisementData.setName(BLE_SHORT_ADVERTISED_NAME);
#ifdef SIDEBAND_IOS_DISCOVERY_HID
  advertisementData.setAppearance(HID_KEYBOARD);
  advertisementData.setCompleteServices16({NimBLEUUID(BLE_HID_SERVICE_UUID)});
#else
  advertisementData.setAppearance(BLE_APPEARANCE_GENERIC_COMPUTER);
  advertisementData.setCompleteServices(NimBLEUUID(BLE_SERVICE_UUID));
#endif
  scanResponseData.setName(SIDEBAND_BLE_NAME);

  advertising->setAdvertisementData(advertisementData);
  advertising->setScanResponseData(scanResponseData);
  advertising->enableScanResponse(true);
  advertising->start();
}

void setupButtons() {
  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
}

bool buttonPressed(uint8_t pin, bool &lastStable, uint32_t &lastChangeMs) {
  bool pressed = digitalRead(pin) == LOW;
  uint32_t now = millis();
  if (pressed != lastStable && now - lastChangeMs > BUTTON_DEBOUNCE_MS) {
    lastStable = pressed;
    lastChangeMs = now;
    return pressed;
  }
  return false;
}

void handleButtons() {
  static bool nextStable = false;
  static bool selectStable = false;
  static uint32_t nextChangedMs = 0;
  static uint32_t selectChangedMs = 0;

  if (buttonPressed(BUTTON_NEXT_PIN, nextStable, nextChangedMs)) {
    selectedMode = nextMode(selectedMode);
    displayDirty = true;
  }

  if (buttonPressed(BUTTON_SELECT_PIN, selectStable, selectChangedMs)) {
    activeMode = selectedMode;
    displayDirty = true;
  }
}

#ifdef SIDEBAND_HAS_TFT
void drawLabel(int16_t x, int16_t y, const char *label, const char *value, uint16_t color) {
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(label, x, y, 2);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, x, y + 16, 2);
}

void renderDisplay() {
  static uint32_t lastDrawMs = 0;
  uint32_t now = millis();
  if (!displayDirty && now - lastDrawMs < DISPLAY_REFRESH_MS) {
    return;
  }
  displayDirty = false;
  lastDrawMs = now;

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("SIDEBAND", 6, 4, 4);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("ADV", 6, 34, 2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(BLE_SHORT_ADVERTISED_NAME, 50, 34, 2);

  drawLabel(6, 58, "ACTIVE", modeName(activeMode), TFT_YELLOW);
  drawLabel(86, 58, "SELECT", modeName(selectedMode), TFT_ORANGE);

  drawLabel(6, 98, "BLE", bleConnected ? "CONNECTED" : "ADVERTISING", bleConnected ? TFT_GREEN : TFT_CYAN);

  char counts[32];
  snprintf(counts, sizeof(counts), "RX %lu TX %lu",
           static_cast<unsigned long>(bleToRadioCount),
           static_cast<unsigned long>(radioToBleCount));
  drawLabel(6, 138, "COUNTERS", counts, TFT_WHITE);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("BTN1 next", 6, 202, 2);
  tft.drawString("BTN2 select", 6, 220, 2);
}

void setupDisplay() {
  Serial.println("SIDEBAND display init");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);
  displayDirty = true;
  Serial.println("SIDEBAND display ready");
}
#else
void renderDisplay() {}
void setupDisplay() {}
#endif

void relayRadioToBle() {
#ifdef SIDEBAND_BLE_ONLY
  return;
#else
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
#endif
}

void printStatus() {
  static uint32_t lastStatusMs = 0;
  uint32_t now = millis();
  if (now - lastStatusMs < 5000) {
    return;
  }
  lastStatusMs = now;

  Serial.printf(
      "SIDEBAND status=running mode=%s selected=%s adv=%s name=\"%s\" ble=%s radio_to_ble=%lu ble_to_radio=%lu\n",
      modeName(activeMode),
      modeName(selectedMode),
      BLE_SHORT_ADVERTISED_NAME,
      SIDEBAND_BLE_NAME,
      bleConnected ? "connected" : "advertising",
      static_cast<unsigned long>(radioToBleCount),
      static_cast<unsigned long>(bleToRadioCount));
}

}  // namespace

void setup() {
  Serial.begin(SIDEBAND_SERIAL_BAUD);
  delay(300);

  Serial.println("SIDEBAND boot");
  setupButtons();
  setupDisplay();
#ifdef SIDEBAND_BLE_ONLY
  Serial.println("BLE diagnostic mode: Bluetooth Classic disabled");
#else
  Serial.println("Primary bridge target: Bluetooth Classic capable ESP32");

  if (activeMode != ClientMode::Ble) {
    radioSerial.begin(SIDEBAND_DEVICE_NAME, true);
  }
#endif
  setupBle();
}

void loop() {
  handleButtons();
  relayRadioToBle();
  renderDisplay();
  printStatus();
  delay(5);
}

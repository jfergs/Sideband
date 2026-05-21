#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#ifdef SIDEBAND_HAS_TFT
#include <TFT_eSPI.h>
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "sideband_config.example.h"
#endif

#ifndef SIDEBAND_RADIO_ALT_NAME_HINT
#define SIDEBAND_RADIO_ALT_NAME_HINT "TH-D74"
#endif

#ifndef SIDEBAND_RADIO_PAIRING_PIN
#define SIDEBAND_RADIO_PAIRING_PIN ""
#endif

#ifndef SIDEBAND_RADIO_MAC
#define SIDEBAND_RADIO_MAC ""
#endif

#ifndef SIDEBAND_WIFI_AP_PASSWORD
#define SIDEBAND_WIFI_AP_PASSWORD "sideband-bridge"
#endif

#ifndef SIDEBAND_WIFI_TCP_PORT
#define SIDEBAND_WIFI_TCP_PORT 8001
#endif

#ifndef SIDEBAND_MDNS_HOSTNAME
#define SIDEBAND_MDNS_HOSTNAME "sideband"
#endif

#ifndef SIDEBAND_TFT_ANIMATIONS
#define SIDEBAND_TFT_ANIMATIONS 0
#endif

namespace {

BluetoothSerial radioSerial;
Preferences preferences;
WiFiServer wifiServer(SIDEBAND_WIFI_TCP_PORT);
WiFiClient wifiClient;
#ifdef SIDEBAND_HAS_TFT
TFT_eSPI tft;
#endif

constexpr uint8_t BUTTON_NEXT_PIN = 0;
constexpr uint8_t BUTTON_SELECT_PIN = 35;
constexpr uint8_t KISS_FEND = 0xC0;
constexpr uint8_t KISS_FESC = 0xDB;
constexpr uint8_t KISS_TFEND = 0xDC;
constexpr uint8_t KISS_TFESC = 0xDD;
constexpr size_t KISS_MAX_FRAME_BYTES = 330;
constexpr size_t RADIO_RX_BUFFER_BYTES = 512;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 45;
constexpr uint32_t DISPLAY_REFRESH_MS = 250;
constexpr uint32_t RADIO_CONNECT_INTERVAL_MS = 15000;
constexpr uint32_t RADIO_SCAN_TIMEOUT_MS = 6000;
constexpr uint32_t RADIO_TEST_TIMEOUT_MS = 3000;
constexpr char SETTINGS_NAMESPACE[] = "sideband";
constexpr char ACTIVE_MODE_KEY[] = "mode";
constexpr char RADIO_MAC_KEY[] = "radio_mac";
constexpr char RADIO_NAME_KEY[] = "radio_name";
constexpr char RADIO_ALT_NAME_KEY[] = "radio_alt";
constexpr char TCP_MODE_KEY[] = "tcp_mode";

uint32_t radioToClientCount = 0;
uint32_t clientToRadioCount = 0;
bool displayDirty = true;
bool radioSerialStarted = false;
bool wifiStarted = false;
bool mdnsStarted = false;
String wifiApSsid = "";

enum class ClientMode : uint8_t {
  Usb = 0,
  Wifi = 1,
};

enum class RadioState : uint8_t {
  Disabled,
  Idle,
  Scanning,
  Pairing,
  Connecting,
  Reconnecting,
  Connected,
  Error,
};

enum class TcpIngressMode : uint8_t {
  Kiss = 0,
  Raw = 1,
};

enum class TowerIconMode : uint8_t {
  Question,
  Lightning,
  Arcs,
};

enum class RadioTestState : uint8_t {
  Idle,
  Wait,
  RxOk,
  NoData,
  NoLink,
};

ClientMode selectedMode = ClientMode::Usb;
ClientMode activeMode = ClientMode::Usb;
TcpIngressMode tcpIngressMode = TcpIngressMode::Kiss;
RadioState radioState = RadioState::Disabled;
RadioTestState radioTestState = RadioTestState::Idle;
String radioPeerName = "";
String radioPeerAddress = "";
String radioTargetName = "";
String radioTargetAltName = "";
String radioTargetMac = "";
char radioPairingCode[8] = "";
uint32_t radioConnectAttempts = 0;
uint32_t radioReconnects = 0;
uint32_t radioPairingEvents = 0;
uint32_t radioTestStartedMs = 0;
uint32_t radioTestTxCount = 0;
uint32_t radioTestRxCount = 0;
uint32_t radioRxByteCount = 0;
uint32_t radioRxBufferWrites = 0;
uint32_t radioRawCommandCount = 0;
uint32_t kissMalformedCount = 0;
uint32_t kissClientFrameCount = 0;
uint32_t kissClientByteCount = 0;
uint32_t kissRadioFrameCount = 0;
uint32_t kissRadioByteCount = 0;
char kissLastError[24] = "-";
uint8_t radioRxBuffer[RADIO_RX_BUFFER_BYTES];
size_t radioRxBufferStart = 0;
size_t radioRxBufferLength = 0;

struct KissIngress {
  uint8_t buffer[KISS_MAX_FRAME_BYTES];
  size_t length = 0;
  bool inFrame = false;
  bool escaped = false;
  bool malformed = false;
};

KissIngress usbKissIngress;
KissIngress wifiKissIngress;
KissIngress radioKissMonitor;

void renderDisplay();

bool serialDiagnosticsEnabled() {
  return activeMode != ClientMode::Usb;
}

void logLine(const char *line) {
  if (serialDiagnosticsEnabled()) {
    Serial.println(line);
  }
}

void setKissLastError(const char *error) {
  strncpy(kissLastError, error, sizeof(kissLastError) - 1);
  kissLastError[sizeof(kissLastError) - 1] = '\0';
}

void rememberRadioRxBytes(const uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    size_t writeIndex = (radioRxBufferStart + radioRxBufferLength) % RADIO_RX_BUFFER_BYTES;
    radioRxBuffer[writeIndex] = data[i];
    if (radioRxBufferLength < RADIO_RX_BUFFER_BYTES) {
      radioRxBufferLength++;
    } else {
      radioRxBufferStart = (radioRxBufferStart + 1) % RADIO_RX_BUFFER_BYTES;
    }
    radioRxBufferWrites++;
  }
}

uint8_t radioRxBufferAt(size_t offset) {
  return radioRxBuffer[(radioRxBufferStart + offset) % RADIO_RX_BUFFER_BYTES];
}

void writeKissByteEscaped(Stream &stream, uint8_t value) {
  if (value == KISS_FEND) {
    stream.write(KISS_FESC);
    stream.write(KISS_TFEND);
    return;
  }
  if (value == KISS_FESC) {
    stream.write(KISS_FESC);
    stream.write(KISS_TFESC);
    return;
  }
  stream.write(value);
}

void writeKissFrameToRadio(const uint8_t *payload, size_t length) {
  if (length == 0 || radioState != RadioState::Connected || !radioSerial.connected()) {
    return;
  }

  radioSerial.write(KISS_FEND);
  for (size_t i = 0; i < length; i++) {
    writeKissByteEscaped(radioSerial, payload[i]);
  }
  radioSerial.write(KISS_FEND);
  kissClientFrameCount++;
  kissClientByteCount += length;
  clientToRadioCount++;
  displayDirty = true;
}

void resetKissIngress(KissIngress &ingress) {
  ingress.length = 0;
  ingress.escaped = false;
  ingress.malformed = false;
}

bool appendKissByte(KissIngress &ingress, uint8_t value) {
  if (ingress.length >= KISS_MAX_FRAME_BYTES) {
    ingress.malformed = true;
    setKissLastError("frame_overflow");
    return false;
  }
  ingress.buffer[ingress.length++] = value;
  return true;
}

bool processKissIngressByte(KissIngress &ingress, uint8_t value) {
  if (value == KISS_FEND) {
    if (ingress.inFrame && ingress.length > 0 && !ingress.malformed) {
      writeKissFrameToRadio(ingress.buffer, ingress.length);
    } else if (ingress.malformed) {
      kissMalformedCount++;
      displayDirty = true;
    }
    ingress.inFrame = true;
    resetKissIngress(ingress);
    return true;
  }

  if (!ingress.inFrame) {
    return false;
  }

  if (ingress.malformed) {
    return true;
  }

  if (ingress.escaped) {
    ingress.escaped = false;
    if (value == KISS_TFEND) {
      appendKissByte(ingress, KISS_FEND);
      return true;
    }
    if (value == KISS_TFESC) {
      appendKissByte(ingress, KISS_FESC);
      return true;
    }
    ingress.malformed = true;
    setKissLastError("bad_escape");
    return true;
  }

  if (value == KISS_FESC) {
    ingress.escaped = true;
    return true;
  }

  appendKissByte(ingress, value);
  return true;
}

void observeRadioKissByte(uint8_t value) {
  if (value == KISS_FEND) {
    if (radioKissMonitor.inFrame && radioKissMonitor.length > 0 && !radioKissMonitor.malformed) {
      kissRadioFrameCount++;
      kissRadioByteCount += radioKissMonitor.length;
      displayDirty = true;
    } else if (radioKissMonitor.malformed) {
      kissMalformedCount++;
      displayDirty = true;
    }
    radioKissMonitor.inFrame = true;
    resetKissIngress(radioKissMonitor);
    return;
  }

  if (!radioKissMonitor.inFrame || radioKissMonitor.malformed) {
    return;
  }

  if (radioKissMonitor.escaped) {
    radioKissMonitor.escaped = false;
    if (value == KISS_TFEND) {
      appendKissByte(radioKissMonitor, KISS_FEND);
      return;
    }
    if (value == KISS_TFESC) {
      appendKissByte(radioKissMonitor, KISS_FESC);
      return;
    }
    radioKissMonitor.malformed = true;
    setKissLastError("radio_bad_escape");
    return;
  }

  if (value == KISS_FESC) {
    radioKissMonitor.escaped = true;
    return;
  }

  appendKissByte(radioKissMonitor, value);
}

const char *modeName(ClientMode mode) {
  switch (mode) {
    case ClientMode::Usb:
      return "USB-C";
    case ClientMode::Wifi:
      return "Wi-Fi";
  }
  return "?";
}

const char *tcpIngressModeName(TcpIngressMode mode) {
  switch (mode) {
    case TcpIngressMode::Kiss:
      return "KISS";
    case TcpIngressMode::Raw:
      return "RAW";
  }
  return "?";
}

const char *radioStateName(RadioState state) {
  switch (state) {
    case RadioState::Disabled:
      return "OFF";
    case RadioState::Idle:
      return "IDLE";
    case RadioState::Scanning:
      return "SCAN";
    case RadioState::Pairing:
      return "PAIR";
    case RadioState::Connecting:
      return "CONNECT";
    case RadioState::Reconnecting:
      return "RECONNECT";
    case RadioState::Connected:
      return "LINKED";
    case RadioState::Error:
      return "ERROR";
  }
  return "?";
}

const char *radioTestStateName(RadioTestState state) {
  switch (state) {
    case RadioTestState::Idle:
      return "-";
    case RadioTestState::Wait:
      return "TEST WAIT";
    case RadioTestState::RxOk:
      return "TEST OK";
    case RadioTestState::NoData:
      return "NO DATA";
    case RadioTestState::NoLink:
      return "NO LINK";
  }
  return "?";
}

ClientMode nextMode(ClientMode mode) {
  return mode == ClientMode::Usb ? ClientMode::Wifi : ClientMode::Usb;
}

ClientMode modeFromValue(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(ClientMode::Wifi):
      return ClientMode::Wifi;
    case static_cast<uint8_t>(ClientMode::Usb):
    default:
      return ClientMode::Usb;
  }
}

TcpIngressMode tcpIngressModeFromValue(uint8_t value) {
  switch (value) {
    case static_cast<uint8_t>(TcpIngressMode::Raw):
      return TcpIngressMode::Raw;
    case static_cast<uint8_t>(TcpIngressMode::Kiss):
    default:
      return TcpIngressMode::Kiss;
  }
}

void setRadioState(RadioState state) {
  if (radioState == state) {
    return;
  }
  radioState = state;
  displayDirty = true;
}

void setRadioTestState(RadioTestState state) {
  if (radioTestState == state) {
    return;
  }
  radioTestState = state;
  displayDirty = true;
}

bool configuredRadioMacPresent() {
  return radioTargetMac.length() >= 11;
}

void setRadioPairingCode(uint32_t code) {
  snprintf(radioPairingCode, sizeof(radioPairingCode), "%06lu", static_cast<unsigned long>(code));
  radioPairingEvents++;
  setRadioState(RadioState::Pairing);
  displayDirty = true;
}

void clearRadioPairingCode() {
  if (radioPairingCode[0] == '\0') {
    return;
  }
  radioPairingCode[0] = '\0';
  displayDirty = true;
}

bool radioNameMatches(const std::string &name) {
  if (name.empty()) {
    return false;
  }

  String candidate(name.c_str());
  candidate.toUpperCase();
  String primary(radioTargetName);
  String alternate(radioTargetAltName);
  primary.toUpperCase();
  alternate.toUpperCase();
  return candidate.indexOf(primary) >= 0 || candidate.indexOf(alternate) >= 0;
}

void loadSettings() {
  preferences.begin(SETTINGS_NAMESPACE, false);
  activeMode = modeFromValue(preferences.getUChar(ACTIVE_MODE_KEY, static_cast<uint8_t>(ClientMode::Usb)));
  selectedMode = activeMode;
  radioTargetName = preferences.getString(RADIO_NAME_KEY, SIDEBAND_RADIO_NAME_HINT);
  radioTargetAltName = preferences.getString(RADIO_ALT_NAME_KEY, SIDEBAND_RADIO_ALT_NAME_HINT);
  radioTargetMac = preferences.getString(RADIO_MAC_KEY, SIDEBAND_RADIO_MAC);
  tcpIngressMode = tcpIngressModeFromValue(preferences.getUChar(TCP_MODE_KEY, static_cast<uint8_t>(TcpIngressMode::Kiss)));
}

void saveActiveMode(ClientMode mode) {
  preferences.putUChar(ACTIVE_MODE_KEY, static_cast<uint8_t>(mode));
}

void setupRadioTransport() {
  if (radioSerialStarted) {
    return;
  }

  radioSerial.enableSSP();
  radioSerial.onConfirmRequest([](uint32_t code) {
    setRadioPairingCode(code);
    if (serialDiagnosticsEnabled()) {
      Serial.printf("SIDEBAND radio pairing confirm code=%06lu\n", static_cast<unsigned long>(code));
    }
    radioSerial.confirmReply(true);
  });
  radioSerial.onAuthComplete([](boolean success) {
    clearRadioPairingCode();
    setRadioState(success ? RadioState::Connecting : RadioState::Error);
    if (serialDiagnosticsEnabled()) {
      Serial.printf("SIDEBAND radio pairing auth=%s\n", success ? "ok" : "failed");
    }
  });
  if (strlen(SIDEBAND_RADIO_PAIRING_PIN) > 0) {
    radioSerial.setPin(SIDEBAND_RADIO_PAIRING_PIN);
  }

  radioSerialStarted = radioSerial.begin(SIDEBAND_DEVICE_NAME, true);
  setRadioState(radioSerialStarted ? RadioState::Idle : RadioState::Error);
}

bool connectRadioFromScan() {
  setRadioState(RadioState::Scanning);
  logLine("SIDEBAND radio scan start");
  BTScanResults *results = radioSerial.discover(RADIO_SCAN_TIMEOUT_MS);
  if (results == nullptr) {
    logLine("SIDEBAND radio scan failed");
    return false;
  }

  int count = results->getCount();
  if (serialDiagnosticsEnabled()) {
    Serial.printf("SIDEBAND radio scan results=%d\n", count);
  }
  for (int i = 0; i < count; i++) {
    BTAdvertisedDevice *device = results->getDevice(i);
    if (device == nullptr) {
      continue;
    }

    std::string deviceName = device->haveName() ? device->getName() : "";
    if (!radioNameMatches(deviceName)) {
      continue;
    }

    radioPeerName = deviceName.c_str();
    radioPeerAddress = device->getAddress().toString(true);
    setRadioState(RadioState::Connecting);
    radioConnectAttempts++;
    if (radioSerial.connect(device->getAddress())) {
      clearRadioPairingCode();
      setRadioState(RadioState::Connected);
      return true;
    }
  }

  logLine("SIDEBAND radio not found");
  return false;
}

void maintainRadioConnection() {
  static uint32_t lastConnectMs = 0;
  setupRadioTransport();
  if (!radioSerialStarted) {
    return;
  }

  if (radioState == RadioState::Connected) {
    if (radioSerial.connected()) {
      return;
    }

    radioReconnects++;
    radioSerial.disconnect();
    clearRadioPairingCode();
    setRadioState(RadioState::Reconnecting);
  }

  uint32_t now = millis();
  if (lastConnectMs != 0 && now - lastConnectMs < RADIO_CONNECT_INTERVAL_MS) {
    return;
  }
  lastConnectMs = now;

  radioConnectAttempts++;
  setRadioState(RadioState::Connecting);
  clearRadioPairingCode();

  if (configuredRadioMacPresent()) {
    radioPeerName = radioTargetName;
    radioPeerAddress = radioTargetMac;
    if (radioSerial.connect(BTAddress(radioTargetMac))) {
      setRadioState(RadioState::Connected);
      return;
    }
    setRadioState(RadioState::Reconnecting);
    return;
  }

  if (!connectRadioFromScan()) {
    setRadioState(RadioState::Idle);
  }
}

void setupWifiTransport() {
  if (wifiStarted || activeMode != ClientMode::Wifi) {
    return;
  }

  uint64_t chipId = ESP.getEfuseMac();
  char ssid[24];
  snprintf(ssid, sizeof(ssid), "Sideband-%04X", static_cast<uint16_t>(chipId & 0xffff));
  wifiApSsid = ssid;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(wifiApSsid.c_str(), SIDEBAND_WIFI_AP_PASSWORD);
  wifiServer.begin();
  wifiServer.setNoDelay(true);
  if (MDNS.begin(SIDEBAND_MDNS_HOSTNAME)) {
    MDNS.addService("kiss-tnc", "tcp", SIDEBAND_WIFI_TCP_PORT);
    MDNS.addServiceTxt("kiss-tnc", "tcp", "model", "sideband-bridge");
    MDNS.addServiceTxt("kiss-tnc", "tcp", "transport", "wifi");
    MDNS.addServiceTxt("kiss-tnc", "tcp", "radio", radioTargetName.c_str());
    mdnsStarted = true;
  } else {
    mdnsStarted = false;
  }
  wifiStarted = true;
  displayDirty = true;
}

void maintainWifiClient() {
  if (activeMode != ClientMode::Wifi || !wifiStarted) {
    return;
  }

  if (wifiClient && wifiClient.connected()) {
    return;
  }

  WiFiClient candidate = wifiServer.available();
  if (candidate) {
    if (wifiClient) {
      wifiClient.stop();
    }
    wifiClient = candidate;
    wifiClient.setNoDelay(true);
    displayDirty = true;
  }
}

void relayRadioToClient() {
  if (radioState != RadioState::Connected || !radioSerial.connected()) {
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
  if (len == 0) {
    return;
  }
  radioRxByteCount += len;
  rememberRadioRxBytes(buffer, len);
  for (size_t i = 0; i < len; i++) {
    observeRadioKissByte(buffer[i]);
  }
  displayDirty = true;

  if (radioTestState == RadioTestState::Wait) {
    radioTestRxCount++;
    setRadioTestState(RadioTestState::RxOk);
  }

  if (activeMode == ClientMode::Wifi) {
    if (wifiClient && wifiClient.connected()) {
      wifiClient.write(buffer, len);
      radioToClientCount++;
      displayDirty = true;
    }
    return;
  }

  Serial.write(buffer, len);
  radioToClientCount++;
  displayDirty = true;
}

void startRadioLinkTest() {
  static const uint8_t testFrame[] = {KISS_FEND, 0x00, KISS_FEND};

  if (radioState != RadioState::Connected || !radioSerial.connected()) {
    setRadioTestState(RadioTestState::NoLink);
    radioTestStartedMs = millis();
    return;
  }

  radioSerial.write(testFrame, sizeof(testFrame));
  radioTestTxCount++;
  radioTestStartedMs = millis();
  setRadioTestState(RadioTestState::Wait);
}

void maintainRadioLinkTest() {
  if (radioTestState != RadioTestState::Wait) {
    return;
  }

  if (millis() - radioTestStartedMs >= RADIO_TEST_TIMEOUT_MS) {
    setRadioTestState(RadioTestState::NoData);
  }
}

void relayWifiToRadio() {
  if (activeMode != ClientMode::Wifi || !(wifiClient && wifiClient.connected())) {
    return;
  }

  bool rawBytesForwarded = false;
  while (wifiClient.available() > 0) {
    uint8_t value = static_cast<uint8_t>(wifiClient.read());
    if (tcpIngressMode == TcpIngressMode::Raw) {
      if (radioState == RadioState::Connected && radioSerial.connected()) {
        radioSerial.write(value);
        rawBytesForwarded = true;
      }
      continue;
    }
    processKissIngressByte(wifiKissIngress, value);
  }
  if (rawBytesForwarded) {
    clientToRadioCount++;
    displayDirty = true;
  }
}

bool isCommandPrefix(const String &input) {
  return input == "help" || input == "?" || input == "status" ||
         input.startsWith("radio ") || input.startsWith("mode ") ||
         input.startsWith("tcp ") || input.startsWith("kiss ");
}

void printSerialHelp() {
  if (!serialDiagnosticsEnabled()) {
    return;
  }
  Serial.println("SIDEBAND commands:");
  Serial.println("  status");
  Serial.println("  mode usb");
  Serial.println("  mode wifi");
  Serial.println("  tcp kiss");
  Serial.println("  tcp raw");
  Serial.println("  kiss stats");
  Serial.println("  kiss reset");
  Serial.println("  radio show");
  Serial.println("  radio test");
  Serial.println("  radio dump");
  Serial.println("  radio replay");
  Serial.println("  radio raw <command>");
  Serial.println("  radio mac <AA:BB:CC:DD:EE:FF>");
  Serial.println("  radio name <name>");
  Serial.println("  radio alt <name>");
  Serial.println("  radio clear");
}

void printRadioConfig() {
  if (!serialDiagnosticsEnabled()) {
    return;
  }
  Serial.printf("SIDEBAND radio config name=\"%s\" alt=\"%s\" mac=%s source=preferences\n",
                radioTargetName.c_str(),
                radioTargetAltName.c_str(),
                radioTargetMac.length() > 0 ? "configured" : "unset");
  Serial.printf("SIDEBAND tcp ingress=%s\n", tcpIngressModeName(tcpIngressMode));
}

void printKissStats() {
  if (!serialDiagnosticsEnabled()) {
    return;
  }

  Serial.printf("SIDEBAND kiss client_frames=%lu client_bytes=%lu radio_frames=%lu radio_bytes=%lu malformed=%lu last_error=%s\n",
                static_cast<unsigned long>(kissClientFrameCount),
                static_cast<unsigned long>(kissClientByteCount),
                static_cast<unsigned long>(kissRadioFrameCount),
                static_cast<unsigned long>(kissRadioByteCount),
                static_cast<unsigned long>(kissMalformedCount),
                kissLastError);
}

void resetKissStats() {
  kissClientFrameCount = 0;
  kissClientByteCount = 0;
  kissRadioFrameCount = 0;
  kissRadioByteCount = 0;
  kissMalformedCount = 0;
  setKissLastError("-");
  resetKissIngress(usbKissIngress);
  resetKissIngress(wifiKissIngress);
  resetKissIngress(radioKissMonitor);
  displayDirty = true;
  logLine("SIDEBAND kiss stats reset");
}

void saveRadioTargetMac(const String &mac) {
  radioTargetMac = mac;
  preferences.putString(RADIO_MAC_KEY, radioTargetMac);
  radioPeerAddress = "";
  displayDirty = true;
}

void saveRadioTargetName(const String &name) {
  radioTargetName = name.length() > 0 ? name : String(SIDEBAND_RADIO_NAME_HINT);
  preferences.putString(RADIO_NAME_KEY, radioTargetName);
  displayDirty = true;
}

void saveRadioTargetAltName(const String &name) {
  radioTargetAltName = name.length() > 0 ? name : String(SIDEBAND_RADIO_ALT_NAME_HINT);
  preferences.putString(RADIO_ALT_NAME_KEY, radioTargetAltName);
  displayDirty = true;
}

void clearRadioConfig() {
  radioTargetMac = "";
  radioTargetName = SIDEBAND_RADIO_NAME_HINT;
  radioTargetAltName = SIDEBAND_RADIO_ALT_NAME_HINT;
  preferences.putString(RADIO_MAC_KEY, radioTargetMac);
  preferences.putString(RADIO_NAME_KEY, radioTargetName);
  preferences.putString(RADIO_ALT_NAME_KEY, radioTargetAltName);
  radioPeerName = "";
  radioPeerAddress = "";
  clearRadioPairingCode();
  setRadioState(RadioState::Idle);
  displayDirty = true;
}

void sendRadioRawCommand(const String &rawCommand) {
  if (radioState != RadioState::Connected || !radioSerial.connected()) {
    logLine("SIDEBAND radio raw failed: no link");
    return;
  }

  String command(rawCommand);
  command.trim();
  if (command.length() == 0) {
    logLine("SIDEBAND radio raw failed: empty command");
    return;
  }

  radioSerial.print(command);
  radioSerial.print("\r");
  radioRawCommandCount++;
  displayDirty = true;
  logLine("SIDEBAND radio raw command sent");
}

void dumpRadioRxBuffer() {
  if (!serialDiagnosticsEnabled()) {
    return;
  }

  Serial.printf("SIDEBAND radio rx buffer bytes=%u writes=%lu\n",
                static_cast<unsigned>(radioRxBufferLength),
                static_cast<unsigned long>(radioRxBufferWrites));
  for (size_t i = 0; i < radioRxBufferLength; i++) {
    if (i % 16 == 0) {
      Serial.printf("%04x:", static_cast<unsigned>(i));
    }
    Serial.printf(" %02x", radioRxBufferAt(i));
    if (i % 16 == 15 || i + 1 == radioRxBufferLength) {
      Serial.println();
    }
  }
}

void replayRadioRxBufferToWifi() {
  if (activeMode != ClientMode::Wifi || !(wifiClient && wifiClient.connected())) {
    logLine("SIDEBAND radio replay failed: no Wi-Fi client");
    return;
  }

  for (size_t i = 0; i < radioRxBufferLength; i++) {
    wifiClient.write(radioRxBufferAt(i));
  }
  radioToClientCount++;
  displayDirty = true;
  logLine("SIDEBAND radio rx buffer replayed");
}

void saveModeAndRestart(ClientMode mode) {
  activeMode = mode;
  selectedMode = mode;
  saveActiveMode(mode);
  displayDirty = true;
  if (serialDiagnosticsEnabled()) {
    Serial.printf("SIDEBAND saved mode=%s restarting\n", modeName(activeMode));
  }
  delay(250);
  ESP.restart();
}

void saveTcpIngressMode(TcpIngressMode mode) {
  tcpIngressMode = mode;
  preferences.putUChar(TCP_MODE_KEY, static_cast<uint8_t>(mode));
  displayDirty = true;
  if (serialDiagnosticsEnabled()) {
    Serial.printf("SIDEBAND tcp ingress saved=%s\n", tcpIngressModeName(tcpIngressMode));
  }
}

void handleSerialCommand(const String &command) {
  if (command.length() == 0 || !isCommandPrefix(command)) {
    return;
  }

  if (command == "help" || command == "?") {
    printSerialHelp();
    return;
  }

  if (command == "status") {
    printRadioConfig();
    return;
  }

  if (command == "mode usb") {
    saveModeAndRestart(ClientMode::Usb);
    return;
  }

  if (command == "mode wifi") {
    saveModeAndRestart(ClientMode::Wifi);
    return;
  }

  if (command == "tcp kiss") {
    saveTcpIngressMode(TcpIngressMode::Kiss);
    return;
  }

  if (command == "tcp raw") {
    saveTcpIngressMode(TcpIngressMode::Raw);
    return;
  }

  if (command == "kiss stats") {
    printKissStats();
    return;
  }

  if (command == "kiss reset") {
    resetKissStats();
    return;
  }

  if (command == "radio show") {
    printRadioConfig();
    return;
  }

  if (command == "radio test") {
    startRadioLinkTest();
    return;
  }

  if (command == "radio dump") {
    dumpRadioRxBuffer();
    return;
  }

  if (command == "radio replay") {
    replayRadioRxBufferToWifi();
    return;
  }

  if (command.startsWith("radio raw ")) {
    sendRadioRawCommand(command.substring(strlen("radio raw ")));
    return;
  }

  if (command == "radio clear") {
    clearRadioConfig();
    logLine("SIDEBAND radio config cleared");
    printRadioConfig();
    return;
  }

  if (command.startsWith("radio mac ")) {
    String mac = command.substring(strlen("radio mac "));
    mac.trim();
    saveRadioTargetMac(mac);
    logLine("SIDEBAND radio mac saved");
    printRadioConfig();
    return;
  }

  if (command.startsWith("radio name ")) {
    String name = command.substring(strlen("radio name "));
    name.trim();
    saveRadioTargetName(name);
    logLine("SIDEBAND radio name saved");
    printRadioConfig();
    return;
  }

  if (command.startsWith("radio alt ")) {
    String name = command.substring(strlen("radio alt "));
    name.trim();
    saveRadioTargetAltName(name);
    logLine("SIDEBAND radio alternate name saved");
    printRadioConfig();
  }
}

void relayUsbToRadio() {
  static String commandInput = "";

  while (Serial.available() > 0) {
    uint8_t ch = static_cast<uint8_t>(Serial.read());

    if (processKissIngressByte(usbKissIngress, ch)) {
      continue;
    }

    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      commandInput.trim();
      handleSerialCommand(commandInput);
      commandInput = "";
      continue;
    }
    if (commandInput.length() < 96 && ch >= 0x20 && ch <= 0x7e) {
      commandInput += static_cast<char>(ch);
    }
  }
}

void handleButtons() {
  static bool nextStable = false;
  static bool selectStable = false;
  static uint32_t nextChangedMs = 0;
  static uint32_t selectChangedMs = 0;

  auto buttonPressed = [](uint8_t pin, bool &lastStable, uint32_t &lastChangeMs) {
    bool pressed = digitalRead(pin) == LOW;
    uint32_t now = millis();
    if (pressed != lastStable && now - lastChangeMs > BUTTON_DEBOUNCE_MS) {
      lastStable = pressed;
      lastChangeMs = now;
      return pressed;
    }
    return false;
  };

  if (buttonPressed(BUTTON_NEXT_PIN, nextStable, nextChangedMs)) {
    selectedMode = nextMode(selectedMode);
    displayDirty = true;
  }

  if (buttonPressed(BUTTON_SELECT_PIN, selectStable, selectChangedMs)) {
    if (selectedMode != activeMode) {
      saveModeAndRestart(selectedMode);
      return;
    }
    startRadioLinkTest();
  }
}

#ifdef SIDEBAND_HAS_TFT
void drawStaticLabel(int16_t x, int16_t y, const char *label) {
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString(label, x, y, 2);
}

void drawValueField(int16_t x, int16_t y, int16_t width, const char *value, const char *previous, uint16_t color) {
  if (strcmp(value, previous) == 0) {
    return;
  }

  tft.fillRect(x, y, width, 18, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(value, x, y, 2);
}

void copyPreviousValue(char *target, size_t targetSize, const char *value) {
  strncpy(target, value, targetSize - 1);
  target[targetSize - 1] = '\0';
}

uint16_t radioStateColor(RadioState state) {
  switch (state) {
    case RadioState::Connected:
      return TFT_GREEN;
    case RadioState::Pairing:
      return TFT_ORANGE;
    case RadioState::Connecting:
    case RadioState::Reconnecting:
      return TFT_YELLOW;
    case RadioState::Error:
      return TFT_RED;
    case RadioState::Disabled:
      return TFT_DARKGREY;
    case RadioState::Scanning:
    case RadioState::Idle:
      return TFT_CYAN;
  }
  return TFT_WHITE;
}

TowerIconMode towerIconMode() {
  if (radioState == RadioState::Connected &&
      (activeMode == ClientMode::Usb || (wifiClient && wifiClient.connected()))) {
    return TowerIconMode::Arcs;
  }
  if (radioState == RadioState::Connecting || radioState == RadioState::Reconnecting ||
      radioState == RadioState::Pairing || radioState == RadioState::Connected) {
    return TowerIconMode::Lightning;
  }
  return TowerIconMode::Question;
}

void drawRadioTowerIcon(RadioState state) {
  constexpr int16_t originX = 186;
  constexpr int16_t originY = 36;
  constexpr int16_t width = 48;
  constexpr int16_t height = 74;
  constexpr int16_t towerX = originX + 24;
  constexpr int16_t towerTop = originY + 20;
  constexpr int16_t towerBottom = originY + 58;
  uint16_t color = radioStateColor(state);
  bool animationsEnabled = SIDEBAND_TFT_ANIMATIONS;
  TowerIconMode mode = towerIconMode();
  static bool iconDrawn = false;
  static RadioState previousState = RadioState::Error;
  static TowerIconMode previousMode = TowerIconMode::Question;
  if (!animationsEnabled && iconDrawn && state == previousState && mode == previousMode) {
    return;
  }

  uint8_t frame = animationsEnabled ? ((millis() / DISPLAY_REFRESH_MS) % 4) : 0;
  int16_t pulse = static_cast<int16_t>(frame) * 3;

  tft.fillRect(originX, originY, width, height, TFT_BLACK);
  tft.drawLine(towerX, towerTop, towerX, towerBottom, color);
  tft.drawLine(towerX, towerTop + 6, towerX - 12, towerBottom, color);
  tft.drawLine(towerX, towerTop + 6, towerX + 12, towerBottom, color);
  tft.drawLine(towerX - 9, towerBottom - 10, towerX + 9, towerBottom - 10, color);
  tft.drawLine(towerX - 6, towerBottom, towerX + 6, towerBottom, color);
  tft.fillCircle(towerX, towerTop, 3, color);

  if (mode == TowerIconMode::Arcs) {
    tft.drawArc(towerX, towerTop, 10 + pulse, 8 + pulse, 300, 60, color, TFT_BLACK, false);
    tft.drawArc(towerX, towerTop, 18 + pulse, 16 + pulse, 300, 60, color, TFT_BLACK, false);
    tft.drawArc(towerX, towerTop, 10 + pulse, 8 + pulse, 120, 240, color, TFT_BLACK, false);
    tft.drawArc(towerX, towerTop, 18 + pulse, 16 + pulse, 120, 240, color, TFT_BLACK, false);
  } else if (mode == TowerIconMode::Lightning) {
    tft.drawLine(towerX - 18, towerTop - 14, towerX - 5, towerTop - 2, color);
    tft.drawLine(towerX - 5, towerTop - 2, towerX - 12, towerTop + 2, color);
    tft.drawLine(towerX - 12, towerTop + 2, towerX, towerTop, color);
    tft.drawLine(towerX + 18, towerTop - 14, towerX + 5, towerTop - 2, color);
    tft.drawLine(towerX + 5, towerTop - 2, towerX + 12, towerTop + 2, color);
    tft.drawLine(towerX + 12, towerTop + 2, towerX, towerTop, color);
  } else {
    tft.drawCircle(towerX - 16, towerTop - 14, 3, color);
    tft.drawLine(towerX - 14, towerTop - 12, towerX - 10, towerTop - 8, color);
    tft.fillCircle(towerX - 8, towerTop - 6, 1, color);
    tft.drawCircle(towerX + 16, towerTop - 14, 3, color);
    tft.drawLine(towerX + 14, towerTop - 12, towerX + 10, towerTop - 8, color);
    tft.fillCircle(towerX + 8, towerTop - 6, 1, color);
  }

  if (state == RadioState::Pairing) {
    tft.drawString("PIN", originX + 12, originY + 2, 1);
  } else if (state == RadioState::Connected) {
    tft.fillCircle(towerX, towerTop, 5, color);
  } else {
    tft.fillCircle(towerX, towerTop, 2, color);
  }

  iconDrawn = true;
  previousState = state;
  previousMode = mode;
}

void drawDisplayFrame() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("SIDEBAND", 6, 8, 2);

  drawStaticLabel(6, 34, "CLIENT");
  drawStaticLabel(6, 58, "ACTIVE");
  drawStaticLabel(86, 58, "SELECT");
  drawStaticLabel(6, 98, "LINK");
  drawStaticLabel(6, 138, "RADIO");
  drawStaticLabel(6, 168, "TEST");
  drawStaticLabel(86, 168, "COUNTERS");
  drawStaticLabel(6, 198, "DEVICES");
}

void renderDisplay() {
  static uint32_t lastDrawMs = 0;
  static bool frameDrawn = false;
  static char previousClient[32] = "";
  static char previousActiveMode[16] = "";
  static char previousSelectedMode[16] = "";
  static char previousClientStatus[32] = "";
  static char previousRadioStatus[32] = "";
  static char previousTestStatus[16] = "";
  static char previousCounters[32] = "";
  static char previousClientDevice[32] = "";
  static char previousRadioDevice[32] = "";
  static char previousWifiSecret[32] = "";
  uint32_t now = millis();
  if (!displayDirty && now - lastDrawMs < DISPLAY_REFRESH_MS) {
    return;
  }
  displayDirty = false;
  lastDrawMs = now;

  if (!frameDrawn) {
    drawDisplayFrame();
    frameDrawn = true;
  }

  char clientLabel[32];
  if (activeMode == ClientMode::Wifi) {
    snprintf(clientLabel, sizeof(clientLabel), "%.20s", wifiApSsid.length() > 0 ? wifiApSsid.c_str() : "Wi-Fi AP");
  } else {
    snprintf(clientLabel, sizeof(clientLabel), "USB SERIAL");
  }
  drawValueField(62, 34, 108, clientLabel, previousClient, TFT_GREEN);
  copyPreviousValue(previousClient, sizeof(previousClient), clientLabel);

  drawValueField(6, 74, 72, modeName(activeMode), previousActiveMode, TFT_YELLOW);
  copyPreviousValue(previousActiveMode, sizeof(previousActiveMode), modeName(activeMode));

  drawValueField(86, 74, 110, modeName(selectedMode), previousSelectedMode, TFT_ORANGE);
  copyPreviousValue(previousSelectedMode, sizeof(previousSelectedMode), modeName(selectedMode));

  char clientStatus[32];
  if (activeMode == ClientMode::Wifi) {
    snprintf(clientStatus, sizeof(clientStatus), "%s %s",
             WiFi.softAPIP().toString().c_str(),
             (wifiClient && wifiClient.connected()) ? "CLIENT" : "OPEN");
  } else {
    snprintf(clientStatus, sizeof(clientStatus), "SERIAL READY");
  }
  drawValueField(6, 114, 168, clientStatus, previousClientStatus, TFT_CYAN);
  copyPreviousValue(previousClientStatus, sizeof(previousClientStatus), clientStatus);

  char radioStatus[32];
  snprintf(radioStatus, sizeof(radioStatus), "%s %s",
           radioStateName(radioState),
           radioPeerName.length() > 0 ? radioPeerName.c_str() : "");
  drawValueField(6, 154, 180, radioStatus, previousRadioStatus, radioStateColor(radioState));
  copyPreviousValue(previousRadioStatus, sizeof(previousRadioStatus), radioStatus);
  drawRadioTowerIcon(radioState);

  const char *testStatus = radioPairingCode[0] != '\0' ? radioPairingCode : radioTestStateName(radioTestState);
  uint16_t testColor = TFT_DARKGREY;
  if (radioPairingCode[0] != '\0' || radioTestState == RadioTestState::Wait) {
    testColor = TFT_ORANGE;
  } else if (radioTestState == RadioTestState::RxOk) {
    testColor = TFT_GREEN;
  } else if (radioTestState == RadioTestState::NoData || radioTestState == RadioTestState::NoLink) {
    testColor = TFT_RED;
  }
  drawValueField(6, 184, 76, testStatus, previousTestStatus, testColor);
  copyPreviousValue(previousTestStatus, sizeof(previousTestStatus), testStatus);

  char counts[32];
  snprintf(counts, sizeof(counts), "RX %lu TX %lu",
           static_cast<unsigned long>(clientToRadioCount),
           static_cast<unsigned long>(radioToClientCount));
  drawValueField(86, 184, 148, counts, previousCounters, TFT_WHITE);
  copyPreviousValue(previousCounters, sizeof(previousCounters), counts);

  char clientDevice[32];
  if (activeMode == ClientMode::Wifi) {
    snprintf(clientDevice, sizeof(clientDevice), "%s %s",
             (wifiClient && wifiClient.connected()) ? "TCP" : "AP",
             mdnsStarted ? "MDNS" : "OPEN");
    drawValueField(6, 214, 84, clientDevice, previousClientDevice, mdnsStarted ? TFT_GREEN : TFT_ORANGE);
    copyPreviousValue(previousClientDevice, sizeof(previousClientDevice), clientDevice);

    char wifiSecret[32];
    snprintf(wifiSecret, sizeof(wifiSecret), "PW %.20s", SIDEBAND_WIFI_AP_PASSWORD);
    drawValueField(96, 214, 138, wifiSecret, previousWifiSecret, TFT_GREEN);
    copyPreviousValue(previousWifiSecret, sizeof(previousWifiSecret), wifiSecret);
    copyPreviousValue(previousRadioDevice, sizeof(previousRadioDevice), "");
    return;
  }

  snprintf(clientDevice, sizeof(clientDevice), "USB -");
  drawValueField(6, 214, 84, clientDevice, previousClientDevice, TFT_GREEN);
  copyPreviousValue(previousClientDevice, sizeof(previousClientDevice), clientDevice);

  char radioDevice[32];
  snprintf(radioDevice, sizeof(radioDevice), "RAD %.12s",
           radioPeerName.length() > 0 ? radioPeerName.c_str() : radioTargetName.c_str());
  drawValueField(96, 214, 114, radioDevice, previousRadioDevice,
                 radioState == RadioState::Connected ? TFT_GREEN : TFT_DARKGREY);
  copyPreviousValue(previousRadioDevice, sizeof(previousRadioDevice), radioDevice);
  copyPreviousValue(previousWifiSecret, sizeof(previousWifiSecret), "");
}

void setupDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);
  tft.fillScreen(TFT_BLACK);
  displayDirty = true;
}
#else
void renderDisplay() {}
void setupDisplay() {}
#endif

void setupButtons() {
  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
}

void printStatus() {
  if (!serialDiagnosticsEnabled()) {
    return;
  }

  static uint32_t lastStatusMs = 0;
  uint32_t now = millis();
  if (now - lastStatusMs < 5000) {
    return;
  }
  lastStatusMs = now;

  Serial.printf(
      "SIDEBAND status=running mode=%s selected=%s tcp=%s wifi=%s mdns=%s wifi_client=%s radio=%s radio_peer=\"%s\" pair=\"%s\" test=%s test_tx=%lu test_rx=%lu raw_cmds=%lu pair_events=%lu attempts=%lu reconnects=%lu radio_rx_bytes=%lu radio_rx_buf=%u radio_to_client=%lu client_to_radio=%lu kiss_tx=%lu kiss_rx=%lu kiss_malformed=%lu kiss_last=%s\n",
      modeName(activeMode),
      modeName(selectedMode),
      tcpIngressModeName(tcpIngressMode),
      activeMode == ClientMode::Wifi ? WiFi.softAPIP().toString().c_str() : "off",
      mdnsStarted ? "on" : "off",
      activeMode == ClientMode::Wifi && wifiClient && wifiClient.connected() ? "yes" : "no",
      radioStateName(radioState),
      radioPeerName.c_str(),
      radioPairingCode[0] != '\0' ? radioPairingCode : "-",
      radioTestStateName(radioTestState),
      static_cast<unsigned long>(radioTestTxCount),
      static_cast<unsigned long>(radioTestRxCount),
      static_cast<unsigned long>(radioRawCommandCount),
      static_cast<unsigned long>(radioPairingEvents),
      static_cast<unsigned long>(radioConnectAttempts),
      static_cast<unsigned long>(radioReconnects),
      static_cast<unsigned long>(radioRxByteCount),
      static_cast<unsigned>(radioRxBufferLength),
      static_cast<unsigned long>(radioToClientCount),
      static_cast<unsigned long>(clientToRadioCount),
      static_cast<unsigned long>(kissClientFrameCount),
      static_cast<unsigned long>(kissRadioFrameCount),
      static_cast<unsigned long>(kissMalformedCount),
      kissLastError);
}

}  // namespace

void setup() {
  Serial.begin(SIDEBAND_SERIAL_BAUD);
  delay(300);

  loadSettings();
  setupButtons();
  setupDisplay();
  setupRadioTransport();
  setupWifiTransport();
  printRadioConfig();
}

void loop() {
  handleButtons();
  maintainRadioConnection();
  maintainRadioLinkTest();
  maintainWifiClient();
  relayUsbToRadio();
  relayWifiToRadio();
  relayRadioToClient();
  renderDisplay();
  printStatus();
  delay(5);
}

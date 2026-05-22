#include <Arduino.h>
#include <BluetoothSerial.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
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
WebServer webServer(80);
#ifdef SIDEBAND_HAS_TFT
TFT_eSPI tft;
#endif

constexpr uint8_t BUTTON_NEXT_PIN = 0;
constexpr uint8_t BUTTON_SELECT_PIN = 35;
constexpr uint8_t KISS_FEND = 0xC0;
constexpr uint8_t KISS_FESC = 0xDB;
constexpr uint8_t KISS_TFEND = 0xDC;
constexpr uint8_t KISS_TFESC = 0xDD;
constexpr uint8_t KISS_CMD_SET_HARDWARE = 0x06;
constexpr size_t KISS_MAX_FRAME_BYTES = 330;
constexpr size_t RADIO_RX_BUFFER_BYTES = 512;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 45;
constexpr uint32_t DISPLAY_REFRESH_MS = 250;
constexpr uint32_t MODE_PREVIEW_TIMEOUT_MS = 5000;
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
bool webStarted = false;
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

enum class RadioTestState : uint8_t {
  Idle,
  Wait,
  RxOk,
  NoData,
  NoLink,
};

ClientMode selectedMode = ClientMode::Usb;
ClientMode activeMode = ClientMode::Usb;
uint32_t selectedModePreviewStartedMs = 0;
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
uint32_t catHardwareCommandCount = 0;
char catLastHardwareCommand[96] = "-";
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
void saveModeAndRestart(ClientMode mode);
void saveRadioTargetMac(const String &mac);
void saveRadioTargetName(const String &name);
void saveRadioTargetAltName(const String &name);
void clearRadioConfig();
void saveTcpIngressMode(TcpIngressMode mode);
void resetKissStats();
void resetCatStats();
void startRadioLinkTest();

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

void rememberHardwareCommand(const uint8_t *payload, size_t length) {
  catHardwareCommandCount++;
  size_t offset = 0;
  for (size_t i = 0; i < length && offset + 4 < sizeof(catLastHardwareCommand); i++) {
    int written = snprintf(catLastHardwareCommand + offset,
                           sizeof(catLastHardwareCommand) - offset,
                           "%02X", payload[i]);
    if (written <= 0) {
      break;
    }
    offset += static_cast<size_t>(written);
    if (i + 1 < length && offset + 2 < sizeof(catLastHardwareCommand)) {
      catLastHardwareCommand[offset++] = ' ';
      catLastHardwareCommand[offset] = '\0';
    }
  }
  catLastHardwareCommand[sizeof(catLastHardwareCommand) - 1] = '\0';
  displayDirty = true;
  if (serialDiagnosticsEnabled()) {
    Serial.printf("SIDEBAND cat hardware command bytes=%u payload=%s\n",
                  static_cast<unsigned>(length),
                  catLastHardwareCommand);
  }
}

void handleKissFrameFromClient(const uint8_t *payload, size_t length) {
  if (length == 0) {
    return;
  }

  uint8_t command = payload[0] & 0x0f;
  if (command == KISS_CMD_SET_HARDWARE) {
    rememberHardwareCommand(payload, length);
    return;
  }

  writeKissFrameToRadio(payload, length);
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
      handleKissFrameFromClient(ingress.buffer, ingress.length);
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
      return "WiFi";
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

String htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    char ch = value.charAt(i);
    if (ch == '&') {
      escaped += F("&amp;");
    } else if (ch == '<') {
      escaped += F("&lt;");
    } else if (ch == '>') {
      escaped += F("&gt;");
    } else if (ch == '"') {
      escaped += F("&quot;");
    } else {
      escaped += ch;
    }
  }
  return escaped;
}

void appendHtmlHeader(String &html, const char *title) {
  html += F("<!doctype html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  html += F("<meta http-equiv=\"refresh\" content=\"8\">");
  html += F("<title>");
  html += title;
  html += F("</title><style>");
  html += F("body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;background:#0b0d0e;color:#f4f4f4;margin:0;padding:18px;}");
  html += F("h1{font-size:22px;margin:0 0 14px;}h2{font-size:15px;margin:18px 0 8px;color:#9ad;}");
  html += F(".row{display:flex;justify-content:space-between;gap:16px;border-bottom:1px solid #222;padding:7px 0;}");
  html += F(".key{color:#99a}.val{text-align:right}.panel{max-width:560px;margin:auto}.card{border:1px solid #222;border-radius:8px;padding:12px;margin:12px 0;background:#111416;}");
  html += F("input,select,button{font:inherit;border-radius:6px;border:1px solid #333;background:#050607;color:#fff;padding:8px;margin:4px 0;width:100%;box-sizing:border-box;}");
  html += F("button{background:#16446b;border-color:#236399}.danger{background:#582024;border-color:#87323a}.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}");
  html += F("</style></head><body><main class=\"panel\">");
  html += F("<h1>");
  html += title;
  html += F("</h1>");
}

void appendHtmlFooter(String &html) {
  html += F("</main></body></html>");
}

void appendStatusRow(String &html, const char *key, const String &value) {
  html += F("<div class=\"row\"><span class=\"key\">");
  html += key;
  html += F("</span><span class=\"val\">");
  html += htmlEscape(value);
  html += F("</span></div>");
}

void sendWebRedirect(const char *path) {
  webServer.sendHeader("Location", path);
  webServer.send(303, "text/plain", "See Other");
}

void handleWebRoot() {
  String html;
  html.reserve(5200);
  appendHtmlHeader(html, "Sideband");

  html += F("<section class=\"card\"><h2>Status</h2>");
  appendStatusRow(html, "Mode", modeName(activeMode));
  appendStatusRow(html, "Selected", modeName(selectedMode));
  appendStatusRow(html, "TCP", tcpIngressModeName(tcpIngressMode));
  appendStatusRow(html, "IP", activeMode == ClientMode::Wifi ? WiFi.softAPIP().toString() : String("off"));
  appendStatusRow(html, "WiFi clients", String(WiFi.softAPgetStationNum()));
  appendStatusRow(html, "TCP client", (wifiClient && wifiClient.connected()) ? String("connected") : String("open"));
  appendStatusRow(html, "mDNS", mdnsStarted ? String("on") : String("off"));
  appendStatusRow(html, "Radio", radioStateName(radioState));
  appendStatusRow(html, "Peer", radioPeerName.length() > 0 ? radioPeerName : radioTargetName);
  appendStatusRow(html, "KISS TX/RX", String(kissClientFrameCount) + "/" + String(kissRadioFrameCount));
  appendStatusRow(html, "CAT commands", String(catHardwareCommandCount));
  appendStatusRow(html, "CAT last", catLastHardwareCommand);
  html += F("</section>");

  html += F("<section class=\"card\"><h2>Mode</h2><form method=\"post\" action=\"/mode\"><select name=\"mode\">");
  html += F("<option value=\"wifi\"");
  html += activeMode == ClientMode::Wifi ? F(" selected") : F("");
  html += F(">WiFi</option><option value=\"usb\"");
  html += activeMode == ClientMode::Usb ? F(" selected") : F("");
  html += F(">USB-C</option></select><button type=\"submit\">Save mode and restart</button></form></section>");

  html += F("<section class=\"card\"><h2>Radio Target</h2><form method=\"post\" action=\"/radio\">");
  html += F("<label>Name<input name=\"name\" value=\"");
  html += htmlEscape(radioTargetName);
  html += F("\"></label><label>Alt name<input name=\"alt\" value=\"");
  html += htmlEscape(radioTargetAltName);
  html += F("\"></label><label>MAC<input name=\"mac\" placeholder=\"AA:BB:CC:DD:EE:FF\" value=\"");
  html += htmlEscape(radioTargetMac);
  html += F("\"></label><button type=\"submit\">Save radio target</button></form>");
  html += F("<form method=\"post\" action=\"/radio/clear\"><button class=\"danger\" type=\"submit\">Clear radio target</button></form></section>");

  html += F("<section class=\"card\"><h2>TCP Ingress</h2><form method=\"post\" action=\"/tcp\"><select name=\"mode\">");
  html += F("<option value=\"kiss\"");
  html += tcpIngressMode == TcpIngressMode::Kiss ? F(" selected") : F("");
  html += F(">KISS</option><option value=\"raw\"");
  html += tcpIngressMode == TcpIngressMode::Raw ? F(" selected") : F("");
  html += F(">RAW</option></select><button type=\"submit\">Save TCP mode</button></form></section>");

  html += F("<section class=\"card\"><h2>Diagnostics</h2><div class=\"grid\">");
  html += F("<form method=\"post\" action=\"/kiss/reset\"><button type=\"submit\">Reset KISS</button></form>");
  html += F("<form method=\"post\" action=\"/cat/reset\"><button type=\"submit\">Reset CAT</button></form>");
  html += F("<form method=\"post\" action=\"/radio/test\"><button type=\"submit\">Radio test</button></form>");
  html += F("<form method=\"post\" action=\"/restart\"><button class=\"danger\" type=\"submit\">Restart</button></form>");
  html += F("</div></section>");

  appendHtmlFooter(html);
  webServer.send(200, "text/html", html);
}

void handleWebMode() {
  String mode = webServer.arg("mode");
  mode.toLowerCase();
  if (mode == "wifi") {
    saveModeAndRestart(ClientMode::Wifi);
    return;
  }
  if (mode == "usb") {
    saveModeAndRestart(ClientMode::Usb);
    return;
  }
  sendWebRedirect("/");
}

void handleWebRadio() {
  if (webServer.hasArg("name")) {
    saveRadioTargetName(webServer.arg("name"));
  }
  if (webServer.hasArg("alt")) {
    saveRadioTargetAltName(webServer.arg("alt"));
  }
  if (webServer.hasArg("mac")) {
    String mac = webServer.arg("mac");
    mac.trim();
    saveRadioTargetMac(mac);
  }
  sendWebRedirect("/");
}

void handleWebTcp() {
  String mode = webServer.arg("mode");
  mode.toLowerCase();
  if (mode == "raw") {
    saveTcpIngressMode(TcpIngressMode::Raw);
  } else {
    saveTcpIngressMode(TcpIngressMode::Kiss);
  }
  sendWebRedirect("/");
}

void setupWebConfig() {
  if (webStarted || activeMode != ClientMode::Wifi) {
    return;
  }

  webServer.on("/", HTTP_GET, handleWebRoot);
  webServer.on("/favicon.ico", HTTP_GET, []() {
    webServer.send(204, "image/x-icon", "");
  });
  webServer.on("/mode", HTTP_POST, handleWebMode);
  webServer.on("/radio", HTTP_POST, handleWebRadio);
  webServer.on("/radio/clear", HTTP_POST, []() {
    clearRadioConfig();
    sendWebRedirect("/");
  });
  webServer.on("/tcp", HTTP_POST, handleWebTcp);
  webServer.on("/kiss/reset", HTTP_POST, []() {
    resetKissStats();
    sendWebRedirect("/");
  });
  webServer.on("/cat/reset", HTTP_POST, []() {
    resetCatStats();
    sendWebRedirect("/");
  });
  webServer.on("/radio/test", HTTP_POST, []() {
    startRadioLinkTest();
    sendWebRedirect("/");
  });
  webServer.on("/restart", HTTP_POST, []() {
    webServer.send(200, "text/plain", "Restarting");
    delay(250);
    ESP.restart();
  });
  webServer.onNotFound([]() {
    webServer.send(404, "text/plain", "Not found");
  });
  webServer.begin();
  webStarted = true;
}

void maintainWebConfig() {
  if (activeMode != ClientMode::Wifi || !webStarted) {
    return;
  }
  webServer.handleClient();
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
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
  } else {
    mdnsStarted = false;
  }
  setupWebConfig();
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
         input.startsWith("tcp ") || input.startsWith("kiss ") ||
         input.startsWith("cat ");
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
  Serial.println("  cat stats");
  Serial.println("  cat reset");
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

void printCatStats() {
  if (!serialDiagnosticsEnabled()) {
    return;
  }

  Serial.printf("SIDEBAND cat hardware_commands=%lu last=\"%s\"\n",
                static_cast<unsigned long>(catHardwareCommandCount),
                catLastHardwareCommand);
}

void resetCatStats() {
  catHardwareCommandCount = 0;
  strncpy(catLastHardwareCommand, "-", sizeof(catLastHardwareCommand));
  catLastHardwareCommand[sizeof(catLastHardwareCommand) - 1] = '\0';
  displayDirty = true;
  logLine("SIDEBAND cat stats reset");
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

  if (command == "cat stats") {
    printCatStats();
    return;
  }

  if (command == "cat reset") {
    resetCatStats();
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
    selectedModePreviewStartedMs = millis();
    displayDirty = true;
  }

  if (buttonPressed(BUTTON_SELECT_PIN, selectStable, selectChangedMs)) {
    if (selectedMode != activeMode) {
      saveModeAndRestart(selectedMode);
      return;
    }
    startRadioLinkTest();
  }

  if (selectedMode != activeMode && selectedModePreviewStartedMs != 0 &&
      millis() - selectedModePreviewStartedMs > MODE_PREVIEW_TIMEOUT_MS) {
    selectedMode = activeMode;
    selectedModePreviewStartedMs = 0;
    displayDirty = true;
  }
}

#ifdef SIDEBAND_HAS_TFT
bool radioConnectedForDisplay() {
  return radioState == RadioState::Connected && radioSerial.connected();
}

bool clientConnectedForDisplay() {
  if (activeMode == ClientMode::Usb) {
    return true;
  }
  return WiFi.softAPgetStationNum() > 0 || (wifiClient && wifiClient.connected());
}

uint16_t towerStatusColor(uint32_t now) {
  bool radioConnected = radioConnectedForDisplay();
  bool clientConnected = clientConnectedForDisplay();
  if (radioConnected && clientConnected) {
    return TFT_GREEN;
  }
  if (radioConnected || clientConnected) {
    return TFT_YELLOW;
  }
  return ((now / 500) % 2) == 0 ? TFT_RED : TFT_BLACK;
}

void drawTinyRadioTower(uint16_t foreground, uint16_t background) {
  static uint16_t previousForeground = 0xffff;
  static bool previousRadioConnected = false;
  bool radioConnected = radioConnectedForDisplay();
  if (foreground == previousForeground && radioConnected == previousRadioConnected) {
    return;
  }

  constexpr int16_t originX = 204;
  constexpr int16_t originY = 6;
  constexpr int16_t towerX = originX + 15;
  constexpr int16_t towerTop = originY + 6;
  constexpr int16_t towerBottom = originY + 32;
  tft.fillRect(originX, originY, 34, 36, background);
  tft.drawLine(towerX, towerTop, towerX, towerBottom, foreground);
  tft.drawLine(towerX, towerTop + 5, towerX - 10, towerBottom, foreground);
  tft.drawLine(towerX, towerTop + 5, towerX + 10, towerBottom, foreground);
  tft.drawLine(towerX - 7, towerBottom - 7, towerX + 7, towerBottom - 7, foreground);
  tft.drawLine(towerX - 5, towerBottom, towerX + 5, towerBottom, foreground);
  tft.fillCircle(towerX, towerTop, radioConnected ? 4 : 2, foreground);
  previousForeground = foreground;
  previousRadioConnected = radioConnected;
}

void drawLineText(int16_t x, int16_t y, const char *label, const char *value,
                  uint16_t foreground, uint16_t background, char *previous,
                  size_t previousSize) {
  char line[48];
  snprintf(line, sizeof(line), "%s:%s", label, value);
  if (strncmp(line, previous, previousSize) == 0) {
    return;
  }

  tft.fillRect(x, y, 196, 18, background);
  tft.setTextColor(foreground, background);
  tft.drawString(label, x, y, 2);
  tft.drawString(value, x + 52, y, 2);
  strncpy(previous, line, previousSize - 1);
  previous[previousSize - 1] = '\0';
}

void renderDisplay() {
  static uint32_t lastDrawMs = 0;
  static bool screenInitialized = false;
  static ClientMode previousLayoutMode = ClientMode::Usb;
  static char previousModeLine[48] = "";
  static char previousSelectLine[48] = "";
  static char previousLine1[48] = "";
  static char previousLine2[48] = "";
  static char previousLine3[48] = "";
  static char previousLine4[48] = "";
  uint32_t now = millis();
  if (!displayDirty && now - lastDrawMs < DISPLAY_REFRESH_MS) {
    return;
  }
  displayDirty = false;
  lastDrawMs = now;

  uint16_t background = TFT_BLACK;
  uint16_t foreground = TFT_WHITE;
  uint16_t towerColor = towerStatusColor(now);
  bool previewMode = selectedMode != activeMode;
  const char *displayMode = previewMode ? modeName(selectedMode) : modeName(activeMode);

  tft.setTextDatum(TL_DATUM);
  if (!screenInitialized || previousLayoutMode != activeMode) {
    tft.fillScreen(background);
    screenInitialized = true;
    previousLayoutMode = activeMode;
    previousModeLine[0] = '\0';
    previousSelectLine[0] = '\0';
    previousLine1[0] = '\0';
    previousLine2[0] = '\0';
    previousLine3[0] = '\0';
    previousLine4[0] = '\0';
  }
  drawTinyRadioTower(towerColor, background);

  char modeLine[32];
  snprintf(modeLine, sizeof(modeLine), "%s", displayMode);
  drawLineText(6, 8, "MODE", modeLine, foreground, background,
               previousModeLine, sizeof(previousModeLine));
  char selectLine[16];
  snprintf(selectLine, sizeof(selectLine), "%s", previewMode ? "SELECT" : "");
  if (strncmp(selectLine, previousSelectLine, sizeof(previousSelectLine)) != 0) {
    tft.fillRect(150, 8, 64, 18, background);
    strncpy(previousSelectLine, selectLine, sizeof(previousSelectLine) - 1);
    previousSelectLine[sizeof(previousSelectLine) - 1] = '\0';
  }
  if (previewMode) {
    tft.setTextColor(TFT_YELLOW, background);
    tft.drawString("SELECT", 150, 8, 2);
    tft.setTextColor(foreground, background);
  }

  if (activeMode == ClientMode::Wifi) {
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%.18s", wifiApSsid.length() > 0 ? wifiApSsid.c_str() : "Wi-Fi AP");
    drawLineText(6, 32, "SSID", ssid, foreground, background,
                 previousLine1, sizeof(previousLine1));
    drawLineText(6, 52, "PASS", SIDEBAND_WIFI_AP_PASSWORD, foreground, background,
                 previousLine2, sizeof(previousLine2));

    char link[32];
    snprintf(link, sizeof(link), "WiFi %u RADIO %.8s",
             static_cast<unsigned int>(WiFi.softAPgetStationNum()),
             radioPeerName.length() > 0 ? radioPeerName.c_str() : radioTargetName.c_str());
    drawLineText(6, 72, "LINK", link, foreground, background,
                 previousLine3, sizeof(previousLine3));

    char ip[32];
    snprintf(ip, sizeof(ip), "%s",
             WiFi.softAPIP().toString().c_str());
    drawLineText(6, 92, "IP", ip, foreground, background,
                 previousLine4, sizeof(previousLine4));
  } else {
    drawLineText(6, 36, "USB", "SERIAL", foreground, background,
                 previousLine1, sizeof(previousLine1));
    char link[32];
    snprintf(link, sizeof(link), "USB RADIO %.8s",
             radioPeerName.length() > 0 ? radioPeerName.c_str() : radioTargetName.c_str());
    drawLineText(6, 60, "LINK", link, foreground, background,
                 previousLine2, sizeof(previousLine2));
  }
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
      "SIDEBAND status=running mode=%s selected=%s tcp=%s wifi=%s mdns=%s wifi_client=%s radio=%s radio_peer=\"%s\" pair=\"%s\" test=%s test_tx=%lu test_rx=%lu raw_cmds=%lu cat_hw=%lu pair_events=%lu attempts=%lu reconnects=%lu radio_rx_bytes=%lu radio_rx_buf=%u radio_to_client=%lu client_to_radio=%lu kiss_tx=%lu kiss_rx=%lu kiss_malformed=%lu kiss_last=%s cat_last=\"%s\"\n",
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
      static_cast<unsigned long>(catHardwareCommandCount),
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
      kissLastError,
      catLastHardwareCommand);
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
  maintainWebConfig();
  relayUsbToRadio();
  relayWifiToRadio();
  relayRadioToClient();
  renderDisplay();
  printStatus();
  delay(5);
}

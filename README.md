# Sideband

Sideband is a portable RF bridge for Kenwood TH-D75 class radios. An original
ESP32 board connects to the radio over Bluetooth Classic SPP and exposes a BLE
KISS TNC endpoint for iPhone APRS apps. Wi-Fi TCP is available as a fallback
client transport.

The project favors small, field-readable tools: stable transports, visible link
state, conservative packet handling, and diagnostics that do not leak payloads
or private operator identifiers by default.

## Flash

**The fastest way to get started:** open the web flasher in Chrome or Edge,
plug in your board, and click Flash.

> **[jfergs.github.io/Sideband](https://jfergs.github.io/Sideband/)**

The flasher includes a pre-built image for the confirmed hardware and a custom
binary upload path for other original ESP32 boards.

To flash from the command line instead, see
[Developer Setup](docs/DEVELOPER_SETUP.md).

## Confirmed Hardware

**TTGO T-Display V1.1** — original ESP32, 4 MB flash, 1.14 inch ST7789
135×240 TFT. Silkscreen on PCB reverse reads `T-Display V1.1`.

Other original ESP32 boards (WROOM, WROVER, DevKitC) work but have no display.
ESP32-S3, C3, and C6 lack Bluetooth Classic and cannot connect to TH-D75 SPP.

## Architecture

```text
TH-D75 class radio
  <-> Bluetooth Classic SPP
  <-> Sideband ESP32 bridge (original ESP32, BTDM dual-mode)
  <-> BLE KISS TNC (primary) or Wi-Fi TCP (fallback)
  <-> iPhone APRS app
      APRS.fi / Packet Commander / RadioMail / PocketPacket
```

The bridge runs the original ESP32's dual Bluetooth stack (BTDM): Classic BT
for the radio link and BLE for the phone simultaneously. BLE advertises the
standard BLE KISS TNC service (`00000001-ba2a-46c9-ae49-01b0961f68bb`) — any
iOS APRS app that supports the BLE KISS TNC specification connects directly
without pairing or a PIN.

Wi-Fi AP and Wi-Fi STA are available as fallback client transports. BLE and
Wi-Fi are mutually exclusive; a button press or serial command switches modes.

## Project Scope

- TH-D75 class Bluetooth Classic SPP radio bridge.
- BLE KISS TNC (KTS profile) and Wi-Fi TCP client transports.
- KISS framing, transparent packet relay, and packet counters.
- Low-refresh instrumentation TFT UI for display-equipped boards.
- Web flasher for one-click firmware install (Chrome/Edge, WebSerial).
- Future: MQTT telemetry, Reticulum, Meshtastic, and TAK integration.

## Layout

```text
BACKLOG.md
docs/
  DEVELOPER_SETUP.md
  FIELD_VALIDATION.md
firmware/
  sideband-bridge/
    platformio.ini
    include/
      sideband_config.example.h
    src/
      main.cpp
flasher/
  index.html
  manifest.ttgo-t-display-ble.json
hardware/
  README.md
```

## Documentation

- [Developer Setup](docs/DEVELOPER_SETUP.md) — build, flash, radio MAC, iOS app, mode switching
- [Field Validation](docs/FIELD_VALIDATION.md) — BLE and Wi-Fi validation checklists
- [Hardware](hardware/README.md) — confirmed boards and compatibility matrix

## Status

**v0.1.0-beta.1** — BLE KISS TNC advertising, Bluetooth Classic SPP radio link,
KISS relay in both directions, TFT instrumentation, NVS config persistence, mode
switching. End-to-end BLE validation with iOS APRS apps is the active test
priority (SB-043). See [BACKLOG.md](BACKLOG.md) for the full item list.

## Privacy

No payload bytes are logged by default. Callsigns, pairing records, and field
logs must not be committed. Local credentials go in `include/secrets.h`, which
is gitignored. See [Developer Setup](docs/DEVELOPER_SETUP.md).

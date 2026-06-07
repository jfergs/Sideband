# Sideband

Sideband is a portable RF bridge and packet middleware project for Kenwood
TH-D75 class radios. The first target is an original ESP32 bridge that connects
to the radio over Bluetooth Classic SPP and exposes USB-C serial or Wi-Fi TCP
transport for iPhone and mobile packet applications.

The project favors small, field-readable tools: stable transports, visible link
state, conservative packet handling, and diagnostics that do not leak payloads
or private operator identifiers by default.

## Project Scope

- TH-D75 class Bluetooth Classic SPP radio bridge.
- USB-C serial and Wi-Fi TCP mobile transport.
- KISS framing, transparent packet relay, and packet counters.
- Low-refresh instrumentation UI for supported ESP32 display boards.
- Hardware compatibility notes for Bluetooth Classic capable ESP32 boards.
- Future MQTT telemetry, Reticulum, Meshtastic, and TAK integration research.

## Architecture

```text
TH-D75 class radio
  <-> Bluetooth Classic SPP
  <-> Sideband ESP32 bridge (original ESP32, BTDM dual-mode)
  <-> BLE KISS TNC (primary) or Wi-Fi TCP (fallback)
  <-> iPhone or mobile packet app
      (APRS.fi / Packet Commander / RadioMail / PocketPacket)
```

The bridge uses the original ESP32's dual Bluetooth stack (BTDM) to run
Bluetooth Classic SPP for the radio and BLE for the phone simultaneously.
BLE is the primary client transport. The bridge advertises the standard
BLE KISS TNC service (`00000001-ba2a-46c9-ae49-01b0961f68bb`) so any iOS
APRS app that supports the BLE KISS TNC specification connects directly.

Wi-Fi mode (AP or STA) is the fallback client transport when BLE is not
suitable. BLE and Wi-Fi are mutually exclusive; only one is active at a time.

ESP32-S3, ESP32-C3, and ESP32-C6 do not support Bluetooth Classic and are
not suitable for the primary bridge.

## Hardware Direction

First boards to evaluate:

- TTGO T-Display V1.1 original ESP32 variant.
- ESP32 DevKitC / ESP32-WROOM boards.
- ESP32-WROVER boards when extra buffering or PSRAM is useful.

Avoid S3-branded TTGO/T-Display variants for the primary bridge unless the radio
transport changes, because those boards cannot connect to TH-D75 SPP.

## Layout

```text
BACKLOG.md
CONTRIBUTING.md
CODEOWNERS
LICENSE
docs/
  ARCHITECTURE.md
  FIELD_VALIDATION.md
enclosure/
experiments/
firmware/
  sideband-bridge/
hardware/
protocol/
references/
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Developer setup](docs/DEVELOPER_SETUP.md)
- [Field validation](docs/FIELD_VALIDATION.md)

## Development

Install PlatformIO, then build and flash the primary target:

```bash
cd firmware/sideband-bridge
pio run -e ttgo-t-display-ble --target upload
```

Other targets: `ttgo-t-display` (Wi-Fi only, no BLE), `esp32dev-ble` (BLE,
no display), `esp32dev` (basic, no BLE or display).

Local credentials, pairing records, callsigns, and field logs should stay out of
the repo. Copy `firmware/sideband-bridge/include/sideband_config.example.h`
to `firmware/sideband-bridge/include/secrets.h` for a local config that is
never committed.

See [Developer Setup](docs/DEVELOPER_SETUP.md) for the full first-flash
walkthrough including radio MAC configuration and iOS app connection steps.

## Status

Active development. Current firmware: BLE KISS TNC advertising (`00000001-ba2a-46c9-ae49-01b0961f68bb`),
Bluetooth Classic SPP radio link to TH-D75, KISS relay in both directions,
TFT instrumentation display, NVS config persistence, mode switching. End-to-end
BLE validation with iOS APRS apps is the active test priority (SB-043).

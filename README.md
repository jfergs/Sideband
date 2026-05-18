# Sideband

Sideband is a portable RF bridge and packet middleware project for Kenwood
TH-D75 class radios. The first target is an original ESP32 bridge that connects
to the radio over Bluetooth Classic SPP and exposes a BLE UART service for
iPhone and mobile packet applications.

The project favors small, field-readable tools: stable transports, visible link
state, conservative packet handling, and diagnostics that do not leak payloads
or private operator identifiers by default.

## Project Scope

- TH-D75 class Bluetooth Classic SPP radio bridge.
- BLE UART / Nordic UART Service compatible mobile transport.
- KISS framing, transparent packet relay, and packet counters.
- Low-refresh instrumentation UI for supported ESP32 display boards.
- Hardware compatibility notes for Bluetooth Classic capable ESP32 boards.
- Future Wi-Fi TCP KISS, MQTT telemetry, Reticulum, Meshtastic, and TAK
  integration research.

## Architecture

```text
TH-D75 class radio
  <-> Bluetooth Classic SPP
  <-> Sideband ESP32 bridge
  <-> BLE UART / Nordic UART Service
  <-> iPhone or mobile packet app
```

The primary bridge requires Bluetooth Classic. ESP32-S3, ESP32-C3, and ESP32-C6
support BLE but not Bluetooth Classic, so they are not suitable for the first
radio bridge. They may still be useful later as display, companion, or telemetry
nodes.

## Hardware Direction

First boards to evaluate:

- TTGO T-Display v1.3 original ESP32 variant.
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
enclosure/
experiments/
firmware/
  sideband-bridge/
hardware/
protocol/
references/
```

## Development

Install PlatformIO, then build the initial bridge scaffold:

```bash
cd firmware/sideband-bridge
pio run -e esp32dev
```

For TTGO T-Display v1.3 original ESP32 boards:

```bash
pio run -e ttgo-t-display
```

Local credentials, pairing records, callsigns, and field logs should stay out of
the repo. If local experiments need private values, copy
`firmware/sideband-bridge/include/sideband_config.example.h` to
`firmware/sideband-bridge/include/secrets.h`.

## Status

Early implementation scaffold. The current firmware advertises a BLE UART
service and initializes Bluetooth Classic SPP, but radio pairing, KISS framing,
persistent storage, TFT UI, and end-to-end TH-D75 validation are still backlog
items.

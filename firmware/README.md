# Firmware

Firmware for the Sideband bridge lives here.

The primary target is a Bluetooth Classic capable ESP32 board that can connect
to a Kenwood TH-D75 class radio over SPP while exposing a BLE UART service for
mobile clients.

## Current Scaffold

- `sideband-bridge/`: initial PlatformIO Arduino scaffold.
- Supported first-pass chip family: original ESP32 with Bluetooth Classic.
- Not primary bridge targets: ESP32-S3, ESP32-C3, ESP32-C6.

## Safety Notes

- The bridge should only relay packets between explicitly paired transports.
- Packet payload logging must be opt-in.
- Diagnostics should prefer counters, state, and error codes over raw payloads.

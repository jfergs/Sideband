# Sideband Architecture

Sideband is a small packet bridge between radio equipment and mobile operator
tools. The first implementation target is a Kenwood TH-D75 class radio paired
to an original ESP32 over Bluetooth Classic SPP, with the ESP32 exposing a BLE
UART service to an iPhone or tablet.

## System Roles

```text
Radio              Sideband bridge                 Mobile client
-----              ---------------                 -------------
TH-D75 SPP   <->   ESP32 Bluetooth Classic   <->   BLE UART app
KISS frames        KISS relay + counters           Packet workflow
```

- Radio: owns RF transmission, receive, and TNC behavior.
- Sideband bridge: owns transport pairing, KISS-safe relay, connection state,
  packet counters, and operator-visible diagnostics.
- Mobile client: owns message composition, packet app workflows, and user data.
- Future services: may consume telemetry or expose TCP KISS, but should not be
  required for the offline bridge path.

## Transport Boundaries

### Bluetooth Classic SPP

The primary radio-facing transport is Bluetooth Classic SPP. This requires an
original ESP32 family device such as ESP32-WROOM, ESP32-WROVER, or ESP32-D0WD.
ESP32-S3, ESP32-C3, and ESP32-C6 do not support Bluetooth Classic and are not
primary bridge targets.

### BLE UART

The mobile-facing transport starts with Nordic UART Service compatible BLE
characteristics:

- Service UUID: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- RX characteristic: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- TX characteristic: `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

BLE MTU and fragmentation rules need validation before large KISS frames are
considered reliable.

### KISS Relay

The relay should preserve binary packet data and treat KISS framing as a
protocol boundary:

- `FEND`: `0xC0`
- `FESC`: `0xDB`
- `TFEND`: `0xDC`
- `TFESC`: `0xDD`

Malformed frames should increment diagnostics and be dropped without wedging
the bridge. Payload hex dumps should be opt-in only.

## Data Model

Initial diagnostics should stay small:

- Connection state: idle, pairing, connected, reconnecting, error.
- Counters: radio to BLE, BLE to radio, malformed frames, reconnects.
- Timing: last packet time, uptime, reconnect age.
- Hardware: board profile, battery hook, display profile.

Sideband should avoid storing message content unless an operator explicitly
enables diagnostic capture for a controlled test.

## Privacy Defaults

- Do not log packet payloads by default.
- Do not commit callsigns, GPS tracks, pairing data, packet captures, or field
  logs.
- Redact device names and operator identifiers in shared screenshots or reports.
- Keep default docs and examples synthetic.

## Future Expansion

- Wi-Fi TCP KISS for laptops and local tools.
- Web configuration portal for pairing and status.
- MQTT telemetry for bridge health and counters.
- Reticulum, Meshtastic, and TAK research as gateway experiments.

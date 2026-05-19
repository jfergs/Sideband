# Sideband Architecture

Sideband is a small packet bridge between radio equipment and mobile operator
tools. The first implementation target is a Kenwood TH-D75 class radio paired
to an original ESP32 over Bluetooth Classic SPP, with the ESP32 exposing USB-C
serial or Wi-Fi TCP service to an iPhone or tablet.

## System Roles

```text
Radio              Sideband bridge                 Mobile client
-----              ---------------                 -------------
TH-D75 SPP   <->   ESP32 Bluetooth Classic   <->   USB/Wi-Fi app
KISS frames        KISS relay + counters           USB/Wi-Fi packet workflow
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

### USB-C Serial

USB-C serial is the default mobile-facing transport. While USB mode is active,
the serial path is treated as packet data and periodic diagnostics are
suppressed to avoid corrupting KISS traffic.

### Wi-Fi TCP KISS

Wi-Fi mode exposes a local access point and TCP relay for mobile clients. The
initial firmware uses AP mode and a configurable TCP port; station mode and
multi-client ownership rules remain future work.

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
- Counters: radio to client, client to radio, malformed frames, reconnects.
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

- Web configuration portal for pairing and status.
- MQTT telemetry for bridge health and counters.
- Reticulum, Meshtastic, and TAK research as gateway experiments.

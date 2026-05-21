# Protocol

Sideband starts with transparent KISS packet relay between a radio-facing
Bluetooth Classic SPP transport and mobile-facing USB-C serial or Wi-Fi TCP
transports.

## Initial Flow

```text
TH-D75 class radio
  <-> Bluetooth Classic SPP
  <-> Sideband ESP32 bridge
  <-> USB-C serial / Wi-Fi TCP KISS
  <-> iPhone or mobile packet app
```

## KISS Framing Scope

- Preserve binary payloads.
- Handle `FEND`, `FESC`, `TFEND`, and `TFESC`.
- Reject malformed frames without wedging the relay.
- Track counters and errors without logging payload bytes by default.

## Privacy Defaults

- Packet payload dumps are opt-in diagnostics.
- Pairing data and station identifiers should not be committed.
- Field logs should redact callsigns, device names, and exact locations unless
  explicitly collected for a controlled test.

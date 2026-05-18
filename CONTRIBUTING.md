# Contributing

Sideband is in early platform bring-up. Keep changes focused, documented, and
safe for licensed amateur-radio and field-operations use.

## Ground Rules

- Do not add code that transmits outside configured radio or network channels.
- Do not log packet payloads by default.
- Do not commit local credentials, pairing data, callsigns, GPS tracks, packet
  captures, or field logs.
- Keep hardware assumptions explicit. ESP32-S3, ESP32-C3, and ESP32-C6 do not
  support Bluetooth Classic and are not primary bridge targets.
- Prefer small pull requests tied to one `SB-XXX` backlog item.

## Local Workflow

1. Read `BACKLOG.md` and the relevant directory README.
2. Build firmware changes with PlatformIO before opening a pull request.
3. Update docs when behavior, supported boards, wiring, or operator workflow
   changes.
4. Include test notes for radio, BLE, packet framing, and reconnect behavior
   where applicable.

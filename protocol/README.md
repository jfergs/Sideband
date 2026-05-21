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

Bench exerciser:

```bash
python3 scripts/kiss-exerciser.py --payload-hex 00
python3 scripts/kiss-exerciser.py --payload-hex '00 c0 db'
python3 scripts/kiss-exerciser.py --malformed
python3 scripts/kiss-exerciser.py --raw-text TEST --cr
```

## Bench Validation Notes

- TH-D75 APRS mode with `Menu 590 = Raw Packets` and `Menu 982 = Bluetooth`
  emits raw APRS text over the Bluetooth SPP link.
- TH-D75 KISS12 mode with KISS output on Bluetooth emits KISS-framed AX.25
  packets when the radio TNC produces packet output.
- Sideband bench validation has confirmed Wi-Fi TCP KISS frames reach the radio,
  malformed KISS escapes are counted without wedging the relay, escaped FEND and
  FESC payloads pass, and radio-originated KISS frames forward to a connected
  TCP client.

## Privacy Defaults

- Packet payload dumps are opt-in diagnostics.
- Pairing data and station identifiers should not be committed.
- Field logs should redact callsigns, device names, and exact locations unless
  explicitly collected for a controlled test.

# Sideband Field Validation

Use this checklist after flashing a Sideband bridge for TH-D75 field tests.
Do not commit packet captures, callsigns, pairing records, or location-bearing
field logs.

## Baseline

Flash and monitor:

```bash
cd firmware/sideband-bridge
pio run -e ttgo-t-display -t upload --upload-port /dev/cu.usbserial-56230391271
pio device monitor --port /dev/cu.usbserial-56230391271 --baud 115200 --echo
```

Expected:

- `mode=Wi-Fi`
- `wifi=192.168.4.1`
- `mdns=on`
- `radio=LINKED`
- `radio_peer="TH-D75"`

## APRS Monitor Mode

Radio:

- TH-D75 in normal APRS mode.
- `Menu 590 = Raw Packets`.
- `Menu 982 = Bluetooth`.

Sideband:

```text
tcp raw
```

Send or receive an APRS message on the radio.

Expected monitor counters:

- `radio_rx_bytes` increments.
- `radio_rx_buf` increments.
- `kiss_rx` remains unchanged because APRS PC output is raw text, not KISS.

Inspect the buffer:

```text
radio dump
```

Replay to a connected TCP client:

```bash
nc 192.168.4.1 8001 | hexdump -C
```

Then in the Sideband monitor:

```text
radio replay
```

Expected:

- `radio_to_client` increments.
- The TCP client receives raw APRS text bytes.

## KISS12 App Mode

Radio:

- TH-D75 in `KISS12`.
- KISS data interface set to Bluetooth.
- Data speed set to 1200 bps.
- Data band set to the active packet band.

Sideband:

```text
kiss reset
tcp kiss
```

Expected before app traffic:

- `tcp=KISS`
- `kiss_tx=0`
- `kiss_rx=0`
- `kiss_malformed=0`

Connect an iOS APRS app to the Sideband TNC by discovery or manually:

```text
192.168.4.1:8001
```

Expected after sending and receiving packets:

- `wifi_client=yes`
- `client_to_radio` increments.
- `kiss_tx` increments.
- `radio_rx_bytes` increments when the TH-D75 emits packets.
- `kiss_rx` increments for radio-originated KISS frames.
- `radio_to_client` increments when a TCP client is connected.
- iOS app decodes and displays APRS packets.

## KISS Bench Exerciser

Run from the repository root while connected to the Sideband Wi-Fi AP:

```bash
python3 scripts/kiss-exerciser.py --payload-hex 00
python3 scripts/kiss-exerciser.py --payload-hex '00 c0 db'
python3 scripts/kiss-exerciser.py --malformed
```

Expected:

- Valid frames increment `kiss_tx`.
- Escaped FEND/FESC payloads pass without incrementing `kiss_malformed`.
- The malformed frame increments `kiss_malformed`.
- `kiss_last=bad_escape` after the malformed test.

Raw TCP pass-through test:

```text
tcp raw
```

```bash
python3 scripts/kiss-exerciser.py --raw-text TEST --cr
```

Expected:

- `client_to_radio` increments.
- `kiss_malformed` does not increment.

## Known Results

- APRS monitor mode has been validated with TH-D75 raw APRS output over
  Bluetooth SPP into Sideband and out through the RX buffer/replay path.
- KISS12 mode has been validated end-to-end with an iOS APRS app using Sideband
  as the TCP KISS TNC.

# Developer Setup

Sideband firmware uses PlatformIO with the Arduino framework for the first ESP32
bridge scaffold.

## Requirements

- Python 3.10 or newer.
- PlatformIO Core.
- USB data cable for the ESP32 board.
- Original ESP32 board with Bluetooth Classic support.

## Install PlatformIO

```bash
python3 -m pip install --user platformio
```

If your shell cannot find `pio`, add the Python user scripts directory to your
`PATH`.

## Build

```bash
cd firmware/sideband-bridge
pio run -e esp32dev
```

For a TTGO T-Display v1.3 original ESP32 board:

```bash
pio run -e ttgo-t-display
```

## Flash And Monitor

```bash
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

## Local Configuration

Private local values belong in `include/secrets.h`, which is ignored by git.

```bash
cp include/sideband_config.example.h include/secrets.h
```

Do not commit pairing records, callsigns, packet captures, GPS tracks, or field
logs.

## First Bring-Up Checks

- Serial output shows `SIDEBAND boot`.
- BLE advertises as `Sideband BLE`.
- The board remains stable for 10 minutes while advertising.
- Bluetooth Classic SPP examples compile for the selected board.
- No packet payloads are logged by default.

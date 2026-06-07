# Developer Setup

Sideband firmware uses PlatformIO with the Arduino framework targeting
original ESP32 boards with Bluetooth Classic support.

## Web Flasher

For non-technical users: visit the GitHub Pages flasher and click
**Flash Sideband Bridge** — no software installation required. Requires
Chrome or Edge (WebSerial is not supported in Safari or Firefox).

To enable the flasher page on your fork: go to repo **Settings → Pages →
Source** and select **GitHub Actions**. The flasher deploys automatically
when you push a `vX.Y.Z` tag.

The flasher also includes a **Flash a custom binary** section for boards
not on the confirmed list. See the [Build for an unlisted board](#build-for-an-unlisted-board)
section below for the PlatformIO + esptool merge steps that produce the
required merged `.bin` file.

## Quick Start

No file editing required for a first flash. The defaults work out of the box.

```bash
# 1. Install PlatformIO
python3 -m pip install --user platformio

# 2. Flash
cd firmware/sideband-bridge
pio run -e ttgo-t-display-ble --target upload
```

The device boots in BLE mode and scans for a TH-D75 by name. On your iPhone,
open APRS.fi → TNC Settings → Bluetooth TNC → select `Sideband Bridge`.

To set a persistent radio MAC (faster connect, skips name scan):

```bash
pio device monitor --baud 115200
```

Then type: `radio mac AA:BB:CC:DD:EE:FF` (find the address on the TH-D75 at
Menu 918). The device saves the MAC to NVS and restarts.

The sections below cover hardware identification, all build targets, optional
`secrets.h` config, iOS app connection, and mode switching.

## Hardware

**Confirmed working:** TTGO T-Display V1.1 with the **original ESP32** chip
(not S3). The silkscreen on the back of the PCB reads `T-Display V1.1`.

The S3 variant lacks Bluetooth Classic and cannot connect to TH-D75 class
radios over SPP.

Visual identification: check the chip marking on the metal-shielded module.
Original ESP32 modules are marked `ESP32-D0WD` or `ESP32-D0WDQ6`. S3 modules
are marked `ESP32-S3`. The silkscreen on the PCB reverse is the most reliable
check — V1.1 boards ship with original ESP32.

Other original ESP32 boards (WROOM, WROVER, DevKitC) also work but do not
have the onboard TFT display.

**You also need:** a USB data cable and a TH-D75 class radio with Bluetooth
enabled.

## Requirements

- Python 3.10 or newer
- PlatformIO Core (`python3 -m pip install --user platformio`)
- USB data cable for flashing

## Build Targets

| Target | Board | Display | BLE | Use for |
|---|---|---|---|---|
| `ttgo-t-display-ble` | TTGO T-Display V1.1 | Yes | Yes | Primary field target |
| `ttgo-t-display` | TTGO T-Display V1.1 | Yes | No | WiFi-only builds |
| `esp32dev-ble` | ESP32-WROOM | No | Yes | BLE without display |
| `esp32dev` | ESP32-WROOM | No | No | Basic WiFi-only |

## Local Configuration

Private values belong in `include/secrets.h`, which is ignored by git:

```bash
cp include/sideband_config.example.h include/secrets.h
```

The defaults in `sideband_config.example.h` work as-is for a first flash.
Edit `secrets.h` to pre-configure your radio MAC, WiFi credentials, or a
custom device name before flashing.

Do not commit `secrets.h`, callsigns, pairing records, or field logs.

## Flash

```bash
cd firmware/sideband-bridge
pio run -e ttgo-t-display-ble --target upload
```

The device resets automatically after flashing.

## First Boot

The device starts in **BLE mode** and immediately advertises as
`Sideband Bridge` using the standard BLE KISS TNC service profile
(`00000001-ba2a-46c9-ae49-01b0961f68bb`).

Simultaneously, it attempts to connect to the radio over Bluetooth Classic
SPP. If no radio MAC is configured it scans by name (`TH-D75` / `TH-D74`).

## Configure the Radio MAC

The radio MAC only needs to be set once and is stored in NVS (survives
reflashing as long as NVS is not erased).

Connect a serial terminal at 115200 baud and run:

```
radio mac AA:BB:CC:DD:EE:FF
```

The device restarts and connects directly by MAC without scanning.

To find the TH-D75 MAC: on the radio go to **Menu 918** (Bluetooth Device
Address), or check the label inside the battery compartment.

Other useful serial commands:

```
status                      — current link state and counters
radio show                  — configured radio name and MAC
radio clear                 — forget configured MAC, revert to name scan
mode ble                    — switch to BLE client mode
mode wifi-ap                — switch to Wi-Fi AP hotspot mode
mode wifi-sta               — switch to Wi-Fi STA mode
wifi ssid <name>            — set Wi-Fi STA credentials
wifi pass <password>
```

## iOS App Connection

Any iOS app that supports the BLE KISS TNC specification works:
**APRS.fi**, **Packet Commander**, **RadioMail**, **PocketPacket**.

Steps:
1. Open the app → TNC or radio settings → Bluetooth TNC
2. Scan for devices — `Sideband Bridge` should appear
3. Connect — the bridge relays KISS frames between the app and the radio

No pairing or PIN is required. Connection parameter negotiation is handled
automatically.

## Mode Switching

The left button (GPIO0) cycles through modes: **BLE → WiFi-AP → WiFi-STA**

The right button (GPIO35) runs a radio link test.

The active mode is saved to NVS and restored on restart.

## Serial Monitor

```bash
pio device monitor --baud 115200
```

The status line prints every 5 seconds and shows link state, packet
counters, and heap. Diagnostics are suppressed in USB mode (pipe mode).

## First Bring-Up Checks

- TFT shows `BLE / ADVERTISING` and the radio connection state
- Serial shows `SIDEBAND ble advertising name="Sideband Bridge" mac=XX:XX:XX:XX:XX:XX`
- `SIDEBAND ble gap adv_start status=0` confirms advertising started
- iOS app finds `Sideband Bridge` in Bluetooth TNC scan
- `kiss_rx` counter increments when the iOS app sends a frame
- `radio_to_client` counter increments when the radio sends a frame to the app

## Build for an Unlisted Board

Any original ESP32 board with Bluetooth Classic should work. ESP32-S3, C3, and
C6 cannot connect to TH-D75 class radios over SPP and are not suitable.

Add a new target to `platformio.ini` by copying `[env:esp32dev-ble]` and
changing the `board` field. Board IDs are at
[registry.platformio.org](https://registry.platformio.org/platforms/platformio/espressif32/boards).

Then build a merged binary for the web flasher:

```bash
pio run -e my-board-ble

BOOT_APP=$(find ~/.platformio -name "boot_app0.bin" -path "*/partitions/*" | head -1)

esptool.py --chip esp32 merge_bin \
  --flash_mode qio --flash_freq 40m --flash_size 4MB \
  -o merged.bin \
  0x1000  .pio/build/my-board-ble/bootloader.bin \
  0x8000  .pio/build/my-board-ble/partitions.bin \
  0xe000  "$BOOT_APP" \
  0x10000 .pio/build/my-board-ble/firmware.bin
```

Upload `merged.bin` using the **Flash a custom binary** section on the web
flasher, or flash directly without merging:

```bash
pio run -e my-board-ble --target upload
```

If your board uses different flash parameters, check
`pio run -e my-board-ble -v` and match the `--flash_mode`, `--flash_freq`,
and `--flash_size` flags to what PlatformIO reports.

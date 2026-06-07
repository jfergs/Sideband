# Developer Setup

Sideband firmware uses PlatformIO with the Arduino framework targeting
original ESP32 boards with Bluetooth Classic support.

## Web Flasher — No Install Required

Open **[jfergs.github.io/Sideband](https://jfergs.github.io/Sideband/)** in
Chrome or Edge, plug in your board via USB-C, and click **Flash Sideband
Bridge**. WebSerial is not supported in Safari or Firefox.

The flasher has two sections:

- **Pre-built firmware** — one-click flash for the confirmed TTGO T-Display
  V1.1 hardware.
- **Flash a custom binary** — upload a merged `.bin` you built yourself for
  any other original ESP32 board.

To enable the flasher on your own fork: repo **Settings → Pages → Source →
GitHub Actions**. It deploys automatically when you push a `vX.Y.Z` tag.

---

## Quick Start — Command Line

No file editing required. The defaults work out of the box.

```bash
# 1. Install PlatformIO
python3 -m pip install --user platformio

# 2. Clone and flash
git clone https://github.com/jfergs/Sideband
cd Sideband/firmware/sideband-bridge
pio run -e ttgo-t-display-ble --target upload
```

The device boots in BLE mode and immediately starts advertising. Open your
iOS APRS app, go to TNC or radio settings, select **Bluetooth TNC**, and
connect to `Sideband Bridge`.

---

## Hardware

**Confirmed working:** TTGO T-Display V1.1 with the **original ESP32** chip
(not S3). The silkscreen on the back of the PCB reads `T-Display V1.1`.

The S3 variant lacks Bluetooth Classic and cannot connect to TH-D75 class
radios over Bluetooth SPP.

Visual identification: check the chip marking on the metal-shielded module.
Original ESP32 modules are marked `ESP32-D0WD` or `ESP32-D0WDQ6`. S3 modules
are marked `ESP32-S3`. The silkscreen on the PCB reverse is the most reliable
check — V1.1 boards ship with original ESP32.

Other original ESP32 boards (WROOM, WROVER, DevKitC) also work but do not
have the onboard TFT display.

**You also need:** a USB-C data cable (not charge-only) and a TH-D75 class
radio with Bluetooth enabled.

## Requirements

- Python 3.10 or newer
- PlatformIO Core (`python3 -m pip install --user platformio`)
- USB-C data cable for flashing

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

The device resets automatically after flashing. If the upload fails, hold the
BOOT button (GPIO0) while clicking upload to force the board into download mode.

## First Boot

The device starts in **BLE mode** and immediately advertises as
`Sideband Bridge` using the BLE KISS TNC service profile
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

The device saves the MAC and restarts. It will connect directly by MAC on
every subsequent boot, skipping the slower name scan.

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
1. Open the app → TNC or radio settings → **Bluetooth TNC**
2. Scan — `Sideband Bridge` appears in the list
3. Tap to connect — the bridge relays KISS frames between app and radio

No pairing or PIN is required. The bridge handles connection parameter
negotiation automatically.

## Testing Your Setup

After flashing, work through these checks in order. Each one builds on the
previous.

**1. Board comes up**

Connect a serial monitor:

```bash
pio device monitor --baud 115200
```

Within a few seconds you should see:

```
SIDEBAND ble advertising name="Sideband Bridge" mac=XX:XX:XX:XX:XX:XX ...
SIDEBAND ble gap adv_start status=0
```

`status=0` confirms advertising started. Any non-zero value means the BLE
stack did not start — check that you flashed `ttgo-t-display-ble`, not a
non-BLE target.

The TFT should show `MODE: BLE` and `BLE: ADVERTISING`.

**2. Radio link**

If your TH-D75 is on and in Bluetooth range, within 15 seconds you should see:

```
SIDEBAND radio state=Connected peer="TH-D75"
```

The tower icon on the TFT turns yellow (one side connected) and green when
both the radio and a BLE client are linked.

If the radio doesn't connect: run `radio show` to confirm the MAC or name is
correct, and check that the TH-D75 has Bluetooth enabled (Menu 980) and KISS
mode set to Bluetooth (Menu 982).

**3. iOS app sees the bridge**

Open APRS.fi (or Packet Commander / RadioMail / PocketPacket) → TNC settings
→ Bluetooth TNC → scan. `Sideband Bridge` should appear. Tap to connect.

The serial monitor will print:

```
SIDEBAND ble client connected
```

The TFT BLE line changes from `ADVERTISING` to `CONNECTED`.

**4. KISS frames flow**

With the radio in KISS mode and the app connected, the status counters should
start moving. Run `status` in the serial monitor:

```
kiss_tx=N      — frames sent from app to radio (increments as app transmits)
kiss_rx=N      — frames from radio to app (increments as radio receives)
radio_to_client=N
client_to_radio=N
```

If `kiss_tx` increments but `kiss_rx` stays at zero, the radio is receiving
but not sending back — check the radio is in KISS12 mode with the correct
band and data speed configured.

**5. Confirm heap headroom**

Check the heap values in the BLE advertising log line:

```
heap_free=NNNNN heap_min=NNNNN
```

`heap_free` should be above 60 KB. If it drops below 40 KB under load
(both stacks active + active app session), flag it in SB-013.

## Mode Switching

The left button (GPIO0) cycles through modes: **BLE → WiFi-AP → WiFi-STA**

The right button (GPIO35) runs a radio link test and prints the result to the
serial monitor.

The active mode is saved to NVS and restored on restart.

## Serial Monitor

```bash
pio device monitor --baud 115200
```

The status line prints every 5 seconds and shows link state, packet counters,
and heap. Diagnostics are suppressed in USB mode (the serial port becomes a
transparent KISS pipe when USB mode is active).

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

If your board uses different flash parameters, run `pio run -e my-board-ble -v`
and match the `--flash_mode`, `--flash_freq`, and `--flash_size` flags to
what PlatformIO reports.

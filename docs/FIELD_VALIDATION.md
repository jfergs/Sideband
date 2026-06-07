# Field Validation

Use these checklists after flashing to validate end-to-end operation. Work
through BLE mode first — it is the primary client transport.

Do not commit packet captures, callsigns, pairing records, or location-bearing
field logs.

---

## Baseline — Flash and Confirm Boot

Flash the primary target and open a serial monitor:

```bash
cd firmware/sideband-bridge
pio run -e ttgo-t-display-ble --target upload
pio device monitor --baud 115200
```

Expected on first boot:

```
SIDEBAND ble advertising name="Sideband Bridge" mac=XX:XX:XX:XX:XX:XX ...
SIDEBAND ble gap adv_start status=0
```

TFT display expected:

```
MODE: BLE
BLE:  ADVERTISING
LINK: BLE RADIO TH-D75
```

If `adv_start status` is non-zero, the BLE stack did not initialise — confirm
you flashed `ttgo-t-display-ble` and not a non-BLE target.

---

## BLE Mode Validation

### Radio Link

Enable Bluetooth on the TH-D75 and confirm it is in range. Within 15 seconds:

```
SIDEBAND radio state=Connected peer="TH-D75"
```

TFT tower icon turns yellow. If connection fails repeatedly, set the MAC:

```
radio mac AA:BB:CC:DD:EE:FF
```

Find the TH-D75 MAC at **Menu 918** (Bluetooth Device Address).

### iOS App Discovery

On iPhone, open APRS.fi → TNC Settings → Bluetooth TNC → Scan.
`Sideband Bridge` must appear. Tap to connect.

Expected in serial monitor:

```
SIDEBAND ble client connected
```

TFT BLE line changes from `ADVERTISING` to `CONNECTED`. Tower icon turns green
when both radio and BLE client are linked.

Repeat with **Packet Commander**, **RadioMail**, and **PocketPacket** if
available — all use the same KTS service UUID and should discover the bridge
identically.

### KISS Frame Relay — BLE to Radio

Radio setup:

- TH-D75 in `KISS12` mode
- KISS interface set to Bluetooth (Menu 982)
- Data speed: 1200 bps
- Data band: active packet band

Run `kiss reset` in the serial monitor, then trigger a packet transmission
from the iOS app (beacon, position, or message).

Expected:

```
kiss_tx increments
client_to_radio increments
```

### KISS Frame Relay — Radio to App

With the radio receiving packets on the air or from another station, the
monitor counters should update:

```
kiss_rx increments
radio_to_client increments
```

The iOS app should display decoded APRS packets.

### Heap Check

After both stacks are active with a client connected, read the heap values:

```bash
status
```

Look for `heap_free` in the BLE advertising log line from boot, or run
`status` to see the current heap. Target: `heap_free` above 60 KB under load.
Flag values below 40 KB in SB-013.

### Simultaneous Radio + BLE

Confirm the Classic BT radio link stays connected while a BLE app client is
active. The TFT tower icon should remain green. BLE advertising restart
happens automatically every 3 seconds when no client is connected, and
immediately after each Classic BT page attempt ends.

---

## Wi-Fi AP Mode Validation

Switch to Wi-Fi AP mode:

```
mode wifi-ap
```

or press the left button (GPIO0) until `MODE: WIFI-AP` is shown on the TFT,
then press the right button (GPIO35) to confirm.

Expected:

```
wifi_ap=READY
wifi=192.168.4.1
```

TFT shows the AP SSID and password.

### KISS TCP Client

Connect a phone or laptop to the Sideband Wi-Fi AP (password: `sideband-bridge`).
Point an iOS APRS app at the TCP TNC endpoint:

```
192.168.4.1:8001
```

With radio in KISS12 mode, send a packet from the app:

```
wifi_tcp=connected
client_to_radio increments
kiss_tx increments
```

### Web Configuration

Open `http://192.168.4.1/` in a browser on the connected device. Expected:

- Status card shows mode, TCP ingress, Wi-Fi client count, radio state, KISS
  counters, and CAT diagnostic counters.
- Radio target settings save without breaking the KISS TCP service.
- Reset KISS, Reset CAT, and Radio test produce matching serial output.

---

## Wi-Fi STA Mode Validation

Set credentials:

```
wifi ssid YourNetwork
wifi pass YourPassword
mode wifi-sta
```

Expected after connect:

```
wifi_sta=Connected
wifi=192.168.x.x
```

mDNS should resolve `sideband.local`. Connect a KISS TCP client to the DHCP
IP on port 8001. Validate TX/RX counters and reconnect after a simulated
network drop.

---

## KISS Bench Exerciser

Run from the repository root while connected to the Sideband Wi-Fi AP:

```bash
python3 scripts/kiss-exerciser.py --payload-hex 00
python3 scripts/kiss-exerciser.py --payload-hex '00 c0 db'
python3 scripts/kiss-exerciser.py --malformed
```

Expected:

- Valid frames: `kiss_tx` increments.
- Escaped FEND/FESC: pass without incrementing `kiss_malformed`.
- Malformed frame: `kiss_malformed` increments, `kiss_last=bad_escape`.

Raw pass-through:

```
tcp raw
```

```bash
python3 scripts/kiss-exerciser.py --raw-text TEST --cr
```

Expected: `client_to_radio` increments, `kiss_malformed` unchanged.

---

## Known Results

- APRS monitor mode validated: TH-D75 raw APRS output over Bluetooth SPP into
  Sideband, out through the RX buffer and replay path.
- KISS12 mode validated end-to-end: iOS APRS app over Wi-Fi TCP KISS, packets
  to and from TH-D75.
- BLE advertising visible on iPhone via nRF Connect.
- BLE KTS service UUID discoverable. iOS app end-to-end KISS validation
  in progress (SB-043).

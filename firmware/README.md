# Firmware

Firmware for the Sideband bridge lives here.

The primary target is a Bluetooth Classic capable ESP32 board that can connect
to a Kenwood TH-D75 class radio over SPP while exposing USB-C serial or Wi-Fi
TCP KISS transports for mobile clients.

## Current Scaffold

- `sideband-bridge/`: initial PlatformIO Arduino scaffold.
- Supported first-pass chip family: original ESP32 with Bluetooth Classic.
- Not primary bridge targets: ESP32-S3, ESP32-C3, ESP32-C6.
- Wi-Fi TCP input defaults to KISS framing. For bench testing raw APRS output
  or TNC command streams, use the serial monitor commands `tcp raw` and
  `tcp kiss` to switch the persisted TCP ingress mode.
- KISS diagnostics are available from the serial monitor with `kiss stats` and
  `kiss reset`. `scripts/kiss-exerciser.py` can send known TCP KISS and raw
  payloads to the bridge during bench tests.
- Wi-Fi mode advertises the TCP KISS service over mDNS as `_kiss-tnc._tcp` on
  port `8001` with default host name `sideband.local`. The display shows the AP
  SSID and Wi-Fi password for field setup.
- Wi-Fi mode also exposes a lightweight onboard web configuration page at
  `http://192.168.4.1/` and advertises HTTP over mDNS. The web UI shows bridge
  status and provides controls for radio target settings, active client mode,
  TCP ingress mode, KISS/CAT diagnostic resets, radio link test, and restart.
  The web UI runs separately from the KISS TCP service and does not replace
  `192.168.4.1:8001` for packet applications.
- Connection diagnostics distinguish Wi-Fi AP readiness, AP station count, TCP
  KISS client state, stale TCP client disconnects, Bluetooth reconnect
  failures, and Bluetooth transport restarts. Wi-Fi AP startup retries until the
  AP has a valid IP, and repeated Bluetooth reconnect failures restart the
  Bluetooth transport without taking down the Wi-Fi/web services.

## Safety Notes

- The bridge should only relay packets between explicitly paired transports.
- Packet payload logging must be opt-in.
- Diagnostics should prefer counters, state, and error codes over raw payloads.

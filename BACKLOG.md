# Sideband Backlog

Project-local backlog for Sideband. Keep off-grid communications and field
radio design work here; roll up only cross-project dependencies to
`workspace-tracker`.

## Current State

- Working KISS bridge validated end-to-end with TH-D75 and iOS APRS apps.
- Client transports: BLE KISS TNC (primary), Wi-Fi AP, Wi-Fi STA, USB-C serial.
  Button cycles BLE → WiFi-AP → WiFi-STA; BLE and Wi-Fi are mutually exclusive.
- BLE KISS TNC service (`00000001-ba2a-46c9-ae49-01b0961f68bb`) advertised from
  `ttgo-t-display-ble` target; iOS apps APRS.fi, Packet Commander, RadioMail,
  and PocketPacket connect directly without nRF Connect. End-to-end KISS frame
  validation in progress (SB-043).
- Bluetooth Classic SPP radio link and BLE client transport run simultaneously
  on original ESP32 BTDM stack.  Advertising restart logic compensates for BLE
  suppression during Classic BT page attempts.
- Wi-Fi STA: credentials stored in NVS, async connect with retry, web UI +
  serial commands for configuration, mDNS and TCP KISS on DHCP IP.
- Onboard web UI accessible in both Wi-Fi AP and Wi-Fi STA modes.
- Individual `SB-XXX` cards are tracked directly in this backlog.

## High Priority

- Define the first Sideband operating scenario.
  - Start with a Kenwood TH-D75 class radio paired to an ESP32 bridge.
  - Support iPhone/mobile packet workflows over USB-C serial or Wi-Fi TCP.
  - Keep low-bandwidth status exchange, mesh relay notes, and radio-adjacent
    operator coordination as later scenarios.
  - Identify the devices involved and what data is safe to transmit.
  - Document what Sideband owns versus Gridrunner, CrashCart, or Meshtastic.

- `SB-001` Initialize the repository structure.
  - Create directories for `firmware/`, `docs/`, `hardware/`, `enclosure/`,
    `protocol/`, `references/`, and `experiments/`.
  - Add placeholder README files where a directory needs project orientation.
  - Add MIT license, PlatformIO/VSCode `.gitignore`, `CONTRIBUTING.md`, and
    `CODEOWNERS`.

- `SB-002` Create the initial project README.
  - Describe the TH-D75 Bluetooth bridge purpose.
  - Explain USB-C/Wi-Fi client transport to Bluetooth Classic radio
    translation.
  - Document supported ESP32 families and recommended boards.
  - Include architecture diagrams and project philosophy.
  - Warn clearly that ESP32-S3, ESP32-C3, and ESP32-C6 do not support Bluetooth
    Classic and are not suitable for the primary radio bridge.

- `SB-090` Write the architecture note.
  - Define roles for radio, ESP32 bridge, iPhone/mobile app, and future relay
    services.
  - Capture Bluetooth Classic SPP, USB serial, KISS, Wi-Fi TCP KISS, MQTT,
    Meshtastic, and file-based exchange as transport options.
  - Document Bluetooth architecture, KISS protocol flow, USB/Wi-Fi client
    transports, and future expansion boundaries.
  - Define privacy and redaction expectations for field logs.

- Build a message/data model.
  - Start with simple status, note, contact, and incident records.
  - Include timestamps, source, confidence, and retention guidance.
  - Keep the format usable offline and easy to sync later.
  - Define how KISS packets, connection telemetry, and packet counters map into
    Sideband diagnostics without logging sensitive payloads by default.

- Decide first implementation target.
  - PlatformIO firmware prototype on Bluetooth Classic capable ESP32 hardware.
  - TFT instrumentation UI with packet counters and connection state.
  - USB-C serial and Wi-Fi TCP services for mobile clients.
  - KISS relay between Bluetooth Classic SPP and USB-C/Wi-Fi clients.

- `SB-010` Document compatible ESP32 hardware.
  - Supported chips: ESP32-WROOM, ESP32-WROVER, and ESP32-D0WD families with
    Bluetooth Classic support.
  - Unsupported primary-bridge chips: ESP32-S3, ESP32-C3, and ESP32-C6 because
    they lack Bluetooth Classic.
  - Add Bluetooth Classic capability matrix, board recommendations, and vendor
    references from Amazon, DigiKey, and Mouser.

- `SB-011` Validate TTGO T-Display V1.1 compatibility. ✓
  - Confirmed working: TTGO T-Display V1.1 (silkscreen on PCB reverse),
    original ESP32, 4 MB flash. BLE + Classic BT + TFT all functional.

- `SB-020` Set up the PlatformIO firmware foundation.
  - Configure ESP32 board definitions for supported Bluetooth Classic boards.
  - Add TFT_eSPI dependency for supported display boards.
  - Validate serial flashing and serial monitor workflow.

- `SB-091` Create developer setup guide.
  - Document PlatformIO setup, flashing, debugging, serial monitor workflow,
    and library installation.

- `SB-021` Bring up the TFT instrumentation display.
  - Display Sideband splash screen, status text, packet counters, Bluetooth
    state, and error state.
  - Add a screen refresh/update loop suitable for field use.

- `SB-022` Create Sideband UI framework.
  - Create a reusable instrumentation UI framework with status bar, connection
    indicators, packet counters, battery status hooks, and low-refresh
    avionics-style layout.

- `SB-023` Fix TFT refresh flicker.
  - Investigate visible flicker during status screen refresh on TTGO T-Display.
  - Compare against the Waveshare ESP32-C6 display refresh issue pattern from
    related projects.
  - Replace full-screen redraws with dirty-region, sprite, or partial update
    rendering where practical.
  - Preserve low-refresh field readability while avoiding distracting screen
    flash during normal status updates.

- `SB-024` Add radio tower connection-status iconography.
  - Add a compact broadcast-tower icon to the TFT instrumentation screen.
  - Mirror radio-link state with changing signal icons around the tower.
  - Represent discoverable, connecting, pairing, connected, and error states
    with distinct visual treatments.
  - Keep the icon readable on TTGO T-Display class screens without crowding
    packet counters or pairing-code text.
  - Use the same state source as the Bluetooth connection state machine so the
    visual indicator does not drift from serial/status output.
  - Refine custom question-mark glyphs so they read clearly as question marks
    when rotated toward the tower instead of resembling magnifying glasses.

- `SB-030` Implement Bluetooth Classic radio transport.
  - Discover, pair with, and connect to a TH-D75 class radio over SPP.
  - Maintain stable connection, detect disconnects, and auto-reconnect.
  - Current diagnostics:
    - Serial monitor status reports raw radio RX bytes separately from forwarded
      client writes.
    - `radio raw <command>` can send CR-terminated TNC commands directly over
      the radio Bluetooth SPP link for bench testing without a packet app.
    - A rolling radio RX buffer keeps the most recent radio bytes for
      `radio dump` inspection or `radio replay` to a connected Wi-Fi client.
    - Wi-Fi TCP ingress can be switched between default KISS framing and raw
      pass-through with `tcp kiss` / `tcp raw`.
    - Wi-Fi AP startup is tracked as `wifi_ap=STARTING/READY/ERROR`, retries
      until the AP has a valid IP, and logs the SSID, KISS port, and HTTP URL
      when ready.
    - TCP KISS clients track activity and stale half-open clients are closed so
      new app connections can be accepted without long waits.
    - Repeated Bluetooth SPP reconnect failures trigger a Bluetooth transport
      restart while leaving Wi-Fi and web configuration running.
    - `kiss stats` and `kiss reset` expose KISS frame/byte counters and the
      last parser error.
    - `scripts/kiss-exerciser.py` sends known KISS, malformed KISS, and raw TCP
      payloads to the bridge for bench validation.
    - Wi-Fi mode advertises `_kiss-tnc._tcp` over mDNS and shows the AP SSID
      and password on the TTGO display.

- `SB-031` Implement persistent pairing storage.
  - Store paired MAC address and restore previous connections from
    NVS/Preferences.
  - Add pairing reset mode.

- `SB-032` Implement Bluetooth connection state machine.
  - Implement a connection state machine with idle, pairing, connected,
    reconnecting, error handling, timeout recovery, and operator-visible state.

- `SB-040` Implement USB-C iPhone/client connectivity.
  - Expose Sideband packet relay over the USB serial data path.
  - Keep diagnostics out of the packet stream while USB mode is active.
  - Validate KISS frame pass-through, reconnect behavior, and iPhone adapter
    workflows.

- `SB-043` Validate BLE KISS TNC transport end-to-end.
  - Flash `ttgo-t-display-ble` target. ✓
  - Verify BLE advertising visible from iPhone (confirmed via nRF Connect). ✓
  - Device visible in APRS.fi, Packet Commander, RadioMail, PocketPacket via
    KTS service UUID `00000001-ba2a-46c9-ae49-01b0961f68bb`. In progress.
  - Confirm KISS frames pass in both directions; verify `kiss_rx`/`kiss_tx` counters.
  - Confirm Classic BT radio link holds simultaneously with BLE client connected.
  - Log heap before and after both stacks are active.
  - If stable, update SB-013 with a go/no-go recommendation for single-device BLE.

- `SB-044` Validate WiFi STA mode end-to-end.
  - Flash `ttgo-t-display` target.  Configure STA credentials via serial or AP
    web UI, switch mode to WiFi-STA.
  - Confirm device connects to a WPA2 network and appears on the LAN.
  - Confirm mDNS resolves `sideband.local` and `_kiss-tnc._tcp` is advertised.
  - Connect an iOS APRS app to the DHCP IP on port 8001.
  - Validate KISS TX/RX counters and reconnect after a simulated network drop.
  - Document web UI credential workflow in FIELD_VALIDATION.md.

- `SB-042` ~~Remove BLE/iOS discovery transport from the primary firmware.~~
  **Reversed.** BLE KISS TNC (KTS profile) is now the primary client transport.
  BLE advertising, KTS UUIDs, iOS connection parameter negotiation, and BTDM
  coexistence logic are all implemented and advertising. See SB-043 for
  validation status.

- `SB-050` Implement KISS protocol middleware.
  - Add KISS parser and serializer support for FEND, FESC, TFEND, TFESC, packet
    escaping, and malformed-frame handling.
  - Current foundation:
    - USB-C serial and Wi-Fi TCP client ingress use a shared KISS frame parser.
    - Client frames are decoded, re-escaped, and serialized onto the radio link.
    - Malformed or oversized client frames are dropped and counted without
      logging payload bytes.
    - Bench validation against the TH-D75 confirmed Wi-Fi TCP KISS client frames
      reach the radio, malformed escapes are counted, escaped FEND/FESC payloads
      pass, and KISS12 radio frames are observed over Bluetooth.
    - iOS APRS app validation confirmed the Sideband TCP KISS endpoint can be
      selected as a TNC, send KISS frames to the TH-D75, receive KISS frames from
      the TH-D75, and display decoded APRS messages.
  - Remaining acceptance criteria:
    - Add automated host-side parser tests beyond the bench exerciser.

- `SB-051` Create transparent packet relay.
  - Relay radio to USB-C/Wi-Fi clients and clients to radio while preserving
    packet integrity.
  - Bench validation:
    - APRS mode with `Menu 590 = Raw Packets` and `Menu 982 = Bluetooth`
      produced raw APRS text over Bluetooth and through the Sideband RX buffer.
    - KISS12 mode produced KISS-framed radio output over Bluetooth; with a TCP
      client connected, Sideband forwarded the frame to Wi-Fi TCP.
    - iOS app monitor mode decoded APRS packets through the Sideband TNC over
      Wi-Fi TCP KISS.

- `SB-052` Implement packet logging.
  - Add concurrent buffering, TX/RX counters, serial debug logging, optional hex
    dump mode, error reporting, and connection telemetry.

## Medium Priority

- `SB-003` Define project branding and identity.
  - Document visual aesthetic, logo concepts, UI typography/colors, and
    terminal/instrumentation inspired design language.
  - Define naming conventions for future Sideband variants.

- Create integration notes.
  - Gridrunner edge-node telemetry.
  - CrashCart recovery workflows.
  - Meshtastic channel usage.
  - Future ham radio utilities.
  - Reticulum interoperability and possible gateway architecture.
  - ATAK/TAK interoperability through APRS position forwarding, CoT event
    generation, GPS integration, and field mapping concepts.

- Add field operations checklists.
  - Pre-deployment.
  - Communications test.
  - Lost-link fallback.
  - Post-operation log review.

- `SB-092` Create field operations manual.
  - TH-D75 pairing.
  - iPhone USB-C serial and Wi-Fi TCP connection.
  - APRS setup.
  - Troubleshooting and recovery procedures.

- `SB-012` Research portable power options.
  - Compare USB-C battery packs, LiPo charging options, and NP-F integration.
  - Document current draw and runtime estimates for supported boards.

- `SB-013` Single-device ESP32 BLE + Classic BT — go/no-go pending SB-043.
  - BLE KISS TNC (KTS profile) implemented on `ttgo-t-display-ble` target using
    Arduino-ESP32 BLE stack. BTDM dual-mode: Classic BT for radio SPP,
    BLE for client. NUS (Nordic UART Service) replaced with KTS standard UUIDs.
  - BLE advertising confirmed visible on iPhone (nRF Connect). ✓
  - iOS APRS app discovery via KTS UUID in progress (SB-043).
  - Heap logging present in serial output; gate go/no-go on SB-043 result
    with <40 KB free under both stacks as fail threshold.
  - Dual-device fallback path (Classic ESP32 bridge + S3 BLE/display coprocessor)
    remains the contingency if single-device heap or coexistence proves unstable.

- `SB-080` Design portable enclosure.
  - Create compact field enclosure concepts with USB access, screen visibility,
    ventilation, MOLLE compatibility, and portable mounting options.

- `SB-081` Design cyberdeck integration mount.
  - Create cyberdeck integration concepts for rail, Velcro, magnetic, and
    field deployment mounting.

- `SB-070` Wi-Fi TCP KISS — done for AP and STA modes.
  - AP mode: hotspot with fixed SSID/password, TCP KISS on port 8001.
  - STA mode: joins saved network, DHCP IP, same TCP KISS endpoint.
  - Multiple-client rule: single active TCP client (most-recent wins), stale
    connections timed out after WIFI_TCP_IDLE_TIMEOUT_MS.
  - Remaining: Wi-Fi STA field validation; credential input UX review for
    field conditions without a laptop (serial commands or AP-mode web UI first).

- `SB-071` Add onboard web configuration.
  - Provide mobile-friendly controls for Bluetooth pairing, Wi-Fi settings,
    packet monitoring, and firmware update support.
  - Current foundation:
    - Wi-Fi mode starts a lightweight HTTP server on port 80 alongside the KISS
      TCP service on port 8001.
    - The web UI exposes status, radio target fields, active mode switching,
      TCP ingress mode, KISS/CAT diagnostic resets, radio link test, and restart
      controls.
    - mDNS advertises the HTTP service in addition to `_kiss-tnc._tcp`.
  - Remaining acceptance criteria:
    - Add authentication or an explicit field-mode warning before exposing any
      sensitive settings beyond the local AP.
    - Add firmware update support only after the core bridge path is stable.

- `SB-072` Add MQTT telemetry.
  - Publish TX/RX metrics, device health, packet metadata, and future GPS
    telemetry hooks.

- `SB-073` Research Reticulum integration.
  - Evaluate transport compatibility, gateway architecture, packet exchange,
    and integration strategy.

- `SB-074` Research Meshtastic integration.
  - Evaluate packet bridge concepts, LoRa coexistence, gateway topology, and
    shared telemetry concepts.

- `SB-075` Research ATAK/TAK integration.
  - Evaluate APRS position forwarding, CoT event generation, GPS integration,
    and field mapping concepts.

- `SB-076` Investigate CAT and Packet Commander frequency control.
  - Determine whether Packet Commander exposes frequency control to generic TCP
    KISS TNCs or only to specific supported radio/control profiles.
  - Capture and decode any KISS SetHardware, B.B. Link-style, BLE, or separate
    CAT/control commands emitted during Packet Commander frequency changes.
  - Keep CAT/control handling separate from the working KISS packet relay unless
    the app explicitly sends hardware-control frames through the TNC stream.
  - Evaluate TH-D75 command support for setting VFO/frequency over Bluetooth
    SPP while preserving APRS/KISS operation.
  - If viable, implement a guarded CAT control path with clear serial
    diagnostics, command counters, and a rollback/failsafe mode.

## Validation

- Walk through the first scenario without internet access.
- Confirm no sensitive identifiers are exposed by default.
- Confirm terminology and ownership boundaries are clear enough to implement.
- `SB-060` Validate TH-D75 end-to-end connectivity.
  - Connect to the radio, exchange KISS frames, verify packet integrity, and
    test reconnect scenarios.
- `SB-041` Validate iOS packet application compatibility.
  - Test RadioMail, APRS applications, and Packet Commander where feasible.
  - Confirm packet integrity and reconnect behavior over USB-C serial and
    Wi-Fi TCP.
- `SB-061` Perform long-duration field testing.
  - Target 24+ hour runtime, reconnect recovery, packet loss metrics, memory
    leak checks, and Bluetooth recovery validation.

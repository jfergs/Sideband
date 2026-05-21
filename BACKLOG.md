# Sideband Backlog

Project-local backlog for Sideband. Keep off-grid communications and field
radio design work here; roll up only cross-project dependencies to
`workspace-tracker`.

## Current State

- Repository has moved from planning placeholder to initial implementation
  scaffold.
- README defines the project around the TH-D75 class Bluetooth bridge with
  USB-C serial and Wi-Fi TCP client transports.
- Architecture notes define Bluetooth Classic SPP, USB serial, Wi-Fi TCP KISS,
  privacy defaults, and future expansion boundaries.
- Repository structure, contribution files, license, hardware/protocol notes,
  developer setup notes, and the first PlatformIO firmware scaffold are present.
- Initial direction is a portable RF bridge and packet middleware for Kenwood
  TH-D75 class radios.
  - Core transport: ESP32 Bluetooth Classic SPP to radio.
  - Client transport: USB-C serial or Wi-Fi TCP for iPhone/mobile
    interoperability.
  - Packet middleware: KISS framing and transparent relay.
  - Field UI: low-refresh instrumentation display on supported ESP32 boards.
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

- `SB-011` Validate TTGO T-Display v1.3 compatibility.
  - Validate TTGO T-Display v1.3 variants, including original ESP32 versus S3
    boards, visual identification, USB port differences, TFT driver needs, and
    Bluetooth Classic support.

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
    - `kiss stats` and `kiss reset` expose KISS frame/byte counters and the
      last parser error.
    - `scripts/kiss-exerciser.py` sends known KISS, malformed KISS, and raw TCP
      payloads to the bridge for bench validation.

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

- `SB-042` Remove BLE/iOS discovery transport from the primary firmware.
  - Drop BLE UART, HID discovery mode, NimBLE dependency, and BLE diagnostic
    build targets from the supported path.
  - Preserve Bluetooth Classic only for radio-side connectivity.
  - Document BLE as deferred or unsupported unless a separate coprocessor
    architecture is introduced.

- `SB-050` Implement KISS protocol middleware.
  - Add KISS parser and serializer support for FEND, FESC, TFEND, TFESC, packet
    escaping, and malformed-frame handling.
  - Current foundation:
    - USB-C serial and Wi-Fi TCP client ingress use a shared KISS frame parser.
    - Client frames are decoded, re-escaped, and serialized onto the radio link.
    - Malformed or oversized client frames are dropped and counted without
      logging payload bytes.
  - Remaining acceptance criteria:
    - Validate escaped frame pass-through against the TH-D75 and mobile packet
      apps.
    - Add host-side parser tests or a firmware-adjacent harness for malformed
      frame cases.

- `SB-051` Create transparent packet relay.
  - Relay radio to USB-C/Wi-Fi clients and clients to radio while preserving
    packet integrity.

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

- `SB-080` Design portable enclosure.
  - Create compact field enclosure concepts with USB access, screen visibility,
    ventilation, MOLLE compatibility, and portable mounting options.

- `SB-081` Design cyberdeck integration mount.
  - Create cyberdeck integration concepts for rail, Velcro, magnetic, and
    field deployment mounting.

- `SB-070` Add Wi-Fi TCP KISS support.
  - Add TCP server mode, configurable port, Wi-Fi AP mode, and future Wi-Fi
    client mode.
  - Define multiple-client behavior and packet ownership rules.

- `SB-071` Add onboard web configuration.
  - Provide mobile-friendly controls for Bluetooth pairing, Wi-Fi settings,
    packet monitoring, and firmware update support.

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

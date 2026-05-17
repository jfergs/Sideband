# Sideband Backlog

Project-local backlog for Sideband. Keep off-grid communications and field
radio design work here; roll up only cross-project dependencies to
`workspace-tracker`.

## Current State

- Repository is an early planning workspace.
- README defines the project scope around off-grid communications, mesh
  networking, and field-radio workflows.
- Architecture and implementation notes are still placeholders.
- Initial direction is a portable RF bridge and packet middleware for Kenwood
  TH-D75 class radios.
  - Core transport: ESP32 Bluetooth Classic SPP to radio.
  - Client transport: BLE UART for iPhone/mobile interoperability.
  - Packet middleware: KISS framing and transparent relay.
  - Field UI: low-refresh instrumentation display on supported ESP32 boards.
- Individual `SB-XXX` cards are tracked directly in this backlog.

## High Priority

- Define the first Sideband operating scenario.
  - Start with a Kenwood TH-D75 class radio paired to an ESP32 bridge.
  - Support iPhone/mobile packet workflows over BLE UART.
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
  - Explain BLE to Bluetooth Classic translation.
  - Document supported ESP32 families and recommended boards.
  - Include architecture diagrams and project philosophy.
  - Warn clearly that ESP32-S3, ESP32-C3, and ESP32-C6 do not support Bluetooth
    Classic and are not suitable for the primary radio bridge.

- `SB-090` Write the architecture note.
  - Define roles for radio, ESP32 bridge, iPhone/mobile app, and future relay
    services.
  - Capture Bluetooth Classic SPP, BLE UART, KISS, Wi-Fi TCP KISS, MQTT,
    Meshtastic, and file-based exchange as transport options.
  - Document Bluetooth architecture, KISS protocol flow, BLE transport, and
    future expansion boundaries.
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
  - BLE UART service for mobile clients.
  - KISS relay between Bluetooth Classic SPP and BLE.

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
  - Add TFT_eSPI and NimBLE-Arduino dependencies.
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

- `SB-030` Implement Bluetooth Classic radio transport.
  - Discover, pair with, and connect to a TH-D75 class radio over SPP.
  - Maintain stable connection, detect disconnects, and auto-reconnect.

- `SB-031` Implement persistent pairing storage.
  - Store paired MAC address and restore previous connections from
    NVS/Preferences.
  - Add pairing reset mode.

- `SB-032` Implement Bluetooth connection state machine.
  - Implement a connection state machine with idle, pairing, connected,
    reconnecting, error handling, timeout recovery, and operator-visible state.

- `SB-040` Implement BLE iPhone connectivity.
  - Expose Sideband as a BLE UART device.
  - Provide BLE advertising and Nordic UART Service compatible TX/RX
    characteristics.
  - Validate iPhone discoverability and stable reconnect behavior.

- `SB-042` Research BLE MTU handling.
  - Research BLE MTU handling for KISS packets, including fragmentation,
    binary payload handling, packet framing, and iOS limitations.

- `SB-050` Implement KISS protocol middleware.
  - Add KISS parser and serializer support for FEND, FESC, TFEND, TFESC, packet
    escaping, and malformed-frame handling.

- `SB-051` Create transparent packet relay.
  - Relay radio to BLE and BLE to radio while preserving packet integrity.

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
  - iPhone BLE connection.
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
  - Add TCP server mode, configurable port, Wi-Fi AP mode, and Wi-Fi client
    mode.
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
  - Confirm packet integrity and reconnect behavior.
- `SB-061` Perform long-duration field testing.
  - Target 24+ hour runtime, reconnect recovery, packet loss metrics, memory
    leak checks, and Bluetooth recovery validation.

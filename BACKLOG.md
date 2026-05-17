# Sideband Backlog

Project-local backlog for Sideband. Keep off-grid communications and field
radio design work here; roll up only cross-project dependencies to
`workspace-tracker`.

## Current State

- Repository is an early planning workspace.
- README defines the project scope around off-grid communications, mesh
  networking, and field-radio workflows.
- Architecture and implementation notes are still placeholders.

## High Priority

- Define the first Sideband operating scenario.
  - Pick one concrete field workflow, such as low-bandwidth status exchange,
    mesh relay notes, or radio-adjacent operator coordination.
  - Identify the devices involved and what data is safe to transmit.
  - Document what Sideband owns versus Gridrunner, CrashCart, or Meshtastic.

- Write the architecture note.
  - Define roles for handheld nodes, base stations, and relay devices.
  - Capture transport options: Meshtastic, Wi-Fi, BLE, serial, or file-based
    exchange.
  - Define privacy and redaction expectations for field logs.

- Build a message/data model.
  - Start with simple status, note, contact, and incident records.
  - Include timestamps, source, confidence, and retention guidance.
  - Keep the format usable offline and easy to sync later.

- Decide first implementation target.
  - CLI prototype.
  - Static document/workflow kit.
  - Gridrunner panel integration.
  - Meshtastic message bridge.

## Medium Priority

- Create integration notes.
  - Gridrunner edge-node telemetry.
  - CrashCart recovery workflows.
  - Meshtastic channel usage.
  - Future ham radio utilities.

- Add field operations checklists.
  - Pre-deployment.
  - Communications test.
  - Lost-link fallback.
  - Post-operation log review.

## Validation

- Walk through the first scenario without internet access.
- Confirm no sensitive identifiers are exposed by default.
- Confirm terminology and ownership boundaries are clear enough to implement.

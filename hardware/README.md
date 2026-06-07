# Hardware

Sideband's primary bridge requires Bluetooth Classic SPP. Use original ESP32
families for the radio transport.

## Compatibility Matrix

| Chip family | Bluetooth Classic | BLE | Primary bridge | Notes |
| --- | --- | --- | --- | --- |
| ESP32-WROOM | Yes | Yes | Yes | Good default target. |
| ESP32-WROVER | Yes | Yes | Yes | Useful when PSRAM is needed. |
| ESP32-D0WD | Yes | Yes | Yes | Common original ESP32 silicon. |
| ESP32-S3 | No | Yes | No | BLE-only; useful for displays or companion nodes. |
| ESP32-C3 | No | Yes | No | BLE-only; not suitable for TH-D75 SPP. |
| ESP32-C6 | No | Yes | No | BLE/Wi-Fi 6 capable, no Bluetooth Classic. |

## Confirmed Working

- **TTGO T-Display V1.1** (LilyGO, silkscreen `T-Display V1.1` on PCB reverse).
  Original ESP32, 4 MB flash, 1.14 inch ST7789 135×240 TFT. BLE + Classic BT
  + TFT all functional under `ttgo-t-display-ble` target.

## Other Boards To Evaluate

- Generic ESP32 DevKitC / ESP32-WROOM dev boards.
- ESP32-WROVER dev boards where PSRAM or extra buffering is useful.

## Validation Checklist

- Confirm the board is original ESP32, not an S3 visual refresh.
- Confirm Bluetooth Classic SPP examples compile and run.
- Confirm serial monitor stability at `115200`.
- Confirm USB power remains stable with BLE advertising and radio pairing active.

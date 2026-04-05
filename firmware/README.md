# Firmware

Device firmware for M5StickC Plus2.

## Targets

- `ble_hid/ble_hid_presentation/ble_hid_presentation.ino`
- `uiflow2/main.py`

## Runtime Responsibilities

- BLE HID keyboard output
- Mode state machine (`IDLE`, `SLIDES`, `POINTER`)
- Button interaction (single, double, long press)
- Slide swing gesture in `SLIDES` mode
- Magnifier step zoom in `POINTER` mode
- Device UI rendering (mode, BLE, battery, timer, hints)

## Track Selection

- `ble_hid`: production firmware for direct BLE keyboard control.
- `uiflow2`: Python prototyping firmware for serial command emission.

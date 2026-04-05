# UIFlow2 Python Firmware

This track is a Python implementation for UIFlow2 custom edit runtime.

## File

- `main.py`

## Purpose

- Rapid interaction prototyping on device
- Serial JSON command output for PC bridge development
- Same mode logic as BLE firmware (`IDLE`, `SLIDES`, `POINTER`)

## Notes

- This track does not send BLE HID key events directly.
- It emits command packets through serial output.
- For production wireless control, use `firmware/ble_hid/ble_hid_presentation/ble_hid_presentation.ino`.

# Presentation Bracelet

A wearable presentation controller based on **M5StickC Plus2** and **BLE HID**.

## Overview

This project turns M5StickC Plus2 into a standalone presentation remote for Windows:

- Slide control
- Mode switching on device
- Window switching
- Magnifier zoom control
- On-device status UI (mode, BLE state, battery, session timer)

## Hardware

- M5StickC Plus2
- ESP32 BLE HID keyboard profile

## Implemented Controls

### Modes

- `IDLE`
- `SLIDES`
- `POINTER`

### Button and Gesture Mapping

- `IDLE`
  - `A single`: previous slide
  - `B single`: next slide
  - `A double`: enter `POINTER`
  - `B double`: enter `SLIDES`
- `SLIDES`
  - `A single`: previous slide
  - `B single`: next slide
  - wrist swing: previous/next slide
  - `A long`: `Alt + Tab`
  - `B long`: `Alt + Shift + Tab`
  - `B double`: back to `IDLE`
- `POINTER`
  - `A long`: `Win + Numpad +` (zoom in one step)
  - `B long`: `Win + Numpad -` (zoom out one step)
  - `B double`: back to `IDLE` and close magnifier (`Win + Esc`)

## Firmware Tracks

- `firmware/ble_hid/ble_hid_presentation/ble_hid_presentation.ino`
  - Production firmware (Arduino C++)
  - Direct BLE HID keyboard control
- `firmware/uiflow2/main.py`
  - UIFlow2 Python prototype firmware
  - Serial JSON command output for PC-side bridge testing

## Build and Flash

### Arduino BLE HID (Production)

1. Open `firmware/ble_hid/ble_hid_presentation/ble_hid_presentation.ino` in Arduino IDE.
2. Select board: `M5Stack-Core-ESP32`.
3. Select the device COM port.
4. Install required libraries:
   - `M5Unified`
   - `ESP32 BLE Keyboard` (version compatible with ESP32 core 2.x)
5. Upload firmware.

### UIFlow2 Python (Prototype)

1. Flash UIFlow2 firmware to M5StickC Plus2.
2. Open UIFlow2 web editor and enter custom Python mode.
3. Copy `firmware/uiflow2/main.py` to the device and run.

## Project Structure

```text
.
|-- firmware/
|   |-- ble_hid/
|   |   `-- ble_hid_presentation/
|   |       `-- ble_hid_presentation.ino
|   `-- uiflow2/
|       |-- main.py
|       `-- README.md
|-- docs/
|   `-- README.md
|-- .gitignore
|-- CONTRIBUTING.md
|-- LICENSE
`-- README.md
```

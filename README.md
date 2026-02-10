# LEGO Duplo Train Remote Control

A custom remote control for the [LEGO DUPLO Steam Train (10874)](https://www.lego.com/product/steam-train-10874) and [LEGO DUPLO Cargo Train (10875)](https://www.lego.com/product/cargo-train-10875), built with a Seeed Studio XIAO ESP32S3 microcontroller. Designed for and tested with the **LEGO DUPLO Train set 10427**. Connects to the train over BLE using raw NimBLE and the LEGO Wireless Protocol 3 (LWP3).

## Hardware

- **Board:** Seeed Studio XIAO ESP32S3
- **Potentiometer** on GPIO2 (D1) for variable speed control
- **4 push buttons** (active low, using internal pull-ups):

| GPIO | Pin | Function |
|------|-----|----------|
| 3    | D2  | Water/action sound cycle |
| 4    | D3  | Horn |
| 5    | D4  | Stop (brake) |
| 6    | D5  | LED color cycle |

## Features

- **Variable speed** — potentiometer maps linearly to 0-100% forward or backward, with a 15% deadband around center for stop
- **11 LED colors** — cycle through off, white, green, yellow, light blue, dark blue, purple, purple pink, light pink, red pink, red
- **7 action sounds** — cycle through recorded sound, cat, nothing, beach, rain, birthday, night
- **Horn** — plays the train horn sound
- **Stop button** — immediately stops the motor
- **Auto-reconnect** — automatically scans and reconnects when the train is turned off and back on
- **Debounced buttons** — using the Bounce2 library

## Building & Uploading

Requires [PlatformIO](https://platformio.org/).

```bash
# Compile
pio run

# Upload to board
pio run -t upload

# Serial monitor
pio device monitor
```

## Dependencies

- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) 2.x — BLE stack
- [Bounce2](https://github.com/thomasfredericks/Bounce2) — button debouncing

## Configuration

Pin assignments, potentiometer deadband, speed limits, and timing are all configurable in `config.h`.

## Protocol

Communicates with the Duplo Train Hub using raw LEGO Wireless Protocol 3 (LWP3) packets over BLE:
- **Service UUID:** `00001623-1212-efde-1623-785feabcd123`
- **Characteristic UUID:** `00001624-1212-efde-1623-785feabcd123`
- **Motor packet format (11 bytes):** `0B 00 81 34 11 51 01 [CMD] 01 [SPEED] 00`
  - CMD: `01`=forward, `02`=backward, `03`=stop, `04`=LED, `06`=action, `07`=horn

## Acknowledgments

This project is based on the excellent work by [drndos](https://github.com/drndos) and his [duplo-train-controller](https://github.com/drndos/duplo-train-controller). Many thanks for figuring out the BLE protocol, the raw LWP3 packet format, and providing a working NimBLE implementation that made this project possible!

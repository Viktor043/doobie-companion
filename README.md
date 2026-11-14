# Doobie Companion

A pocket-sized companion with dot-matrix eyes. It runs on a 1.64" AMOLED ESP32-S3 board, reacts to motion and touch, and talks back in tiny snippets. The 3D-printed case has two magnets, so it clips onto wearables, plus a hidden keyring bar.

## Hardware

| Part | Detail |
|---|---|
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.64 (V1) |
| MCU | ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM |
| Display | 280x456 AMOLED over QSPI (CO5300 command set, 20 px column offset) |
| Motion | QMI8658 6-axis IMU at I2C 0x6B |
| Touch | FT3168 at I2C 0x38 |
| Power | Single-cell LiPo on the BAT connector; voltage on GPIO4 (1:3 divider) |
| Buttons | BOOT (GPIO0), RST |

Pins: QSPI CS 9, CLK 10, D0-D3 11-14, LCD RST 21, I2C SDA 47 / SCL 48. The V1.1 board moved LCD CS to GPIO46.

## Firmware

`firmware/doobie/` is an Arduino sketch.

```sh
arduino-cli core install esp32:esp32@3.1.3
arduino-cli lib install "GFX Library for Arduino@1.5.6"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile -b "$FQBN" firmware/doobie
arduino-cli upload  -b "$FQBN" -p /dev/cu.usbmodem* firmware/doobie
```

### What it does

- **Onboarding on first boot.** A dot grid sweeps on, the wordmark types in, the eyes open and say hello. You pick **light** (bright teal, grainy) or **dark** (inverted to near-black, so most pixels are off and the battery lasts longer). Then it walks you through *tilt me* and *tap me*. The choice is saved in NVS.
- **Eyes** are signed-distance shapes sampled on a 9 px dot grid. The edges render as halftone dots, so moods blend smoothly.
- **Reactions:**

| You do | It does |
|---|---|
| Tilt | Eyes follow the low side |
| Shake | Dizzy spirals, then grumpy |
| Bump | Surprised |
| Drop | Panic |
| Turn it upside down | Annoyed |
| Tap | Happy face and blush |
| Hold | Hearts |
| Leave it still for 20 s / 45 s | Sleepy / asleep (dims) |

- **Speech:** about 140 short phrases in 16 situations (`phrases.h`), typed out in a 5x7 dot font and dissolved away. It avoids repeating recent lines and makes the odd unprompted remark.
- **BOOT button:** a short press toggles light and dark. Hold it for 2 s to run onboarding again.

## Case

`cad/` holds a parametric CadQuery model with STEP, STL and 3MF exports. See `cad/README.md` for the parts list and print settings.

## Simulator

`tools/eyes-sim.html` runs the eye engine in a browser: tilt pad, sensor-event buttons, and click or hold on the screen.

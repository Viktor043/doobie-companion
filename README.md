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
| Power | EEMB LP401730 LiPo (3.7 V, 150 mAh, 4x17x30 mm) on the JST 1.25 BAT socket; voltage on GPIO4 (1:3 divider) |
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

- **Profile questions** during onboarding: *he / she / skip*, then *under 18 / 18+*. Adults also get a separate opt-in for spicy lines.
- **Speech:** about 230 short lines (`phrases.h`), typed out in a 5x7 dot font and dissolved away. Lines come in tiers (base, kid, adult, spicy), and `{N}` is swapped for a pet name that suits the profile. It avoids repeating recent lines and makes the odd unprompted remark.
- **Mic reactions (optional add-on):** claps, bangs, sustained noise, music or a party, chatter and long silences each get their own lines, and the eyes bounce with loudness.
- **Power saving** for the 150 mAh cell: about 12 fps while asleep, the panel switches off after 5 minutes asleep, and motion wakes it.

### User journeys

| Journey | Profile | Pet names | Lines it can say | Onboarding ends with |
|---|---|---|---|---|
| Kid | Under 18, any gender | BUDDY, CHAMP, FRIEND, PAL | Base, plus encouraging kid lines ("HIGH FIVE!", "HOMEWORK DONE?") | "LET'S BE FRIENDS, BUDDY!" |
| Grown-up, cute | 18+, spicy off | He: HANDSOME, TIGER… / She: GORGEOUS, QUEEN… / Skip: CUTIE, STAR… | Base, plus flirty adult lines ("OH, NICE FINGER.", "I LIKE THAT.") | "NICE TO MEET YOU, {N}." |
| Grown-up, spicy | 18+, spicy opted in | Same as above | Base, adult, plus cheeky innuendo ("BUY ME DINNER FIRST.", "UPSIDE DOWN? KINKY.") | "WELL HELLO, {N}. ;)" |

The age gate is self-declared. Under-18 profiles never draw from the adult or spicy pools (`pickPool()`). Spicy lines are innuendo, not explicit, and the opt-in screen warns that it may say them in public. Hold BOOT for 2 s to redo onboarding and change the profile.

### Charge LEDs

The board shows charge state in hardware, and the lights glow through a translucent case:

| Red (PWR) | Green (CHG) | Meaning |
|---|---|---|
| On | On | Charging |
| On | Off | Fully charged (or no battery connected) |
| Off | Off | Running on battery |

The red LED runs from USB 5 V. The green LED is driven by the ETA6098's STAT pin, which isn't wired to the ESP32. To show **FULL** on screen and get an "ALL CHARGED UP!" reaction, fit this mod and set `CHG_SENSE_PIN 7` in `doobie.ino`:

- STAT (the CHG LED cathode) to a 100 kΩ resistor to GPIO7
- GPIO7 to a 100 kΩ resistor to GND

**Charge current warning (V1 boards):** R11 = 82 kΩ sets the charger to 2 A, far too much for a 150 mAh cell. Replace it with about 1.2 MΩ (~0.14 A). V1.1 boards ship with 360 kΩ (0.5 A).

### Optional microphone

The board has no mic. Wire an Adafruit PDM MEMS mic breakout (#3492, MP34DT01-M):

| Mic pin | Board pin |
|---|---|
| 3V | 3V3 |
| GND | G |
| CLK | GPIO5 |
| DAT | GPIO6 |

The firmware detects it at boot; the serial log shows `Mic: detected`. The level thresholds (`MIC_*_DB`) are first guesses based on the mic's datasheet sensitivity, so tune them against the `mic=…dB` value in the serial log.
- **Settings:** triple-tap the screen anywhere for a tile menu with light/dark mode, spicy on/off (adults), redo onboarding, power off, and exit. It closes on its own after 10 s idle.
- **On/off:** hold BOOT for 2 s, or pick *Power off* in settings. It says goodnight, its eyes close, the picture collapses like an old TV, and it goes into deep sleep. Press BOOT (or RST) to turn it back on. It also switches itself off below 3.3 V on battery to protect the cell. While it's off, the battery still charges over USB.
- **BOOT button:** a short press toggles light and dark.
- **Board revision:** V1 and V1.1 are detected at boot (V1.1 has a pull-up on GPIO18) and the LCD CS pin is set to match. The boot log also prints an I2C scan.

## Case

`cad/` holds a parametric CadQuery model with STEP, STL and 3MF exports. See `cad/README.md` for the parts list and print settings.

## Simulator

`tools/eyes-sim.html` runs the eye engine in a browser: tilt pad, sensor-event buttons, and click or hold on the screen.

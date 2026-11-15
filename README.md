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

- **Profile questions** during onboarding: *he / she / skip*, then *under 18 / 18+*. Adults then pick an attitude: *Pet*, *Cute* or *Spicy*. Quotes and Events can be chosen later in settings.
- **Speech:** about 400 short lines (`phrases.h`), typed out in a 5x7 dot font and dissolved away. Lines come in tiers (base, kid, pet, adult, spicy), and `{N}` is swapped for a pet name that suits the profile. It avoids repeating recent lines and makes the odd unprompted remark.
- **Mic reactions (optional add-on):** claps, bangs, sustained noise, music or a party, chatter and long silences each get their own lines, and the eyes bounce with loudness.
- **Power saving** for the 150 mAh cell: about 12 fps while asleep, the panel switches off after 5 minutes asleep, and motion wakes it.

### User journeys

| Journey | Profile | Pet names | Lines it can say | Onboarding ends with |
|---|---|---|---|---|
| Kid | Under 18 | BUDDY, CHAMP, FRIEND, PAL | Base, kid ("HIGH FIVE!", "HOMEWORK DONE?") and a little pet attitude | "LET'S BE FRIENDS, BUDDY!" |
| Pet | 18+, attitude: Pet | HUMAN, BUDDY, PAL | Base and pet attitude: cat/dog energy, no flirting ("FEED ME?", "I'M THE BOSS.", "*PURR*") | "YOU MAY PET ME NOW." |
| Cute | 18+, attitude: Cute | He: HANDSOME, TIGER… / She: GORGEOUS, QUEEN… / Skip: CUTIE, STAR… | Base, plus flirty adult lines ("OH, NICE FINGER.", "I LIKE THAT.") | "NICE TO MEET YOU, {N}." |
| Spicy | 18+, attitude: Spicy | Same as Cute | Base, adult, plus cheeky innuendo ("BUY ME DINNER FIRST.", "UPSIDE DOWN? KINKY.") | "WELL HELLO, {N}. ;)" |

There are about 400 lines across the pools. Change the vibe at any time in settings (triple-tap).

The age gate is self-declared. Under-18 profiles never draw from the adult or spicy pools (`pickPool()`). Spicy lines are innuendo, not explicit.

**Pacing:** idle remarks come every 28-69 s. Taps get a spoken reply about two times in three. Low-stakes reactions wait 2.5 s after the previous message, and the eyes stay below the text while Doobie is talking.

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
- **Settings:** tap the screen **4 times within 1 second**. Page 1 has Focus, Time, Theme and Attitude, each opening its own submenu. Page 2 has Brightness (25 / 50 / 75 / 100 %, saved), Profile (redo onboarding), Power off, Back and Exit. The menu closes on its own after 10 s idle.
- **Attitude:** *Settings → Attitude…* picks how Doobie talks:
  - **Pet:** cat/dog attitude, no flirting.
  - **Cute:** charming, never rude (18+).
  - **Spicy:** cheeky innuendo (18+).
  - **Quotes:** most idle remarks become quote cards, chosen from 107 short quotes by famous people with the attribution in orange.
  - **Events:** most idle remarks become history cards, chosen from 223 events, each with its month and year. When the clock is set, it prefers "THIS MONTH IN 19xx".

  Kid profiles only see Pet, Quotes and Events. Picking Quotes or Events shows a sample straight away. The content lives in `firmware/doobie/quotes.h`.
- **Themes:** *Settings → Theme…* offers Light, Dark and Custom 1–3 (the active one is marked with `>`). Tapping a custom slot opens a two-step editor. First *Pick eye color*: a live preview, a dot-art colour wheel (hue around, saturation outward), a dark-to-bright slider, then SAVE. Then *Pick background* the same way, and SAVE stores and applies the theme. Custom backgrounds get the same gradient, glow, grain and dot grid as the built-in themes. Pick a near-black background to save battery. A short press on BOOT still flips between light and dark.
- **Clock:** the board has no RTC chip, so the time comes from the ESP32's own timer. It keeps running through deep sleep, but disconnecting the battery or pressing RST loses it. Set the time in *Settings → Time* (top third of the screen = +, bottom third = −, hold to repeat, tap the middle for next / save), or run `python3 tools/timesync.py /dev/cu.usbmodem…` over USB. The time shows in the bottom-left corner. Mornings (6–10) bring morning lines; nights (22–05) bring bedtime lines, and Doobie gets sleepy twice as fast.
- **Pomodoro:** *Settings → Focus…* opens a submenu with Start/Stop, focus length (15/20/25/30/45/50 min), break length (3/5/10), long break every 4th round (10/15/20/30), and Auto (whether the next round starts by itself after a break). All of these are saved. The defaults are 25 / 5 / 15 with Auto on. During focus the eyes squint, it stays quiet and awake, and taps get a "SHH. FOCUS.". The bottom-left shows `FOCUS mm:ss` over a shrinking orange bar. It announces each break and each return to work. Stop it from the submenu.
- **On/off:** hold BOOT for 2 s, or pick *Power off* in settings. It says goodnight, its eyes close, the picture collapses like an old TV, and it goes into deep sleep. Press BOOT (or RST) to turn it back on. It also switches itself off below 3.3 V on battery to protect the cell. While it's off, the battery still charges over USB.
- **BOOT button:** a short press toggles light and dark.
- **Board revision:** V1 and V1.1 are detected at boot (V1.1 has a pull-up on GPIO18) and the LCD CS pin is set to match. The boot log also prints an I2C scan.

## Case

`cad/` holds a parametric CadQuery model with STEP, STL and 3MF exports. See `cad/README.md` for the parts list and print settings.

## Simulator

`tools/eyes-sim.html` runs the eye engine in a browser: tilt pad, sensor-event buttons, and click or hold on the screen.

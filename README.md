# Doobie Companion

A pocket-sized companion with dot-matrix eyes. It runs on a 1.64" AMOLED ESP32-S3 board and reacts to motion, touch and (optionally) sound. It talks back in tiny snippets, quotes and history cards, and has a handful of personalities. A magnetic 3D-printed case lets it clip onto wearables or a hairband, and the case has a hidden keyring bar.

**Firmware 1.9** · Arduino / ESP32-S3 · 280×456 AMOLED · ~1,500 lines of things to say

---

## Contents

- [Repository](#repository)
- [Hardware](#hardware)
- [Build and flash](#build-and-flash)
- [Using Doobie](#using-doobie)
- [Settings](#settings)
- [Personality](#personality)
- [Eyes and reactions](#eyes-and-reactions)
- [Themes](#themes)
- [Clock and pomodoro](#clock-and-pomodoro)
- [Power and battery](#power-and-battery)
- [LEDs and the LED mod](#leds-and-the-led-mod)
- [Optional microphone](#optional-microphone)
- [Case and hairband](#case-and-hairband)
- [Tools](#tools)
- [Known limitations](#known-limitations)
- [Version history](#version-history)

---

## Repository

| Path | What's in it |
|---|---|
| `firmware/doobie/doobie.ino` | Main sketch: display, sensors, behaviour, onboarding, settings, power |
| `firmware/doobie/phrases.h` | Reaction lines by event and tier (base, pet, kid, adult) |
| `firmware/doobie/chars.h` | Character packs (Bossy, Refined, Balkan). **Generated, don't edit** |
| `firmware/doobie/quotes.h` | 311 quotes and 421 dated history events |
| `firmware/doobie/eyetypes.h` | Shared types: moods, onboarding steps, menu choices |
| `tools/gen_chars.py` | Source of the character lines; writes `chars.h` |
| `tools/timesync.py` | Sets Doobie's clock from your computer over USB |
| `tools/eyes-sim.html` | Browser preview of the eye engine (early 0.2 look) |
| `cad/` | Case and hairband (CadQuery), STEP/STL/3MF, renders, `cad/README.md` |

---

## Hardware

| Part | Detail |
|---|---|
| Board | Waveshare **ESP32-S3-Touch-AMOLED-1.64**, V1 (V1.1 auto-detected) |
| MCU | ESP32-S3R8, 16 MB flash, 8 MB OPI PSRAM |
| Display | 280×456 AMOLED, QSPI, CO5300 command set, 20 px column offset |
| Motion | QMI8658 6-axis IMU, I2C 0x6B |
| Touch | FT3168, I2C 0x38 (kept in active mode: reg 0x86 = 0) |
| Battery | EEMB **LP401730**, 3.7 V 150 mAh, 4×17×30 mm, JST 1.25 on the BAT socket |
| Charger | ETA6098, hardware-controlled. **See the charge-current warning below** |
| Buttons | BOOT (GPIO0), RST |
| Clock | No RTC chip; uses the ESP32's own timer |
| Mic | None on board. Optional PDM add-on |

**Pins:**

| Function | Pins |
|---|---|
| QSPI | CS 9 (V1.1: 46), CLK 10, D0–D3 11–14 |
| LCD reset | 21 |
| I2C | SDA 47, SCL 48 |
| Battery ADC | 4 |
| Optional mic | CLK 5, DAT 6 |
| LED mod | red 16, green 17, STAT 7 |

**Board quirks:**
- **Revision detection:** V1.1 has a 4.7 k pull-up on GPIO18 (touch interrupt). The firmware probes for it at boot and picks LCD CS 9 or 46.
- **Battery sense:** on V1 the battery-sense divider sits on VCC, not the battery. It reads ~4.7 V on USB, and the firmware uses that to detect USB power. On battery it reads the cell.
- **IMU axes vs screen:** +X = down, +Y = right, +Z = out of the screen.

---

## Build and flash

```sh
arduino-cli core install esp32:esp32@3.1.3
arduino-cli lib install "GFX Library for Arduino@1.5.6"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile -b "$FQBN" firmware/doobie
arduino-cli upload  -b "$FQBN" -p /dev/cu.usbmodemXXXX firmware/doobie
python3 tools/timesync.py /dev/cu.usbmodemXXXX      # flashing resets the clock
```

- **Character lines:** after editing them, run `python3 tools/gen_chars.py`. It refuses to write unless each character has exactly 200 lines, all ASCII, ≤36 characters, with no duplicates.
- **Serial log:** 115200 baud. It prints the board revision, I2C scan, last reset reason, profile, free memory and microphone status at boot, then a status line every 0.5 s.
- **Memory use:** ~595 KB of the 3 MB app slot (18 %), 316 KB internal RAM free, 6.9 MB PSRAM free. The 9.9 MB FAT partition is unused.

---

## Using Doobie

| Gesture | Effect |
|---|---|
| **Tap** | Happy squint and blush (it replies about 2 times in 3) |
| **Hold** (0.7 s) | Hearts, and the LED dance with the mod |
| **4 taps within 1 s** | Opens settings |
| **Tilt** | Eyes look toward the low side |
| **Jerk or wiggle hard** | Jelly eyes: they lag, squash against the edges and bounce back |
| **Long, hard shake** | Dizzy spirals, then grumpy |
| **Bump / drop** | Surprised / panic |
| **Face down** | Annoyed |
| **Upside down, held steady 1.5 s** | The view rotates 180° |
| **Screen off: double-tap, 3 firm shakes, or BOOT** | Wakes up |
| **BOOT short press** | Toggles light/dark, or wakes the screen |
| **BOOT hold 2 s** | Power off (deep sleep); BOOT or RST turns it back on |

### Onboarding

This runs on first boot, or from *Settings → Profile*:
1. **Boot:** a dot grid sweeps on and the DOOBIE wordmark types in.
2. **Hello:** the eyes open and introduce themselves.
3. **01 Mode:** light or dark.
4. **02 Who are you?** He / she / skip. This only affects pet names.
5. **03 Attitude:** Pet / Bossy / Refined / Balkan.
6. **04 Tilt me**, then **05 Tap me**.

There's no age question. Everything is saved in NVS.

---

## Settings

Open with **4 taps within 1 s**. Rows are tiles: tap one to pick it, or tilt to move the highlight. The menu closes after 10 s idle, and every value is saved.

| Page | Rows |
|---|---|
| **1/2** | **FOCUS…** · **TIME** (set the clock) · **THEME…** · **ATTITUDE…** · MORE… · EXIT |
| **2/2** | **BRIGHT** 25/50/75/100 · **AOD** on/off · **ROTATE** on/off · PROFILE (redo onboarding) · POWER OFF · BACK |
| **FOCUS** | START/STOP · FOCUS 15–50 min · BREAK 3/5/10 · LONG 10/15/20/30 · AUTO on/off · BACK |
| **THEME** | LIGHT · DARK · CUSTOM 1 · CUSTOM 2 · CUSTOM 3 · BACK |
| **ATTITUDE** (2 pages) | PET · CUTE · QUOTES · EVENTS · BOSSY · REFINED · BALKAN |

**Setting the time:** tap the top third of the screen for +, the bottom third for −, and hold to repeat. Tap the middle for hours → minutes → save.

---

## Personality

| Attitude | How it talks | Calls you | Lines |
|---|---|---|---|
| **Pet** | Cat and dog energy: "FEED ME?", "*PURR*", "I'M THE BOSS." | HUMAN, BUDDY, PAL | Base, pet and a little encouragement |
| **Bossy** | A CEO with a stopwatch: "CHOP CHOP.", "PER MY LAST MESSAGE…" | INTERN, ROOKIE | 200 + base |
| **Refined** | Victorian English: "GOOD HEAVENS!", "MILK FIRST? SCANDAL." | GOOD SIR / MY LADY / DEAR FRIEND | 200 + base |
| **Balkan** | Rowdy, friendly, laid back: "AJDE, COFFEE?", "PROMAJA WILL KILL US!" | BRATE / SESTRO / LEGENDO | 200 + base |
| **Quotes** | Every idle remark becomes a quote card | — | 311 quotes |
| **Events** | Every idle remark becomes a history card ("THIS MONTH IN 1989" when the clock is set) | — | 421 events |
| **Cute** | Mildly flirty: "OH, NICE FINGER.", "I LIKE THAT." | HANDSOME / GORGEOUS / CUTIE | Base + flirty. **Settings only, after a one-time "I'M 18+" confirmation** |

- **Kid-friendly by default.** The spicy tier was removed in 1.2. Cute is the only adult-leaning option. It sits behind a self-declared 18+ confirmation, and redoing onboarding locks it again.
- **Situational lines:** morning (6–10) and night (22–05) lines, USB plug/unplug, low battery, focus/break, and microphone reactions.
- **Pacing:** idle remarks come every 28–69 s, or 20–45 s in card modes. Low-stakes chatter waits 2.5 s after the previous message, and recently used lines aren't repeated.
- **Cards:** for a quote or event, the eyes shrink smoothly into the bottom third and the text fills the top two thirds at the largest font that fits. All 732 layouts were simulated with no overflow.
- **Accuracy:** quotes and dates were written from memory and not checked against sources. Known misattributions were left out. Corrections go in `quotes.h`.

---

## Eyes and reactions

- **Rendering:** the eyes are signed-distance shapes sampled on a 9 px dot grid, so their edges render as halftone dots. Moods blend by tweening the shape (size, corner radius, lids, slant), and the gaze moves on a spring.
- **Moods:** neutral, happy, sleepy, angry, sad, surprised, suspicious, panic, focus. There are also hearts, dizzy spirals and sleeping lines.
- **Idle life:** blinks (sometimes doubled), glances, eye-rolls, winks, cross-eyes and suspicious side-eye.
- **Gaze:** left/right follows tilt. Up/down is relative to how you're holding Doobie and re-centres after a few seconds.
- **Jelly eyes:** linear acceleration drives a bouncy spring. The effect starts at ~0.35 g and is at full strength by 1.1 g. Eyes that hit an edge squash (down to 30 %), the other eye piles in, and the squashed axis bulges the other way.
- **Auto-rotate:** flips 180° only when Doobie is clearly upside down (~50°), held fairly vertical (not flat) and steady for 1.5 s, with a 3 s cooldown. Drawing, touch and gaze all follow. If it flips the wrong way, set `UPRIGHT_SIGN` to `-1.0f`. Landscape isn't supported.

---

## Themes

- **Built-in:** **Light** (bright teal gradient, film grain, a faint unlit dot grid) and **Dark** (the same, near-black, so most AMOLED pixels are off).
- **Custom 1–3:** a two-step editor.
  1. **Pick eye color:** a live preview, a dot-art colour wheel (hue around, saturation outward), a dark-to-bright slider, then SAVE.
  2. **Pick background:** the same layout, then SAVE, which stores and applies the theme.

  Custom backgrounds get the same gradient, glow, grain and dot grid.

---

## Clock and pomodoro

- **Clock:** there's no RTC, so the ESP32 keeps time. It runs through screen sleep and deep sleep, but **RST, flashing or a battery disconnect** lose it.
  - **Setting it:** in settings, or with `tools/timesync.py`.
  - **Display:** bottom-left of the main view.
- **Pomodoro:** focus / break / long break every 4th round, all configurable, plus Auto-continue.
  - **During focus:** the eyes take on a determined squint, Doobie stays quiet and awake, and taps get "SHH. FOCUS.".
  - **Display:** `FOCUS mm:ss` with a shrinking orange bar.
  - **Transitions:** each change is announced and wakes the screen.

---

## Power and battery

| State | What's on | How it's entered | How it's left |
|---|---|---|---|
| **Awake** | Everything, ~30 fps | — | — |
| **Screen sleep** | Panel off, CPU at 80 MHz, touch and IMU polled, clock and timers run | 30 s with no touch, **no movement**, and nothing being said (it never cuts off mid-sentence) | Double-tap, 3 firm shakes, BOOT, a focus/break change |
| **AOD** (setting) | Screen never turns off; Doobie dozes dim with "zzz" | Settings → AOD | Any motion or tap |
| **Deep sleep** ("off") | Almost nothing | BOOT hold 2 s, Settings → Power off, or automatically below 3.3 V | BOOT or RST |

- **Charging:** the ETA6098 charges over USB-C in hardware, including while Doobie is off.
- **Display:** the bottom-right label shows **USB** (or **FULL** with the LED mod) when plugged in, and the battery % otherwise.
- **Battery %:** only meaningful on battery, because V1 can't measure the cell while on USB.

> ⚠️ **Charge current (V1 boards):** R11 = 82 kΩ sets the ETA6098 to **2 A**, roughly 13C for the 150 mAh cell. Replace R11 with **~1.2 MΩ (~0.14 A)**. V1.1 boards ship with 360 kΩ (0.5 A). Until then, only charge while you're around, and unplug it if the cell gets warm.

---

## LEDs and the LED mod

**Stock wiring (V1), from the schematic.** Neither LED is connected to a GPIO, so firmware cannot change them:

| LED | Wired to | Lit when |
|---|---|---|
| Red (PWR) | VCC → R3 2.2 kΩ → LED → GND | **Whenever the board has power** (USB or battery, even in deep sleep). Draws ~1 mA constantly |
| Green (CHG) | VBAT → R22 3 kΩ → LED → ETA6098 STAT | While charging |

**The mod.** Parts are 0402, so use a fine tip and flux:
1. **Red:** remove **R3**. Solder 1 kΩ from **GPIO16** to R3's LED-side pad.
   - Removing R3 alone just kills the red light and its 1 mA drain.
2. **Green:** remove **LED2**. Fit **GPIO17 → 1 kΩ → green LED → GND**.
3. **Charge sense:** wire **LED2's old cathode pad (STAT)** to **GPIO7**. The firmware reads it with the internal pull-up.
4. Set `#define LED_MOD 1` in `doobie.ino` and flash.

Swap R11 during the same session.

**Behaviour after the mod:**
- **Red:** on when USB is plugged in or the battery is below 10 %.
- **Green:** on only when the battery is full ("ALL CHARGED UP!").
- **Both:** a quick alternating dance when Doobie is super happy (held, a focus round done, fully charged).

---

## Optional microphone

Wire an Adafruit PDM MEMS mic breakout (#3492, MP34DT01-M). The case rev B has a pocket and sound port for it.

| Mic | Board |
|---|---|
| 3V | 3V3 |
| GND | G |
| CLK | GPIO5 |
| DAT | GPIO6 |

The firmware detects it at boot (`Mic: detected` / `Mic: none`) and reacts like this:
- **Claps and bangs:** startled.
- **Loud for 3 s:** "TOO LOUD!".
- **Loud for 9 s or more:** party mode.
- **Chatter:** "WHO'S TALKING?".
- **30 s of silence:** quiet lines.
- **Eyes:** bounce with loudness.

The thresholds (`MIC_*_DB`) are datasheet-based first guesses, so tune them against `mic=…dB` in the log. On arduino-esp32 3.1 with PSRAM, the I2S driver has to be initialised while PSRAM is briefly reserved; that's already handled in `micInit()`.

---

## Case and hairband

`cad/` holds the parametric CadQuery models (`case.py`, `hairband.py`), STEP/STL/3MF exports and renders.

- **Case rev B:** 32.7 × 47.6 × 15.7 mm.
  - **Mounting:** screws to the board's brass standoffs with 4 × M2×8 button heads.
  - **Battery:** LP401730 pocket, plus a mic shelf with a sound port.
  - **Magnets:** two Ø6×3 N52 magnets on SOLDR's 30 mm pitch, hidden behind 0.3 mm skins.
  - **Keyring:** a 2 mm bar on the back.
  - **Openings:** USB-C, RST and BOOT access.
- **Hairband:** prints flat (140 × 199 mm). A fin with matching magnets holds Doobie upright, and a ledge supports the case's bottom corners.
- **Material:** Bambu **translucent PETG** on the P1S, for both parts.

The BOM, print settings, magnet polarity and assembly steps are in `cad/README.md`.

---

## Tools

- **`gen_chars.py`:** edit the character lines (`BOSSY`, `POSH` → shown as REFINED, `BALKAN`), then run it to regenerate `chars.h`.
- **`timesync.py <port>`:** sends `T<local epoch>` over serial. The firmware also accepts that command from any serial terminal.
- **`eyes-sim.html`:** a browser simulator of the early (0.2) eye engine: tilt pad, sensor buttons, and tap/hold.
  - **Not covered:** the current menus, cards and characters.

---

## Known limitations

- **Touch direction:** the touch X axis hasn't been fully confirmed on hardware (it matters for the colour wheel). If the selection marker moves mirrored, flip X in `touchRead()`.
- **Clock:** lost on RST or a battery disconnect. Wi-Fi/NTP sync would fix that and isn't implemented.
- **LEDs:** firmware control and "battery full" need the LED mod.
- **Display:** landscape isn't supported.
- **Battery life:** not measured. Roughly 1.5–2 h with the screen always on; much longer with screen sleep.

---

## Version history

| Version | Highlights |
|---|---|
| 0.2 | Dot-art eyes, grainy teal theme, onboarding, light/dark |
| 0.3 | Profile journeys, tiered phrases, optional mic, USB detection |
| 0.4–0.5 | Measured tile layout, pet attitude, clock, pomodoro, 4-tap settings |
| 0.6–0.8 | Brightness, focus submenu, custom themes with a colour wheel |
| 0.9–1.0 | Attitude menu: quotes, events, and the Bossy / Refined / Balkan characters |
| 1.1 | Age question removed; Cute behind an 18+ confirmation |
| 1.2 | Big quote/event cards with shrinking eyes; spicy removed; EXQUISITE renamed REFINED |
| 1.3–1.5 | Screen sleep after 30 s, double-tap wake, AOD, fix for instant screen-off |
| 1.6 | Jelly eyes |
| 1.7–1.8 | Correct IMU axes, deliberate auto-rotate, shake to wake |
| 1.9 | Corrected LED wiring analysis; single-switch `LED_MOD` |

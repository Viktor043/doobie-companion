// ============================================================================
//  DOOBIE COMPANION, firmware 1.0
//  Board : Waveshare ESP32-S3-Touch-AMOLED-1.64, V1 or V1.1 (auto-detected), 280x456 AMOLED
//  MCU   : ESP32-S3R8, 16MB flash, 8MB OPI PSRAM
//  Sense : QMI8658 6-axis IMU (0x6B), FT3168 touch (0x38), battery ADC GPIO4,
//          BOOT button GPIO0
//
//  Build (arduino-cli):
//    FQBN esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB
//    lib  "GFX Library for Arduino"@1.5.6     core esp32:esp32@3.1.3
//
//  First boot runs onboarding: boot sweep, hello, light/dark choice, tilt and
//  profile questions (he/she/skip, under 18 / 18+, spicy opt-in for adults),
//  tilt and tap tutorial. The result is saved in NVS.
//  Optional mic: Adafruit PDM MEMS mic (#3492) on GPIO5 = CLK, GPIO6 = DAT,
//  auto-detected at boot. Without it the noise reactions are simply off.
//    4 taps in 1 s : settings (focus timer, clock, mode, vibe, profile, power)
//    BOOT short press : toggle light / dark (wakes it when off)
//    BOOT hold 2 s    : power off (deep sleep); press BOOT or RST to turn on
// ============================================================================
#include <Arduino_GFX_Library.h>
#include <font/glcdfont.h>   // classic 5x7 ASCII table, reused as a dot-matrix font
#include <Wire.h>
#include <Preferences.h>
#include <ESP_I2S.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <sys/time.h>
#include <time.h>
#include "eyetypes.h"
#include "phrases.h"
#include "quotes.h"
#include "chars.h"

#define FW_VERSION "1.0"

void powerOff(const char *msg);
extern OnbStep ob;

// ---------- pins (Waveshare 1.64; V1.1 moved LCD CS to GPIO46, see detectBoard) ----------
#define PIN_LCD_RST   21
#define PIN_QSPI_CS_V1    9
#define PIN_QSPI_CS_V11  46
#define PIN_TP_INT_V11   18   // V1.1 only: 4.7k pull-up to 3V3, unconnected on V1
#define PIN_QSPI_CLK  10
#define PIN_QSPI_D0   11
#define PIN_QSPI_D1   12
#define PIN_QSPI_D2   13
#define PIN_QSPI_D3   14
#define PIN_SDA       47
#define PIN_SCL       48
#define PIN_BAT_ADC    4
#define PIN_BOOT       0

#define W 280
#define H 456
#define COL_OFFSET 20      // Waveshare driver: x + 0x14

#define ADDR_TOUCH 0x38
uint8_t ADDR_IMU = 0x6B;

#define GAZE_SIGN_X  (+1.0f)
#define GAZE_SIGN_Y  (-1.0f)

Arduino_DataBus *bus = nullptr;     // created in setup() once the board revision is known
Arduino_CO5300 *panel = nullptr;
Arduino_Canvas *g = nullptr;
bool boardV11 = false;

// V1.1 routes TP_INT to GPIO18 with an external 4.7k pull-up; on V1 the pin is
// unconnected. With the internal pull-down on, V1.1 reads HIGH and V1 reads LOW.
bool detectBoardV11() {
  pinMode(PIN_TP_INT_V11, INPUT_PULLDOWN);
  delay(3);
  int hi = 0;
  for (int i = 0; i < 8; i++) { hi += digitalRead(PIN_TP_INT_V11); delayMicroseconds(200); }
  pinMode(PIN_TP_INT_V11, INPUT);
  return hi >= 6;
}
Preferences prefs;

static inline uint16_t rgb(uint8_t r, uint8_t gg, uint8_t b) {
  return ((r & 0xF8) << 8) | ((gg & 0xFC) << 3) | (b >> 3);
}
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float approach(float c, float t, float rate, float dt) { return c + (t - c) * (1.0f - expf(-rate * dt)); }
static uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }

// ============================================================================
//  Theme
// ============================================================================
bool darkMode = false;
uint16_t C_INK;                                 // eye / text dots
const uint16_t C_PINK   = rgb(255, 92, 140);
const uint16_t C_RED    = rgb(232, 30, 84);
const uint16_t C_ORANGE = rgb(255, 92, 24);     // accent: step markers, progress
const uint16_t C_TEAL   = rgb(40, 240, 212);

#define PITCH   9
#define DOT_R   3.9f
#define GRAIN_FRAMES 3
uint16_t *bgFrames[GRAIN_FRAMES];
uint16_t *bgBlack = nullptr;                    // boot sequence background

// Light: bright teal diagonal gradient + glow + film grain.
// Dark: the same composition, inverted to near-black with a faint teal glow, so
// almost every pixel is off (AMOLED power), and the dots glow teal instead of ink.
void buildBackground(uint16_t *b, bool dark, int f) {
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    float t = clampf((x * 0.35f + y) / (W * 0.35f + H), 0, 1);
    float gx = (x - W * 0.3f) / W, gy = (y - H * 0.22f) / H;
    float glow = expf(-(gx * gx + gy * gy) * 6.0f);
    float r, gg, bl, amp;
    if (!dark) {
      r = 18 - 18 * t + 70 * glow; gg = 236 + (168 - 236) * t + 18 * glow; bl = 214 + (178 - 214) * t + 30 * glow; amp = 34;
    } else {
      r = 2; gg = 8 + 22 * glow * (1 - t); bl = 9 + 20 * glow * (1 - t); amp = 7;
    }
    int dx = (x % PITCH) - PITCH / 2, dy = (y % PITCH) - PITCH / 2;
    if (dx * dx + dy * dy <= 2) {
      if (!dark) { r *= 0.88f; gg *= 0.9f; bl *= 0.9f; }
      else { gg += 14; bl += 13; r += 3; }
    }
    float n = ((hash32(x * 7919 + y * 104729 + f * 15485863) & 0xFF) / 255.0f - 0.5f) * amp;
    b[y * W + x] = rgb((uint8_t)clampf(r + n, 0, 255), (uint8_t)clampf(gg + n, 0, 255), (uint8_t)clampf(bl + n, 0, 255));
  }
}
// ---- Themes: 0 light, 1 dark, 2..4 custom 1-3 (eye colour + background colour)
uint8_t themeId = 0;
uint32_t customEye[3] = { 0x28F0D4, 0xFF5C8C, 0xFFC24B };
uint32_t customBg[3]  = { 0x06161C, 0x1A0B2E, 0x0E2216 };
static inline float lum(uint32_t c) { return 0.30f * ((c >> 16) & 255) + 0.59f * ((c >> 8) & 255) + 0.11f * (c & 255); }
static inline uint16_t rgb565(uint32_t c) { return rgb((c >> 16) & 255, (c >> 8) & 255, c & 255); }

uint32_t hsv2rgb(float h, float s, float v) {
  h = fmodf(h, 360.0f); if (h < 0) h += 360.0f;
  float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1)), m = v - c, r, g2, b;
  if (h < 60) { r = c; g2 = x; b = 0; } else if (h < 120) { r = x; g2 = c; b = 0; }
  else if (h < 180) { r = 0; g2 = c; b = x; } else if (h < 240) { r = 0; g2 = x; b = c; }
  else if (h < 300) { r = x; g2 = 0; b = c; } else { r = c; g2 = 0; b = x; }
  return ((uint32_t)((r + m) * 255) << 16) | ((uint32_t)((g2 + m) * 255) << 8) | (uint32_t)((b + m) * 255);
}
void rgb2hsv(uint32_t c, float &h, float &s, float &v) {
  float r = ((c >> 16) & 255) / 255.0f, g2 = ((c >> 8) & 255) / 255.0f, b = (c & 255) / 255.0f;
  float mx = fmaxf(r, fmaxf(g2, b)), mn = fminf(r, fminf(g2, b)), dd = mx - mn;
  v = mx; s = mx > 0 ? dd / mx : 0;
  if (dd == 0) h = 0;
  else if (mx == r) h = 60 * fmodf((g2 - b) / dd, 6);
  else if (mx == g2) h = 60 * ((b - r) / dd + 2);
  else h = 60 * ((r - g2) / dd + 4);
  if (h < 0) h += 360;
}

// Custom background: same composition as the built-in themes (diagonal falloff,
// soft glow, film grain, faint unlit dot grid) but tinted from any colour.
void buildBackgroundCustom(uint16_t *b, uint32_t c, int f) {
  const float R = (c >> 16) & 255, G = (c >> 8) & 255, B = c & 255, L = lum(c);
  const float amp = 6 + 28 * L / 255.0f;
  for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
    float t = clampf((x * 0.35f + y) / (W * 0.35f + H), 0, 1);
    float gx = (x - W * 0.3f) / W, gy = (y - H * 0.22f) / H;
    float glow = expf(-(gx * gx + gy * gy) * 6.0f), k = 1 - 0.32f * t;
    float r = R * k + (255 - R) * 0.22f * glow, gg = G * k + (255 - G) * 0.22f * glow, bl = B * k + (255 - B) * 0.22f * glow;
    int dx = (x % PITCH) - PITCH / 2, dy = (y % PITCH) - PITCH / 2;
    if (dx * dx + dy * dy <= 2) { if (L > 90) { r *= 0.88f; gg *= 0.88f; bl *= 0.88f; } else { r += 14; gg += 14; bl += 14; } }
    float n = ((hash32(x * 7919 + y * 104729 + f * 15485863) & 0xFF) / 255.0f - 0.5f) * amp;
    b[y * W + x] = rgb((uint8_t)clampf(r + n, 0, 255), (uint8_t)clampf(gg + n, 0, 255), (uint8_t)clampf(bl + n, 0, 255));
  }
}
void applyTheme(bool dark);
void applyThemeId(uint8_t id) {
  if (id <= 1) { applyTheme(id == 1); return; }
  themeId = id;
  uint32_t e = customEye[id - 2], bg = customBg[id - 2];
  C_INK = rgb565(e);
  darkMode = lum(bg) < 90;
  for (int f = 0; f < GRAIN_FRAMES; f++) buildBackgroundCustom(bgFrames[f], bg, f);
}
void saveTheme() { prefs.putUChar("theme", themeId); prefs.putBool("dark", darkMode); }

void applyTheme(bool dark) {
  themeId = dark ? 1 : 0;
  darkMode = dark;
  C_INK = dark ? C_TEAL : rgb(4, 38, 44);
  for (int f = 0; f < GRAIN_FRAMES; f++) buildBackground(bgFrames[f], dark, f);
}

// ============================================================================
//  Sensors
// ============================================================================
static void i2cWrite(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}
static bool i2cRead(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}
bool imuOk = false;
float ax, ay, az, gx, gy, gz;
void imuInit() {
  uint8_t who = 0;
  for (uint8_t a : {0x6B, 0x6A}) {
    for (int t = 0; t < 5 && who != 0x05; t++) { who = 0; i2cRead(a, 0x00, &who, 1); delay(10); }
    if (who == 0x05) { ADDR_IMU = a; break; }
  }
  imuOk = (who == 0x05);
  Serial.printf("QMI8658 @0x%02X %s\n", ADDR_IMU, imuOk ? "OK" : "MISSING");
  if (!imuOk) return;
  i2cWrite(ADDR_IMU, 0x02, 0x40);   // auto-increment, little endian
  i2cWrite(ADDR_IMU, 0x03, 0x15);   // accel ±4g
  i2cWrite(ADDR_IMU, 0x04, 0x55);   // gyro ±512 dps
  i2cWrite(ADDR_IMU, 0x08, 0x03);   // enable both
}
void imuRead() {
  if (!imuOk) { ax = ay = 0; az = 1; gx = gy = gz = 0; return; }
  uint8_t b[12];
  if (!i2cRead(ADDR_IMU, 0x35, b, 12)) return;
  int16_t r[6];
  for (int i = 0; i < 6; i++) r[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8));
  ax = r[0] / 8192.0f; ay = r[1] / 8192.0f; az = r[2] / 8192.0f;
  gx = r[3] / 64.0f;   gy = r[4] / 64.0f;   gz = r[5] / 64.0f;
}
bool touchDown = false; int touchX = 0, touchY = 0;
void touchRead() {
  uint8_t b[5];
  if (!i2cRead(ADDR_TOUCH, 0x02, b, 5)) { touchDown = false; return; }
  touchDown = (b[0] & 0x0F) > 0;
  if (touchDown) { touchX = ((b[1] & 0x0F) << 8) | b[2]; touchY = ((b[3] & 0x0F) << 8) | b[4]; }
}
// V1 board: the divider sits on the system rail (VCC), not VBAT. On USB it reads
// ~4.7 V; on battery VCC = VBAT through the AO3401 power-path FET, so it reads
// the cell. Charging is done in hardware by the ETA6098 (CHG LED); STAT isn't
// wired to a GPIO, so "on USB" is the best the firmware can know.
float batteryV() { return analogReadMilliVolts(PIN_BAT_ADC) * 3.0f / 1000.0f; }
float batV = 4.0f; bool onUsb = false;

// Optional charge-status mod: the ETA6098 STAT pin (= cathode of the green CHG
// LED) isn't wired to the ESP32. Solder STAT -> 100k -> GPIO7 and GPIO7 -> 100k
// -> GND, then set CHG_SENSE_PIN to 7. STAT pulls low while charging (~0 V) and
// floats up to VBAT through the LED when done, so the divider reads ~1.5-2.1 V.
#define CHG_SENSE_PIN  -1          // -1 = mod not fitted
bool chargeFull = false;
bool readChargeFull() {
#if CHG_SENSE_PIN >= 0
  return analogReadMilliVolts(CHG_SENSE_PIN) > 900;
#else
  return false;
#endif
}
// ---- Clock. No RTC chip on this board: the ESP32's own RTC timer keeps time
// while powered, including deep sleep. Lost on battery disconnect / RST.
// Stored as local wall time (treated as UTC internally; no timezone math).
// Set from settings, or over USB serial with "T<local epoch>\n".
#define CLOCK_MIN_VALID 1735689600L      // 2025-01-01
bool clockValid() { return time(nullptr) > CLOCK_MIN_VALID; }
void setClock(time_t t) { struct timeval tv = { t, 0 }; settimeofday(&tv, nullptr); }
bool localHM(int &h, int &m) {
  if (!clockValid()) return false;
  time_t t = time(nullptr); struct tm tmv; gmtime_r(&t, &tmv);
  h = tmv.tm_hour; m = tmv.tm_min; return true;
}
int localHour() { int h, m; return localHM(h, m) ? h : -1; }
bool isNight()   { int h = localHour(); return h >= 22 || (h >= 0 && h < 5); }
bool isMorning() { int h = localHour(); return h >= 6 && h < 10; }
void pollSerial() {
  static char buf[24]; static int n = 0;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      buf[n] = 0;
      if (buf[0] == 'T') {
        long long t = atoll(buf + 1);
        if (t > CLOCK_MIN_VALID) { setClock((time_t)t); int h, m; localHM(h, m); Serial.printf("clock set %02d:%02d\n", h, m); }
      }
      n = 0;
    } else if (n < 23) buf[n++] = c;
  }
}

int batteryPct() { return (int)clampf((batV - 3.30f) / (4.15f - 3.30f) * 100.0f, 0, 100); }

// --- PDM microphone (optional add-on). Level in dBFS, computed on core 0.
#define MIC_CLK   5
#define MIC_DATA  6
#define MIC_LOUD_DB  (-38.0f)   // sustained loud (~80 dB SPL with MP34DT01, -26 dBFS @ 94 dB)
#define MIC_QUIET_DB (-75.0f)   // near silence
#define MIC_BANG_DB  (-28.0f)   // clap / bang peak
I2SClass *micI2S = nullptr;   // must live in internal RAM (DMA callback context)
volatile float micDb = -90, micPeakDb = -90;
volatile bool micOk = false;
void micTask(void *) {
  static int16_t buf[256];
  float dc = 0; uint32_t start = millis(); bool decided = false;
  for (;;) {
    size_t n = micI2S->readBytes((char *)buf, sizeof(buf)) / 2;
    if (!n) { vTaskDelay(5); continue; }
    double acc = 0; int16_t mn = 32767, mx = -32768;
    for (size_t i = 0; i < n; i++) {
      dc += (buf[i] - dc) * 0.002f;
      float v = buf[i] - dc; acc += v * v;
      if (buf[i] < mn) mn = buf[i]; if (buf[i] > mx) mx = buf[i];
    }
    float db = 20.0f * log10f(sqrtf(acc / n) / 32768.0f + 1e-7f);
    micDb = db; if (db > micPeakDb) micPeakDb = db;
    if (!decided) {
      if (mx - mn > 8 && millis() - start > 300) { micOk = true; decided = true; Serial.println("Mic: detected"); }
      else if (millis() - start > 2500) {
        decided = true;
        Serial.println("Mic: none (wire a PDM mic: GPIO5 CLK, GPIO6 DAT)");
        micI2S->end(); vTaskDelete(NULL);
      }
    }
  }
}
void micInit() {
  void *mem = heap_caps_malloc(sizeof(I2SClass), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!mem) return;
  micI2S = new (mem) I2SClass();
  micI2S->setPinsPdmRx(MIC_CLK, MIC_DATA);
  // arduino-esp32 3.1 + PSRAM: the I2S driver allocates its channel object with
  // MALLOC_CAP_DEFAULT, which can land in PSRAM, and the GDMA ISR then rejects
  // it ("user context not in internal RAM"). Workaround: briefly reserve all
  // free PSRAM so that allocation has to come from internal RAM, then release.
  void *hold[96]; int nh = 0;
  while (nh < 96) {
    size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if (big < 16) break;
    hold[nh] = heap_caps_malloc(big, MALLOC_CAP_SPIRAM);
    if (!hold[nh]) break;
    nh++;
  }
  bool ok = micI2S->begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  while (nh) heap_caps_free(hold[--nh]);
  if (!ok) { Serial.println("Mic: I2S init failed"); return; }
  gpio_pulldown_en((gpio_num_t)MIC_DATA);   // no mic -> constant bitstream -> detected as absent
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr, 1, nullptr, 0);
}

// ============================================================================
//  Dot rendering
// ============================================================================
static inline float sdBox(float px, float py, float hw, float hh, float r) {
  float qx = fabsf(px) - (hw - r), qy = fabsf(py) - (hh - r);
  float ox = fmaxf(qx, 0), oy = fmaxf(qy, 0);
  return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - r;
}
static inline float sdEllipse(float px, float py, float rx, float ry) {
  return (sqrtf((px * px) / (rx * rx) + (py * py) / (ry * ry)) - 1.0f) * fminf(rx, ry);
}
static inline float sdCircle(float px, float py, float r) { return sqrtf(px * px + py * py) - r; }

template <typename F>
void dotField(float cx, float cy, float hw, float hh, uint16_t col, F sdf) {
  int x0 = ((int)(cx - hw) / PITCH) * PITCH + PITCH / 2, x1 = (int)(cx + hw) + PITCH;
  int y0 = ((int)(cy - hh) / PITCH) * PITCH + PITCH / 2, y1 = (int)(cy + hh) + PITCH;
  for (int y = max(y0, PITCH / 2); y < min(y1, H); y += PITCH)
    for (int x = max(x0, PITCH / 2); x < min(x1, W); x += PITCH) {
      float c = clampf(0.5f - sdf(x - cx, y - cy) / (PITCH * 1.1f), 0, 1);
      if (c <= 0.08f) continue;
      float r = DOT_R * sqrtf(c);
      if (r < 1.2f) g->fillRect(x, y, 2, 2, col);
      else g->fillCircle(x, y, (int)(r + 0.5f), col);
    }
}

// --- dot-matrix text (5x7 glcdfont, column-major, bit0 = top row)
static void dotDot(int x, int y, float r, uint16_t col) {
  if (r < 1.1f) g->drawPixel(x, y, col);
  else if (r < 1.6f) g->fillRect(x, y, 2, 2, col);
  else g->fillCircle(x, y, (int)(r + 0.4f), col);
}
// Draws text. visible = chars shown (typewriter), dissolve 0..1 eats dots randomly.
// align: 0 left, 1 centre. Returns the height used.
int dotText(const char *s, int visible, int x, int y, int pitch, float r, uint16_t col,
            int align = 1, float dissolve = 0, int maxWidth = W - 32) {
  const int adv = 6 * pitch;
  const int maxChars = max(1, maxWidth / adv);
  int len = strlen(s), shown = 0, line = 0, i = 0;
  while (i < len) {
    // word wrap
    int end = min(len, i + maxChars);
    if (end < len) { int sp = end; while (sp > i && s[sp] != ' ') sp--; if (sp > i) end = sp; }
    int n = end - i;
    int lx = align ? x - (n * adv - pitch) / 2 : x;
    int ly = y + line * (8 * pitch + pitch * 2);
    for (int k = 0; k < n; k++, shown++) {
      if (shown >= visible) return (line + 1) * (10 * pitch);
      uint8_t ch = (uint8_t)s[i + k];
      for (int c = 0; c < 5; c++) {
        uint8_t bits = font[ch * 5 + c];
        for (int rr = 0; rr < 7; rr++) if (bits & (1 << rr)) {
          if (dissolve > 0 && (hash32((i + k) * 131 + c * 17 + rr * 7) & 0xFF) / 255.0f < dissolve) continue;
          dotDot(lx + k * adv + c * pitch, ly + rr * pitch, r, col);
        }
      }
    }
    i = end; while (i < len && s[i] == ' ') { i++; shown++; }
    line++;
  }
  return line * (10 * pitch);
}

// Line count dotText() would use for s at this pitch/width (same wrap rules).
int textLines(const char *s, int pitch, int maxWidth) {
  const int maxChars = max(1, maxWidth / (6 * pitch));
  int len = strlen(s), i = 0, lines = 0;
  while (i < len) {
    int end = min(len, i + maxChars);
    if (end < len) { int sp = end; while (sp > i && s[sp] != ' ') sp--; if (sp > i) end = sp; }
    i = end; while (i < len && s[i] == ' ') i++;
    lines++;
  }
  return lines;
}
int textHeight(int lines, int pitch) { return lines ? lines * 10 * pitch - 3 * pitch : 0; }

// ============================================================================
//  Speech
// ============================================================================
const char *speechText = nullptr;
uint32_t speechEndsAt = 0;
uint32_t speechStart = 0, nextIdleTalk = 25000;
uint16_t recent[8]; uint8_t recentHead = 0;  // recently used lines (pointer hash) to avoid repeats

uint8_t speechKind = 0;          // 0 normal line, 1 quote card, 2 event card
char speechSub[40];               // card attribution / date line
uint32_t speechDuration(const char *s) {
  if (speechKind) return min(14000UL, (unsigned long)(strlen(s) * 60 + 3200));   // cards: time to read
  return strlen(s) * 38 + 1700 + 380;
}
bool speaking() { return speechText && millis() - speechStart < speechDuration(speechText); }

// ---------- user profile (set during onboarding) ----------
uint8_t profSex = 2;            // 0 he, 1 she, 2 skip
bool profAdult = false;         // under-18 profiles never see ADULT/SPICY lines
// Attitude: 0 pet, 1 cute (flirty), 2 spicy, 3 quotes, 4 events,
//           5 bossy, 6 exquisite (Victorian), 7 balkan. Kids: everything but 1 and 2.
uint8_t profTone = 0;
#define profSpicy (profAdult && profTone == 2)
#define profFlirt (profAdult && (profTone == 1 || profTone == 2))
const char *const TONE_NAME[8] = { "PET", "CUTE", "SPICY", "QUOTES", "EVENTS", "BOSSY", "EXQUISITE", "BALKAN" };
#define TONE_COUNT 8
char speechBuf[128];             // quotes run up to ~90 chars

const char *petName() {
  if (profTone >= 5) return CHAR_PET[profTone - 5][profAdult ? min((int)profSex, 2) : 2][random(4)];
  if (!profAdult)   return PET_KID[random(4)];
  if (!profFlirt) return PET_REG[random(4)];
  if (profSex == 0) return PET_MALE[random(4)];
  if (profSex == 1) return PET_FEMALE[random(4)];
  return PET_ANY[random(4)];
}
void sayText(const char *s) {                 // expands {N} to a pet name
  char *o = speechBuf; const char *p = s;
  while (*p && o < speechBuf + sizeof(speechBuf) - 12) {
    if (p[0] == '{' && p[1] == 'N' && p[2] == '}') { const char *n = petName(); while (*n) *o++ = *n++; p += 3; }
    else *o++ = *p++;
  }
  *o = 0;
  speechKind = 0;
  speechText = speechBuf; speechStart = millis(); speechEndsAt = speechStart + speechDuration(speechBuf);
}
// Kids: base + kid + some pet. Regular: base + pet. Flirty: + adult. Spicy: + spicy.
const Pool *pickPool(Ev e) {
  const EvPools &P = EVENTS[e];
  int roll = random(100);
  if (profTone >= 5) {                          // character packs override, base fills gaps
    const Pool *cp = &CHARS[profTone - 5][e];
    if (cp->n && roll < (e == EV_IDLE ? 88 : 78)) return cp;
    return &P.base;
  }
  if (!profAdult) {
    if (e == EV_IDLE && roll < 35) return &KID_IDLE;
    if (P.pet.n && roll >= 70) return &P.pet;
    return &P.base;
  }
  if (profSpicy && P.spicy.n && roll < 40) return &P.spicy;
  if (profFlirt && P.adult.n && roll >= 40 && roll < 70) return &P.adult;
  if (P.pet.n && roll >= (profFlirt ? 85 : 50)) return &P.pet;
  return &P.base;
}
// Frequent, low-stakes reactions don't always talk, and wait for a short pause
// after the previous message ("breathing room"). Big events always speak.
bool chattyEvent(Ev e) { return e == EV_TAP || e == EV_LOVE || e == EV_TALK || e == EV_IDLE || e == EV_SUS || e == EV_ROLL || e == EV_CROSS || e == EV_WINK; }
// Quote / event cards (QUOTES and EVENTS attitudes)
uint16_t lastCard[6]; uint8_t lastCardHead = 0;
uint16_t pickFresh(uint16_t n, uint16_t salt) {
  uint16_t i = random(n);
  for (int t = 0; t < 8; t++) {
    bool used = false;
    for (uint16_t k : lastCard) if (k == (uint16_t)(i ^ salt)) used = true;
    if (!used) break;
    i = random(n);
  }
  lastCard[lastCardHead++ % 6] = i ^ salt;
  return i;
}
void showCard(uint8_t kind, const char *text, const char *sub) {
  strncpy(speechBuf, text, sizeof(speechBuf) - 1); speechBuf[sizeof(speechBuf) - 1] = 0;
  strncpy(speechSub, sub, sizeof(speechSub) - 1); speechSub[sizeof(speechSub) - 1] = 0;
  speechKind = kind; speechText = speechBuf; speechStart = millis();
  speechEndsAt = speechStart + speechDuration(speechBuf);
}
void sayQuote() {
  const Quote &q = QUOTES[pickFresh(QUOTE_COUNT, 0x5100)];
  char who[40]; snprintf(who, sizeof(who), "- %s", q.who);
  showCard(1, q.text, who);
}
void sayEvent() {
  // with a set clock, prefer "this month in history"
  int month = -1;
  if (clockValid()) { time_t t = time(nullptr); struct tm tmv; gmtime_r(&t, &tmv); month = tmv.tm_mon + 1; }
  uint16_t idx = 0; bool thisMonth = false;
  if (month > 0 && random(100) < 60) {
    uint16_t hits[48]; int nh = 0;
    for (uint16_t i = 0; i < HIST_COUNT && nh < 48; i++) if (HIST[i].month == month) hits[nh++] = i;
    if (nh) { idx = hits[pickFresh(nh, 0xE700 + month)]; thisMonth = true; }
  }
  if (!thisMonth) idx = pickFresh(HIST_COUNT, 0xE000);
  const HistEvent &e = HIST[idx];
  char date[40];
  if (thisMonth) snprintf(date, sizeof(date), "THIS MONTH IN %u", e.year);
  else snprintf(date, sizeof(date), "%s %u", MONTHS3[e.month - 1], e.year);
  showCard(2, e.title, date);
}

void say(Ev e, bool force = true) {
  if (!force && speaking()) return;
  if (chattyEvent(e)) {
    if (speaking() || millis() < speechEndsAt + 2500) return;
    if (e == EV_TAP && random(100) >= 65) return;
  }
  const Pool *L = pickPool(e);
  uint8_t idx = random(L->n);
  for (int tries = 0; tries < 6; tries++) {
    uint16_t key = (uint16_t)((uintptr_t)L->items[idx] >> 2);
    bool used = false;
    for (uint16_t k : recent) if (k == key) used = true;
    if (!used) break;
    idx = random(L->n);
  }
  recent[recentHead++ & 7] = (uint16_t)((uintptr_t)L->items[idx] >> 2);
  // QUOTES / EVENTS attitudes: most idle remarks become cards
  if (e == EV_IDLE && (profTone == 3 || profTone == 4) && random(100) < 70) {
    if (profTone == 3) sayQuote(); else sayEvent();
    nextIdleTalk = speechEndsAt + random(28000, 69000);
    return;
  }
  sayText(L->items[idx]);
  nextIdleTalk = millis() + random(28000, 69000);   // +25% vs 0.3
}
void drawSpeech() {
  if (!speechText) return;
  uint32_t t = millis() - speechStart, total = speechDuration(speechText);
  if (t >= total) { speechText = nullptr; return; }
  int len = strlen(speechText);
  int visible = min(len, (int)(t / 38) + 1);
  float dissolve = t > total - 380 ? (t - (total - 380)) / 380.0f : 0;
  if (speechKind) {                                  // quote / event card
    int lines = textLines(speechText, 2, W - 36);
    int vis = min(len, (int)(t / 20) + 1);
    float dis = t > total - 380 ? (t - (total - 380)) / 380.0f : 0;
    if (speechKind == 2) dotText(speechSub, 99, W / 2, 34, 2, 1.0f, C_ORANGE, 1, dis, W - 36);
    int ty = speechKind == 2 ? 58 : 36;
    dotText(speechText, vis, W / 2, ty, 2, 1.0f, C_INK, 1, dis, W - 36);
    if (speechKind == 1 && vis >= len) dotText(speechSub, 99, W / 2, ty + textHeight(lines, 2) + 10, 2, 1.0f, C_ORANGE, 1, dis, W - 36);
    return;
  }
  int pitch = textLines(speechText, 4, W - 40) <= 2 ? 4 : 3;
  float rr = pitch == 4 ? 1.7f : 1.3f;
  int lines = textLines(speechText, pitch, W - 40);
  dotText(speechText, visible, W / 2, 40, pitch, rr, C_INK, 1, dissolve, W - 40);
  if (visible < len && (t / 120) % 2)   // typing cursor under the block
    g->fillRect(W / 2 - 4, 40 + textHeight(lines, pitch) + 6, 8, 3, C_ORANGE);
}

// ============================================================================
//  Eye model
// ============================================================================
EyeShape moodShape(Mood m, bool left) {
  switch (m) {
    case HAPPY:      return {120, 150, 50, 0.00f, 0.58f, 0.0f};
    case SLEEPY:     return {120, 140, 40, 0.55f, 0.10f, -0.15f};
    case ANGRY:      return {115, 140, 34, 0.38f, 0.00f, 0.75f};
    case SAD:        return {110, 140, 40, 0.30f, 0.00f, -0.70f};
    case SURPRISED:  return {135, 175, 66, 0.00f, 0.00f, 0.0f};
    case SUSPICIOUS: return left ? EyeShape{120, 140, 34, 0.50f, 0.20f, 0.2f}
                                 : EyeShape{115, 150, 40, 0.10f, 0.00f, 0.0f};
    case PANIC:      return {80, 95, 46, 0.00f, 0.00f, 0.0f};
    case FOCUS:      return {126, 118, 28, 0.22f, 0.10f, 0.10f};   // determined squint
    default:         return {115, 150, 40, 0.00f, 0.00f, 0.0f};
  }
}
EyeShape curL, curR;
Mood mood = NEUTRAL, baseMood = NEUTRAL;
uint32_t moodUntil = 0;
Special special = SP_NONE;
uint32_t specialUntil = 0;
float blush = 0, blushT = 0;
float gazeX = 0, gazeY = 0, gvX = 0, gvY = 0, glanceX = 0, glanceY = 0;
float converge = 0, convergeT = 0, rollPhase = -1;
float blinkT = -1; int blinkWhich = 0;
uint32_t nextBlink = 2000, nextGlance = 1500, nextTrick = 12000;
float eyeOpen = 1.0f;     // 0 = closed line (used by onboarding to open the eyes)
float audioPulse = 0;     // 0..1 loudness above ambient, makes the eyes bounce

static void tweenShape(EyeShape &c, const EyeShape &t, float dt, float rate) {
  c.w = approach(c.w, t.w, rate, dt); c.h = approach(c.h, t.h, rate, dt); c.r = approach(c.r, t.r, rate, dt);
  c.lidTop = approach(c.lidTop, t.lidTop, rate, dt); c.lidBot = approach(c.lidBot, t.lidBot, rate, dt);
  c.slant = approach(c.slant, t.slant, rate, dt);
}
void setMood(Mood m, uint32_t ms) { mood = m; moodUntil = ms ? millis() + ms : 0; }
void setSpecial(Special s, uint32_t ms) { special = s; specialUntil = millis() + ms; }

#define SCALE   0.72f
#define GAZE_KX 0.40f
#define GAZE_KY 0.70f

void drawEye(float cx, float cy, const EyeShape &e0, float squash, bool isLeft, float sc = SCALE, uint16_t col = 0xFFFF) {
  if (col == 0xFFFF) col = C_INK;
  EyeShape e = e0; e.w *= sc; e.h *= sc; e.r *= sc;
  float h = e.h * (1.0f - squash) + 7.0f * squash, w = e.w * (1.0f + 0.12f * squash);
  float r = fminf(e.r, fminf(w, h) * 0.5f);
  bool lid = e.lidTop > 0.01f || fabsf(e.slant) > 0.01f;
  float s = e.slant * h * 0.35f, base = -h / 2 + e.lidTop * h;
  float slope = (isLeft ? 2 * s : -2 * s) / w, norm = 1.0f / sqrtf(1 + slope * slope);
  float ery = h * 0.62f, erx = w * 0.85f, ecy = h / 2 + ery - e.lidBot * h * 1.15f;
  dotField(cx, cy, w / 2 + 4, h / 2 + 4, col, [&](float x, float y) {
    float d = sdBox(x, y, w / 2, h / 2, r);
    if (lid) d = fmaxf(d, ((base + slope * x) - y) * norm);
    if (e.lidBot > 0.01f) d = fmaxf(d, -sdEllipse(x, y - ecy, erx, ery));
    return d;
  });
}
void drawHeart(float cx, float cy, float u, uint16_t col) {
  const float k = 0.70710678f; float hs = u * 0.62f;
  dotField(cx, cy, u * 1.3f, u * 1.3f, col, [&](float x, float y) {
    float py = y - u * 0.15f, rx = (x + py) * k, ry = (py - x) * k;
    return fminf(sdBox(rx, ry, hs, hs, 2), fminf(sdCircle(x + hs * k, py + hs * k, hs), sdCircle(x - hs * k, py + hs * k, hs)));
  });
}
void drawSpiral(float cx, float cy, float t, float size) {
  const float b = size / (2.2f * PI);
  dotField(cx, cy, size, size, C_INK, [&](float x, float y) {
    float rr = sqrtf(x * x + y * y), th = atan2f(y, x) + t * 7.0f;
    float u = rr / (2 * PI * b) - th / (2 * PI), f = u - floorf(u);
    return fmaxf(fminf(f, 1 - f) * 2 * PI * b - 3.5f, rr - size);
  });
}
void drawZ(float x, float y, int sz) { g->setTextColor(C_INK); g->setTextSize(sz); g->setCursor(x, y); g->print("z"); }

void drawBackground() {
  static uint8_t gf = 0; static uint32_t gfT = 0;
  if (millis() - gfT > 90) { gfT = millis(); gf = (gf + 1) % GRAIN_FRAMES; }
  memcpy(g->getFramebuffer(), bgFrames[gf], W * H * 2);
}

void drawFace(float t) {
  const float cx = W / 2, cy = H / 2 + 22, sep = 66;
  float bob = sinf(t * 1.7f) * 3.0f - audioPulse * 12.0f, cv = converge * 0.6f;
  float lx = cx - sep + gazeX * GAZE_KX + cv, rx = cx + sep + gazeX * GAZE_KX - cv;
  float gy = gazeY * GAZE_KY;
  if (speechText) gy = fmaxf(gy, -24.0f);       // breathing room under the speech
  float ly = cy + gy + bob, ry = ly;
  float sqL = 1 - eyeOpen, sqR = 1 - eyeOpen;
  if (blinkT >= 0) {
    float p = blinkT / 0.16f, s = p < 1 ? sinf(p * PI) : 0;
    if (blinkWhich != 2) sqL = fmaxf(sqL, s);
    if (blinkWhich != 1) sqR = fmaxf(sqR, s);
  }
  if (special == SP_LOVE) {
    float p = 1.0f + 0.12f * sinf(t * 10.0f);
    drawHeart(lx, ly, 40 * p, C_RED); drawHeart(rx, ry, 40 * p, C_RED);
  } else if (special == SP_DIZZY) {
    drawSpiral(lx, ly, t, 46); drawSpiral(rx, ry, -t, 46);
  } else if (special == SP_ASLEEP) {
    dotField(lx, ly, 44, 8, C_INK, [](float x, float y) { return sdBox(x, y, 40, 4, 4); });
    dotField(rx, ry, 44, 8, C_INK, [](float x, float y) { return sdBox(x, y, 40, 4, 4); });
    for (float off : {0.0f, 0.5f}) {
      float zt = fmodf(t * 0.6f + off, 1.0f);
      if (zt < 0.85f) drawZ(rx + 20 + zt * 30, ry - 40 - zt * 90, 2 + (int)(zt * 4));
    }
  } else {
    drawEye(lx, ly, curL, sqL, true);
    drawEye(rx, ry, curR, sqR, false);
  }
  if (blush > 0.05f) {
    float ry2 = 12 * blush, rx2 = 30 * blush;
    dotField(lx - 8, ly + 82, rx2 + 4, ry2 + 4, C_PINK, [&](float x, float y) { return sdEllipse(x, y, rx2, ry2); });
    dotField(rx + 8, ry + 82, rx2 + 4, ry2 + 4, C_PINK, [&](float x, float y) { return sdEllipse(x, y, rx2, ry2); });
  }
}

// ============================================================================
//  Pomodoro: 25 min focus / 5 min break, every 4th break 15 min.
// ============================================================================
enum : uint8_t { POMO_OFF, POMO_FOCUS, POMO_BREAK };
uint8_t pomo = POMO_OFF, pomoCount = 0;
uint32_t pomoEnd = 0, pomoLen = 0;
// user-adjustable in Settings > Focus (saved in NVS)
const uint8_t FOCUS_OPTS[] = { 15, 20, 25, 30, 45, 50 };
const uint8_t BREAK_OPTS[] = { 3, 5, 10 };
const uint8_t LONG_OPTS[]  = { 10, 15, 20, 30 };
uint8_t focusIdx = 2, breakIdx = 1, longIdx = 1;
bool pomoAuto = true;                       // start the next focus after a break by itself
#define FOCUS_MS  ((uint32_t)FOCUS_OPTS[focusIdx] * 60000UL)
#define BREAK_MS  ((uint32_t)BREAK_OPTS[breakIdx] * 60000UL)
#define LBREAK_MS ((uint32_t)LONG_OPTS[longIdx] * 60000UL)
uint32_t pomoLeft() { return pomo ? (uint32_t)max(0L, (long)(pomoEnd - millis())) : 0; }

// ============================================================================
//  Behaviour
// ============================================================================
float shakeEnergy = 0, gravX = 0, gravY = 0, gravZ = 1;
uint32_t lastMotion = 0, touchStart = 0, freefallStart = 0;
bool wasTouch = false, asleep = false, talkative = true;
uint32_t asleepSince = 0;
bool tapEvent = false;            // consumed by onboarding
bool tripleTap = false;           // 4-tap gesture: opens settings
uint8_t brightness = 255, brightTarget = 255;
// user brightness: 25 / 50 / 75 / 100 %
const uint8_t BRIGHT_LEVELS[4] = { 64, 128, 191, 255 };
uint8_t brightIdx = 3;
uint8_t userBright() { return BRIGHT_LEVELS[brightIdx]; }

void wake(Mood m) {
  if (asleep) { asleep = false; special = SP_NONE; setMood(m, 1500); if (talkative) say(m == ANGRY ? EV_GRUMPY : EV_WAKE); }
  baseMood = NEUTRAL;
  lastMotion = millis();
}

void behave(float dt) {
  uint32_t now = millis();
  imuRead(); touchRead();

  float amag = sqrtf(ax * ax + ay * ay + az * az);
  gravX = approach(gravX, ax, 6, dt); gravY = approach(gravY, ay, 6, dt); gravZ = approach(gravZ, az, 6, dt);
  float jerk = fabsf(amag - 1.0f) + (fabsf(gx) + fabsf(gy) + fabsf(gz)) / 900.0f;
  shakeEnergy = shakeEnergy * expf(-2.5f * dt) + jerk * dt * 8.0f;
  if (jerk > 0.08f) { if (asleep && jerk > 0.25f) wake(SURPRISED); lastMotion = now; }

  if (amag < 0.35f) { if (!freefallStart) freefallStart = now; } else freefallStart = 0;
  if (freefallStart && now - freefallStart > 60 && mood != PANIC) { setMood(PANIC, 900); wake(PANIC); if (talkative) say(EV_DROP); }

  if (shakeEnergy > 3.2f && special != SP_DIZZY) {
    setSpecial(SP_DIZZY, 2600); shakeEnergy = 0; wake(NEUTRAL); if (talkative) say(EV_SHAKE);
  } else if (amag > 2.3f && special == SP_NONE && mood != SURPRISED && mood != PANIC) {
    setMood(SURPRISED, 900); blinkT = -1; if (talkative) say(EV_BUMP);
  }

  // touch
  if (touchDown && !wasTouch) touchStart = now;
  if (touchDown && now - touchStart > 700 && special != SP_LOVE) {
    if (asleep) wake(ANGRY); else { setSpecial(SP_LOVE, 2500); blushT = 1; if (talkative) say(EV_LOVE); }
  }
  if (!touchDown && wasTouch && now - touchStart < 700) {
    tapEvent = true;
    static uint32_t tapTimes[4] = {0, 0, 0, 0};          // 4 taps within 1 s -> settings
    for (int k = 0; k < 3; k++) tapTimes[k] = tapTimes[k + 1];
    tapTimes[3] = now;
    if (tapTimes[0] && now - tapTimes[0] < 1000) { tripleTap = true; for (auto &tt : tapTimes) tt = 0; }
    if (asleep) wake(ANGRY);
    else if (pomo == POMO_FOCUS) { setMood(FOCUS, 800); if (talkative && random(3) == 0) say(EV_SHH); }
    else { setMood(HAPPY, 1600); blushT = 1; if (talkative) say(EV_TAP); }
    lastMotion = now;
  }
  wasTouch = touchDown;

  // upside down
  if (gravZ < -0.75f && special == SP_NONE && !asleep) { if (baseMood != ANGRY && talkative) say(EV_FLIP); baseMood = ANGRY; }
  else if (baseMood == ANGRY) baseMood = NEUTRAL;

  // drowsiness
  if (pomo != POMO_OFF) lastMotion = now;            // stay awake through a pomodoro
  uint32_t still = now - lastMotion;
  const uint32_t sleepyAt = isNight() ? 10000 : 20000, asleepAt = isNight() ? 25000 : 45000;
  if (!asleep && still > asleepAt) { asleep = true; asleepSince = now; special = SP_ASLEEP; specialUntil = UINT32_MAX; speechText = nullptr; }
  else if (!asleep && still > sleepyAt && baseMood == NEUTRAL) { baseMood = SLEEPY; if (talkative) say(EV_SLEEPY); }
  else if (still < sleepyAt && baseMood == SLEEPY) baseMood = NEUTRAL;
  brightTarget = asleep ? max(12, userBright() / 6) : userBright();

  static uint32_t nextBat = 0; static bool firstBat = true;
  if (now > nextBat) {
    nextBat = now + 2000;
    float v = 0; for (int i = 0; i < 8; i++) v += batteryV(); v /= 8;
    bool usb = !boardV11 && v > 4.45f;   // V1.1 measures VBAT directly: no USB sense
    if (!usb) batV = firstBat ? v : batV * 0.8f + v * 0.2f;
    if (!firstBat && usb != onUsb) {
      if (asleep) wake(NEUTRAL); else lastMotion = now;
      setMood(usb ? HAPPY : NEUTRAL, 1500);
      if (talkative) say(usb ? EV_CHARGE : EV_UNPLUG);
      if (usb && baseMood == SAD) baseMood = NEUTRAL;
    }
    bool full = usb && readChargeFull();
    if (!firstBat && full && !chargeFull && talkative) { setMood(HAPPY, 1500); sayText("ALL CHARGED UP!"); }
    chargeFull = full;
    onUsb = usb; firstBat = false;
    if (!usb && batV > 2.5f && batV < 3.30f && ob == OB_FINISHED) powerOff("TOO TIRED. BYE...");   // protect the cell
    static uint32_t lastLowTalk = 0;
    if (!usb && batV > 2.5f && batV < 3.45f && baseMood == NEUTRAL) {
      baseMood = SAD;
      if (talkative && now - lastLowTalk > 120000) { lastLowTalk = now; say(EV_LOWBAT); }
    }
  }

  if (pomo != POMO_OFF && (int32_t)(now - pomoEnd) >= 0) {
    if (pomo == POMO_FOCUS) {
      pomoCount++;
      pomo = POMO_BREAK; pomoLen = (pomoCount % 4 == 0) ? LBREAK_MS : BREAK_MS;
      setMood(HAPPY, 2500); blushT = 1; say(EV_BREAK);
    } else if (pomoAuto) {
      pomo = POMO_FOCUS; pomoLen = FOCUS_MS; say(EV_FOCUS);
    } else {
      pomo = POMO_OFF; setMood(HAPPY, 1500); sayText("BREAK OVER. READY?");
    }
    pomoEnd = now + pomoLen;
    if (asleep) wake(NEUTRAL);
  }
  if (pomo == POMO_FOCUS && baseMood == NEUTRAL) baseMood = FOCUS;
  else if (pomo != POMO_FOCUS && baseMood == FOCUS) baseMood = NEUTRAL;
  if (moodUntil && now > moodUntil) { mood = baseMood; moodUntil = 0; }
  if (!moodUntil) mood = baseMood;
  if (special != SP_NONE && special != SP_ASLEEP && now > specialUntil) {
    if (special == SP_DIZZY) setMood(ANGRY, 1400);
    special = SP_NONE;
  }
  blushT = approach(blushT, 0, 0.9f, dt);
  blush = approach(blush, blushT > 0.3f ? 1.0f : 0.0f, 8, dt);

  // microphone reactions
  if (micOk) {
    float peak = micPeakDb; micPeakDb = -90;
    static float fast = -70, ambient = -65;
    static uint32_t loudSince = 0, quietSince = 0, talkSince = 0, lastBang = 0, lastLoud = 0, lastTalk = 0, lastQuiet = 0;
    fast = approach(fast, micDb, 3, dt);
    if (fast < ambient + 6) ambient = approach(ambient, fast, 0.05f, dt);
    audioPulse = approach(audioPulse, clampf((peak - ambient - 4) / 30.0f, 0, 1), 18, dt);
    if (peak > ambient + 24 && peak > MIC_BANG_DB && now - lastBang > 4000) {
      lastBang = now; setMood(SURPRISED, 900); blinkT = -1;
      if (asleep) wake(SURPRISED); else { lastMotion = now; if (talkative) say(EV_BANG); }
    }
    if (fast > MIC_LOUD_DB) { if (!loudSince) loudSince = now; } else loudSince = 0;
    if (loudSince && now - loudSince > 3000 && now - lastLoud > 15000 && !asleep) {
      bool party = now - loudSince > 9000;
      lastLoud = now; lastMotion = now; setMood(party ? HAPPY : ANGRY, 1500);
      if (talkative) say(party ? EV_PARTY : EV_LOUD);
    }
    if (fast > ambient + 8 && fast <= MIC_LOUD_DB) { if (!talkSince) talkSince = now; } else talkSince = 0;
    if (talkSince && now - talkSince > 2000 && now - lastTalk > 30000 && !asleep) {
      lastTalk = now; if (talkative && random(2)) say(EV_TALK, false);
    }
    if (fast < MIC_QUIET_DB) { if (!quietSince) quietSince = now; } else quietSince = 0;
    if (quietSince && now - quietSince > 30000 && now - lastQuiet > 90000 && !asleep) {
      lastQuiet = now; if (talkative) say(EV_QUIET, false);
    }
  }

  // idle life
  if (blinkT >= 0) { blinkT += dt; if (blinkT > 0.16f) blinkT = -1; }
  if (now > nextBlink && special == SP_NONE) { blinkT = 0; blinkWhich = 0; nextBlink = now + (random(10) == 0 ? 220 : random(1800, 5500)); }
  if (now > nextGlance) {
    bool c = random(3) == 0;
    glanceX = c ? 0 : random(-55, 56); glanceY = c ? 0 : random(-30, 31);
    nextGlance = now + random(900, 3200);
  }
  if (talkative && now > nextTrick && special == SP_NONE && !asleep && mood == baseMood && pomo != POMO_FOCUS) {
    switch (random(5)) {
      case 0: rollPhase = 0; if (random(2)) say(EV_ROLL, false); break;
      case 1: blinkT = 0; blinkWhich = 1; say(EV_WINK, false); break;
      case 2: convergeT = 38; say(EV_CROSS, false); break;
      case 3: setMood(SUSPICIOUS, 2200); glanceX = 70; say(EV_SUS, false); break;
      case 4: setMood(HAPPY, 1200); break;
    }
    nextTrick = now + random(9000, 22000);
  }
  if (talkative && now > nextIdleTalk && !asleep && special == SP_NONE && pomo != POMO_FOCUS) {
    Ev e = EV_IDLE;
    if (isMorning() && random(100) < 30) e = EV_MORNING;
    else if (isNight() && random(100) < 35) e = EV_NIGHT;
    say(e, false);
  }

  if (convergeT > 0) { converge = approach(converge, convergeT, 6, dt); if (converge > convergeT - 1) convergeT = -1; }
  else converge = approach(converge, 0, 3, dt);

  float tx = GAZE_SIGN_X * gravX * 130 + glanceX, ty = GAZE_SIGN_Y * gravY * 110 + glanceY;
  if (rollPhase >= 0) {
    rollPhase += dt; float a = rollPhase * 2 * PI / 1.1f - PI / 2;
    tx = cosf(a) * 70; ty = sinf(a) * 55 - 15; if (rollPhase > 1.1f) rollPhase = -1;
  }
  tx = constrain(tx, -120, 120); ty = constrain(ty, -100, 100);
  gvX += (tx - gazeX) * 220 * dt; gvY += (ty - gazeY) * 220 * dt;
  float damp = expf(-16 * dt); gvX *= damp; gvY *= damp; gazeX += gvX * dt; gazeY += gvY * dt;

  tweenShape(curL, moodShape(mood, true), dt, 10);
  tweenShape(curR, moodShape(mood, false), dt, 10);

  if (brightness != brightTarget) {
    brightness += (brightTarget > brightness) ? min(8, brightTarget - brightness) : -min(2, brightness - brightTarget);
    panel->setBrightness(brightness);
  }
}

// ============================================================================
//  Onboarding (Teenage Engineering-ish: tiny caps, numbered steps, one accent)
// ============================================================================
OnbStep ob = OB_FINISHED;
uint32_t obT0 = 0;
int modeHover = 0;               // 0 light, 1 dark
char serialTag[12];

void obGo(OnbStep s) { ob = s; obT0 = millis(); tripleTap = false; }
void stepHeader(const char *num, const char *title);
void startOnboarding();
void powerOff(const char *msg);
uint32_t obT() { return millis() - obT0; }

// tiny label: pitch 2 → effectively a 2x pixel font
void label(const char *s, int x, int y, uint16_t col, int align = 0, int maxW = W) { dotText(s, 99, x, y, 2, 1.0f, col, align, 0, maxW); }
void brackets(int x, int y, int w, int h, float t) {   // orange corner brackets, breathing
  int p = 3 + (int)(2 * sinf(t * 6)), L = 16; uint16_t o = C_ORANGE;
  g->fillRect(x - p, y - p, L, 3, o); g->fillRect(x - p, y - p, 3, L, o);
  g->fillRect(x + w + p - L, y - p, L, 3, o); g->fillRect(x + w + p - 3, y - p, 3, L, o);
  g->fillRect(x - p, y + h + p - 3, L, 3, o); g->fillRect(x - p, y + h + p - L, 3, L, o);
  g->fillRect(x + w + p - L, y + h + p - 3, L, 3, o); g->fillRect(x + w + p - 3, y + h + p - L, 3, L, o);
}
int totalSteps() { return profAdult ? 6 : 5; }
void stepHeaderN(int n, const char *title) { char b[4]; snprintf(b, sizeof(b), "%02d", n); stepHeader(b, title); }

void stepHeader(const char *num, const char *title) {
  g->fillRect(16, 18, 8, 8, C_ORANGE);
  int x = 30;
  if (num && *num) { label(num, x, 18, C_INK); x += strlen(num) * 12 + 10; }
  label(title, x, 18, C_INK);
}
void progressDots(int n, int of) {
  int total = of * 14 - 6, x0 = (W - total) / 2;
  for (int i = 0; i < of; i++) {
    if (i < n) g->fillCircle(x0 + i * 14 + 4, H - 26, 4, C_ORANGE);
    else g->drawCircle(x0 + i * 14 + 4, H - 26, 4, C_INK);
  }
}

// Boot: dots light up in a diagonal sweep, then the wordmark types in.
void obBoot() {
  uint32_t t = obT();
  memcpy(g->getFramebuffer(), bgBlack, W * H * 2);
  float sweep = t * 0.55f;   // px/ms along x+y
  for (int y = PITCH / 2; y < H; y += PITCH)
    for (int x = PITCH / 2; x < W; x += PITCH) {
      float d = sweep - (x * 0.6f + y);
      if (d < 0) continue;
      float k = clampf(d / 60.0f, 0, 1);
      // a bright wavefront that settles to dim
      uint8_t v = (uint8_t)(d < 60 ? 255 * (1 - k * 0.8f) : 40);
      g->fillCircle(x, y, d < 60 ? 2 : 1, rgb(v / 6, v, (uint8_t)(v * 0.85f)));
    }
  if (t > 700) {
    int vis = (t - 700) / 70;
    dotText("DOOBIE", vis, W / 2, 168, 7, 3.0f, C_TEAL, 1, 0, W);   // 6 x 42 px: needs the full width
  }
  if (t > 1300) {
    label("COMPANION", W / 2, 236, C_TEAL, 1);
    g->fillRect(W / 2 - 4, 262, 8, 8, C_ORANGE);
  }
  if (t > 1600) {
    label("OS " FW_VERSION, 16, 18, C_TEAL);
    label("280X456", W - 16 - 7 * 12, 18, C_TEAL);
    label(serialTag, 16, H - 30, C_TEAL);
    label("BOOT OK", W - 16 - 7 * 12, H - 30, C_TEAL);
  }
  if (t > 3200) { applyTheme(true); obGo(OB_HELLO); eyeOpen = 0; }
}

void obHello() {
  uint32_t t = obT();
  drawBackground();
  if (t < 900) eyeOpen = clampf((t - 300) / 600.0f, 0, 1);
  else eyeOpen = 1;
  if (t > 1000 && t < 1050 && !speaking()) sayText("HEY THERE.");
  if (t > 3300 && t < 3350) sayText("I'M DOOBIE.");
  if (t > 1600 && t < 3000) glanceX = sinf(t / 300.0f) * 60;   // looks around, curious
  drawFace(t / 1000.0f);
  drawSpeech();
  if (t > 6000) { obGo(OB_MODE); modeHover = 0; }
}

// Mode tiles. The rect for tile i, optionally zooming towards full screen.
void tileRect(int i, float zoom, int &x, int &y, int &w, int &h) {
  int tx = 20, tw = W - 40, th = 168, ty = i == 0 ? 52 : 52 + th + 16;
  x = tx + (0 - tx) * zoom; y = ty + (0 - ty) * zoom;
  w = tw + (W - tw) * zoom; h = th + (H - th) * zoom;
}
void drawTile(int i, float zoom, bool hover, float t) {
  int x, y, w, h; tileRect(i, zoom, x, y, w, h);
  bool dark = i == 1;
  // tile fill: a slice of the real background for that theme
  uint16_t *fb = g->getFramebuffer();
  for (int yy = max(0, y); yy < min(H, y + h); yy++)
    for (int xx = max(0, x); xx < min(W, x + w); xx++) {
      uint32_t hsh = hash32(xx * 7919 + yy * 104729);
      int n = (int)(hsh & 31) - 16;
      if (!dark) {
        float tt = (float)(xx + yy) / (W + H);
        fb[yy * W + xx] = rgb(clampf(18 - 18 * tt + n, 0, 255), clampf(236 - 60 * tt + n, 0, 255), clampf(214 - 36 * tt + n, 0, 255));
      } else {
        fb[yy * W + xx] = rgb(2, clampf(9 + n / 4, 0, 255), clampf(10 + n / 4, 0, 255));
      }
    }
  uint16_t ink = dark ? C_TEAL : rgb(4, 38, 44);
  // mini eyes
  EyeShape e = moodShape(hover ? HAPPY : NEUTRAL, true);
  float s = 0.36f + 0.36f * zoom, ecx = x + w / 2, ecy = y + h / 2 - 8 + 18 * zoom;
  float look = sinf(t * 1.3f + i) * 10;
  drawEye(ecx - 34 * (1 + zoom) + look, ecy, e, 0, true, s, ink);
  drawEye(ecx + 34 * (1 + zoom) + look, ecy, e, 0, false, s, ink);
  if (zoom < 0.05f) {
    label(dark ? "DARK" : "LIGHT", x + 12, y + 12, ink);
    label(dark ? "B" : "A", x + w - 22, y + 12, ink);
    if (dark) label("SAVES BATTERY", x + 12, y + h - 22, ink);
    else label("BRIGHT + GRAINY", x + 12, y + h - 22, ink);
    if (hover) brackets(x, y, w, h, t);
  }
}

void obMode() {
  uint32_t t = obT();
  memcpy(g->getFramebuffer(), bgBlack, W * H * 2);
  stepHeader("01", "CHOOSE MODE");
  // hover follows tilt (tilt the top/bottom towards you) or the finger
  if (touchDown) modeHover = touchY > H / 2 ? 1 : 0;
  else if (fabsf(gravY) > 0.25f) modeHover = (GAZE_SIGN_Y * gravY > 0) ? 1 : 0;
  drawTile(0, 0, modeHover == 0, t / 1000.0f);
  drawTile(1, 0, modeHover == 1, t / 1000.0f);
  label("TAP TO PICK", W / 2, H - 52, C_TEAL, 1);
  progressDots(1, totalSteps());
  if (tapEvent) { tapEvent = false; modeHover = touchY > H / 2 ? 1 : 0; obGo(OB_MODE_ZOOM); }
}
void obModeZoom() {
  uint32_t t = obT();
  memcpy(g->getFramebuffer(), bgBlack, W * H * 2);
  float z = clampf(t / 450.0f, 0, 1);
  z = 1 - (1 - z) * (1 - z) * (1 - z);   // ease-out
  drawTile(modeHover, z, false, t / 1000.0f);
  if (t > 500) {
    applyTheme(modeHover == 1);
    saveTheme();
    obGo(OB_SEX); tapEvent = false;
  }
}

// Generic stacked-tile question. Returns the picked index once, else -1.
int choiceHover = 0, choicePicked = -1; uint32_t choiceFlash = 0;
int obChoose(int stepN, const char *title, const Choice *c, int n) {
  uint32_t t = obT();
  drawBackground();
  if (stepN > 0) stepHeaderN(stepN, title); else stepHeader("", title);
  const int top = 52, bottom = H - 70, gap = n >= 4 ? 8 : 12, th = (bottom - top - gap * (n - 1)) / n;
  auto idxAt = [&](int y) { return constrain((y - top) / (th + gap), 0, n - 1); };
  if (choicePicked < 0) {
    if (touchDown) choiceHover = idxAt(touchY);
    else if (fabsf(gravY) > 0.3f) choiceHover = GAZE_SIGN_Y * gravY > 0 ? n - 1 : 0;
    if (tapEvent && t > 400) { choicePicked = idxAt(touchY); choiceFlash = millis(); }
  }
  tapEvent = false;
  for (int i = 0; i < n; i++) {
    int x = 20, y = top + i * (th + gap), w = W - 40;
    bool flash = choicePicked == i && ((millis() - choiceFlash) / 80) % 2 == 0;
    bool hov = (choicePicked < 0 ? choiceHover : choicePicked) == i;
    if (flash) g->fillRect(x, y, w, th, C_ORANGE);
    g->drawRect(x, y, w, th, C_INK); g->drawRect(x + 1, y + 1, w - 2, th - 2, C_INK);
    char key[2] = { (char)('A' + i), 0 };
    label(key, x + w - 22, y + 10, C_INK);
    // Measure, then stack: big label, then the small one right under it.
    // Shrink the big label to pitch 3 if needed; drop the small one if it
    // still doesn't fit. Nothing is ever drawn past the tile edge.
    const int bigW = w - 52, smallW = w - 28, padT = 12, padB = 10, between = 9;
    int bp = 4, bl = textLines(c[i].big, 4, bigW);
    int sl = c[i].small ? textLines(c[i].small, 2, smallW) : 0;
    auto need = [&](int p, int l, int s2) { return padT + textHeight(l, p) + (s2 ? between + textHeight(s2, 2) : 0) + padB; };
    if (need(bp, bl, sl) > th) { bp = 3; bl = textLines(c[i].big, 3, bigW); }
    if (need(bp, bl, sl) > th) sl = 0;
    int by = y + padT + (sl ? 0 : max(0, (th - padT - padB - textHeight(bl, bp)) / 2));
    dotText(c[i].big, 99, x + 14, by, bp, bp == 4 ? 1.7f : 1.3f, C_INK, 0, 0, bigW);
    if (sl) label(c[i].small, x + 14, by + textHeight(bl, bp) + between, C_INK, 0, smallW);
    if (hov && !flash) brackets(x, y, w, th, t / 1000.0f);
  }
  label(stepN > 0 ? "TAP TO PICK" : "4 TAPS: SETTINGS", W / 2, H - 52, C_INK, 1);
  if (stepN > 0) progressDots(stepN, totalSteps());
  if (choicePicked >= 0 && millis() - choiceFlash > 480) { int r = choicePicked; choicePicked = -1; choiceHover = 0; return r; }
  return -1;
}

const Choice CH_SEX[]   = { {"HE", "HIM / MALE"}, {"SHE", "HER / FEMALE"}, {"SKIP", "RATHER NOT SAY"} };
const Choice CH_AGE[]   = { {"UNDER 18", "KID MODE"}, {"18+", "GROWN-UP MODE"} };
const Choice CH_TONE[]  = { {"PET", "PET ATTITUDE. NO FLIRTING."}, {"CUTE", "CHARMING, NEVER RUDE."}, {"SPICY", "CHEEKY. MAY SAY IT IN PUBLIC."} };

void obSex() {
  int r = obChoose(2, "WHO ARE YOU?", CH_SEX, 3);
  if (r >= 0) { profSex = r; obGo(OB_AGE); }
}
void obAge() {
  int r = obChoose(3, "HOW OLD?", CH_AGE, 2);
  if (r >= 0) {
    profAdult = (r == 1); profTone = 0;
    if (profAdult) obGo(OB_SPICY);
    else { obGo(OB_TILT); sayText("COOL! LET'S PLAY."); }
  }
}
void obSpicy() {
  int r = obChoose(4, "ATTITUDE", CH_TONE, 3);
  if (r >= 0) {
    profTone = r; obGo(OB_TILT);
    sayText(r == 2 ? "OH MY. ;)" : r == 1 ? "CHARMED, I'M SURE." : "OK. I'M THE BOSS.");
  }
}

// Settings (4 taps in 1 s). Two pages; toggles apply instantly; closes after 10 s idle.
uint32_t settingsTouched = 0;
int settingsPage = 0;
void openSettings() { obGo(OB_SETTINGS); settingsPage = 0; settingsTouched = millis(); tapEvent = false; choicePicked = -1; speechText = nullptr; }
void openClockSet();
void openColorEditor(int slot);
char pomoMsg[24];
void pomoStart() { pomo = POMO_FOCUS; pomoLen = FOCUS_MS; pomoEnd = millis() + pomoLen; pomoCount = 0; setMood(FOCUS, 1500);
  snprintf(pomoMsg, sizeof(pomoMsg), "FOCUS TIME. %d MIN.", FOCUS_OPTS[focusIdx]); sayText(pomoMsg); }
void pomoStop()  { pomo = POMO_OFF; sayText("FOCUS OFF. GOOD JOB."); }

void obSettings() {
  static char modeTxt[16], toneTxt[16], clockTxt[16], focusTxt[16], brightTxt[16];
  Choice items[8]; int n = 0, ids[8];
  if (settingsPage == 0) {
    snprintf(focusTxt, sizeof(focusTxt), "%s", pomo == POMO_FOCUS ? "FOCUS: ON" : pomo == POMO_BREAK ? "FOCUS: BRK" : "FOCUS...");
    items[n] = { focusTxt, "POMODORO TIMER" }; ids[n++] = 15;
    int h, m;
    if (localHM(h, m)) snprintf(clockTxt, sizeof(clockTxt), "TIME %02d:%02d", h, m); else snprintf(clockTxt, sizeof(clockTxt), "SET TIME");
    items[n] = { clockTxt, "TAP TO SET THE CLOCK" }; ids[n++] = 11;
    snprintf(modeTxt, sizeof(modeTxt), "%s", "THEME...");
    items[n] = { modeTxt, "LIGHT, DARK, CUSTOM" }; ids[n++] = 16;
    snprintf(toneTxt, sizeof(toneTxt), "%s", "ATTITUDE...");
    items[n] = { toneTxt, "PET, CUTE, SPICY, QUOTES, EVENTS" }; ids[n++] = 17;
    items[n] = { "MORE...", nullptr }; ids[n++] = 12;
  } else if (settingsPage == 4 || settingsPage == 5) {
    static char aTxt[TONE_COUNT][16];
    int allowed[TONE_COUNT], na = 0;
    for (int i = 0; i < TONE_COUNT; i++) if (profAdult || (i != 1 && i != 2)) allowed[na++] = i;   // no flirting for kids
    int from = settingsPage == 4 ? 0 : 4, to = settingsPage == 4 ? min(4, na) : na;
    for (int k = from; k < to; k++) {
      int i = allowed[k];
      snprintf(aTxt[i], sizeof(aTxt[i]), "%s%s", profTone == i ? ">" : "", TONE_NAME[i]);
      items[n] = { aTxt[i], nullptr }; ids[n++] = 40 + i;
    }
    if (settingsPage == 4) { items[n] = { "MORE...", nullptr }; ids[n++] = 18; }
    else { items[n] = { "BACK", nullptr }; ids[n++] = 19; }
  } else if (settingsPage == 3) {
    static char tTxt[5][16];
    const char *names[5] = { "LIGHT", "DARK", "CUSTOM 1", "CUSTOM 2", "CUSTOM 3" };
    for (int i = 0; i < 5; i++) {
      snprintf(tTxt[i], sizeof(tTxt[i]), "%s%s", themeId == i ? "> " : "", names[i]);
      items[n] = { tTxt[i], i >= 2 ? "TAP TO EDIT" : nullptr }; ids[n++] = 30 + i;
    }
    items[n] = { "BACK", nullptr }; ids[n++] = 13;
  } else if (settingsPage == 2) {
    static char fTxt[16], bTxt[16], lTxt[16];
    snprintf(fTxt, sizeof(fTxt), "FOCUS %d", FOCUS_OPTS[focusIdx]);
    snprintf(bTxt, sizeof(bTxt), "BREAK %d", BREAK_OPTS[breakIdx]);
    snprintf(lTxt, sizeof(lTxt), "LONG %d", LONG_OPTS[longIdx]);
    items[n] = { pomo ? "STOP" : "START", pomo ? "END THE POMODORO" : "BEGIN A FOCUS ROUND" }; ids[n++] = 10;
    items[n] = { fTxt, "MINUTES OF FOCUS" }; ids[n++] = 20;
    items[n] = { bTxt, "SHORT BREAK, MINUTES" }; ids[n++] = 21;
    items[n] = { lTxt, "EVERY 4TH BREAK" }; ids[n++] = 22;
    items[n] = { pomoAuto ? "AUTO ON" : "AUTO OFF", "NEXT ROUND STARTS ITSELF" }; ids[n++] = 23;
    items[n] = { "BACK", nullptr }; ids[n++] = 13;
  } else {
    snprintf(brightTxt, sizeof(brightTxt), "BRIGHT %d", (brightIdx + 1) * 25);   // <=10 chars: one line
    items[n] = { brightTxt, "TAP: 25 / 50 / 75 / 100" }; ids[n++] = 14;
    items[n] = { "PROFILE", "REDO ONBOARDING" }; ids[n++] = 2;
    items[n] = { "POWER OFF", "BOOT WAKES IT" }; ids[n++] = 4;
    items[n] = { "BACK", nullptr }; ids[n++] = 13;
    items[n] = { "EXIT", nullptr }; ids[n++] = 3;
  }
  if (touchDown) settingsTouched = millis();
  int r = obChoose(0, settingsPage == 0 ? "SETTINGS 1/2" : settingsPage == 1 ? "SETTINGS 2/2" : settingsPage == 2 ? "FOCUS" : settingsPage == 3 ? "THEME" : settingsPage == 4 ? "ATTITUDE 1/2" : "ATTITUDE 2/2", items, n);
  if (r >= 0) {
    settingsTouched = millis();
    switch (ids[r]) {
      case 0:  applyTheme(!darkMode); saveTheme(); break;

      case 2:  startOnboarding(); return;
      case 3:  obGo(OB_FINISHED); sayText("BYE, SETTINGS."); lastMotion = millis(); return;
      case 4:  obGo(OB_FINISHED); powerOff("GOODNIGHT."); return;
      case 10: obGo(OB_FINISHED); if (pomo) pomoStop(); else pomoStart(); lastMotion = millis(); return;
      case 11: openClockSet(); return;
      case 12: settingsPage = 1; obT0 = millis(); return;
      case 13: settingsPage = 0; obT0 = millis(); return;
      case 15: settingsPage = 2; obT0 = millis(); return;
      case 16: settingsPage = 3; obT0 = millis(); return;
      case 17: settingsPage = 4; obT0 = millis(); return;
      case 18: settingsPage = 5; obT0 = millis(); return;
      case 19: settingsPage = 4; obT0 = millis(); return;
      case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
        profTone = ids[r] - 40; prefs.putUChar("tone", profTone);
        if ((profTone == 3 || profTone == 4)) {                                    // show a sample straight away
          obGo(OB_FINISHED); lastMotion = millis();
          if (profTone == 3) sayQuote(); else sayEvent();
          nextIdleTalk = speechEndsAt + 20000;
          return;
        }
        if (profTone >= 5) {                                    // characters introduce themselves
          obGo(OB_FINISHED); lastMotion = millis(); say(EV_GREET); return;
        }
        break;
      case 30: case 31: applyThemeId(ids[r] - 30); saveTheme(); break;
      case 32: case 33: case 34: openColorEditor(ids[r] - 32); return;
      case 20: focusIdx = (focusIdx + 1) % sizeof(FOCUS_OPTS); prefs.putUChar("fmin", focusIdx); break;
      case 21: breakIdx = (breakIdx + 1) % sizeof(BREAK_OPTS); prefs.putUChar("bmin", breakIdx); break;
      case 22: longIdx = (longIdx + 1) % sizeof(LONG_OPTS); prefs.putUChar("lmin", longIdx); break;
      case 23: pomoAuto = !pomoAuto; prefs.putBool("pauto", pomoAuto); break;
      case 14: brightIdx = (brightIdx + 1) % 4; prefs.putUChar("bright", brightIdx);
               brightness = brightTarget = userBright(); panel->setBrightness(brightness); break;
    }
  }
  if (millis() - settingsTouched > 10000) { obGo(OB_FINISHED); lastMotion = millis(); }
}

// Custom theme editor: step 0 eye colour, step 1 background colour.
// Dot-art colour wheel (hue around, saturation outward), value slider, SAVE.
int colSlot = 0, colStep = 0;
float colH = 170, colS = 0.8f, colV = 1.0f;
uint32_t colEyePick = 0, colTouched = 0;
const int WH_CX = W / 2, WH_CY = 206, WH_R = 96, SL_Y = 322, SV_Y = 362, SV_H = 44;
void openColorEditor(int slot) {
  colSlot = slot; colStep = 0;
  rgb2hsv(customEye[slot], colH, colS, colV);
  obGo(OB_COLOR); tapEvent = false; colTouched = millis();
}
void obColor() {
  const uint16_t UI = rgb(225, 235, 235);
  uint16_t savedInk = C_INK; C_INK = UI;               // menu chrome in neutral white
  memcpy(g->getFramebuffer(), bgBlack, W * H * 2);
  uint32_t cur = hsv2rgb(colH, colS, colV);
  uint32_t eyeC = colStep == 0 ? cur : colEyePick;
  uint32_t bgC  = colStep == 1 ? cur : customBg[colSlot];
  char title[24]; snprintf(title, sizeof(title), "%s  C%d", colStep == 0 ? "PICK EYE COLOR" : "PICK BACKGROUND", colSlot + 1);
  stepHeader("", title);

  // live preview: background swatch with mini eyes
  g->fillRoundRect(16, 40, W - 32, 52, 8, rgb565(bgC));
  EyeShape e = moodShape(NEUTRAL, true);
  drawEye(W / 2 - 30, 66, e, 0, true, 0.26f, rgb565(eyeC));
  drawEye(W / 2 + 30, 66, e, 0, false, 0.26f, rgb565(eyeC));

  // input
  if (touchDown) {
    colTouched = millis();
    float dx = touchX - WH_CX, dy = touchY - WH_CY, rr = sqrtf(dx * dx + dy * dy);
    if (rr <= WH_R + 14) { colH = atan2f(dy, dx) * 180.0f / PI; if (colH < 0) colH += 360; colS = clampf(rr / WH_R, 0, 1); }
    else if (touchY > SL_Y - 16 && touchY < SL_Y + 30) colV = clampf((touchX - 30) / (float)(W - 60), 0.05f, 1);
  }
  bool saveTap = tapEvent && touchY >= SV_Y - 6 && touchY <= SV_Y + SV_H + 6 && obT() > 400;
  tapEvent = false;

  // wheel as dots on a 7 px grid
  const int P7 = 7;
  for (int y = WH_CY - WH_R; y <= WH_CY + WH_R; y += P7)
    for (int x = WH_CX - WH_R; x <= WH_CX + WH_R; x += P7) {
      float dx = x - WH_CX, dy = y - WH_CY, rr = sqrtf(dx * dx + dy * dy);
      if (rr > WH_R) continue;
      float h = atan2f(dy, dx) * 180.0f / PI;
      g->fillCircle(x, y, 3, rgb565(hsv2rgb(h, rr / WH_R, colV)));
    }
  float mx = WH_CX + cosf(colH * PI / 180) * colS * WH_R, my = WH_CY + sinf(colH * PI / 180) * colS * WH_R;
  g->fillCircle(mx, my, 9, rgb565(cur)); g->drawCircle(mx, my, 10, UI); g->drawCircle(mx, my, 11, 0x0000);

  // value slider
  for (int x = 30; x <= W - 30; x += 6) {
    float v = (x - 30) / (float)(W - 60);
    g->fillCircle(x, SL_Y + 7, 3, rgb565(hsv2rgb(colH, colS, fmaxf(v, 0.05f))));
  }
  int kx = 30 + (int)(colV * (W - 60));
  g->fillRect(kx - 2, SL_Y - 4, 4, 22, C_ORANGE);
  label("DARK", 30, SL_Y + 18, UI); label("BRIGHT", W - 30 - 6 * 12, SL_Y + 18, UI);

  // save
  g->fillRoundRect(40, SV_Y, W - 80, SV_H, 10, C_ORANGE);
  dotText("SAVE", 99, W / 2, SV_Y + 9, 4, 1.7f, 0x0000, 1, 0, W);
  label(colStep == 0 ? "STEP 1/2" : "STEP 2/2", W / 2, H - 30, UI, 1);
  C_INK = savedInk;

  if (saveTap) {
    if (colStep == 0) {
      colEyePick = cur; colStep = 1; obT0 = millis();
      rgb2hsv(customBg[colSlot], colH, colS, colV);
    } else {
      customEye[colSlot] = colEyePick; customBg[colSlot] = cur;
      char k[4];
      snprintf(k, sizeof(k), "ce%d", colSlot); prefs.putUInt(k, colEyePick);
      snprintf(k, sizeof(k), "cb%d", colSlot); prefs.putUInt(k, cur);
      applyThemeId(2 + colSlot); saveTheme();
      obGo(OB_FINISHED); setMood(HAPPY, 1500); sayText("NEW LOOK!"); lastMotion = millis();
    }
  }
  if (millis() - colTouched > 30000) obGo(OB_FINISHED);   // walked away: keep the old theme
}

// Clock setting: top third = +, bottom third = -, hold to repeat, middle = next / save.
int clkField = 0, clkH = 12, clkM = 0;
uint32_t clkTouched = 0;
void openClockSet() {
  int h, m; if (localHM(h, m)) { clkH = h; clkM = m; }
  clkField = 0; obGo(OB_CLOCK); tapEvent = false; clkTouched = millis();
}
void obClock() {
  static uint32_t pressAt = 0, lastStep = 0; static int pressZone = 0; static bool wasDown = false;
  drawBackground();
  stepHeader("", "SET TIME");
  const int zTop = H / 3, zBot = 2 * H / 3;
  int zone = touchY < zTop ? 1 : touchY > zBot ? -1 : 0;
  auto step = [&](int d) {
    if (clkField == 0) clkH = (clkH + d + 24) % 24; else clkM = (clkM + d + 60) % 60;
  };
  if (touchDown && obT() > 300) {
    clkTouched = millis();
    if (!wasDown) { pressAt = millis(); pressZone = zone; lastStep = millis(); if (zone) step(zone); }
    else if (pressZone && millis() - pressAt > 450 && millis() - lastStep > 90) { lastStep = millis(); step(pressZone); }
  }
  if (!touchDown && wasDown && pressZone == 0 && obT() > 300) {
    if (clkField == 0) clkField = 1;
    else {
      time_t base = clockValid() ? (time(nullptr) / 86400) * 86400 : 1767225600L;   // keep today, else 2026-01-01
      setClock(base + clkH * 3600 + clkM * 60);
      obGo(OB_FINISHED); sayText("TIME IS SET."); lastMotion = millis();
      wasDown = false; return;
    }
  }
  wasDown = touchDown;
  tapEvent = false;
  // + / - zones
  dotText("+", 99, W / 2, zTop / 2 - 10, 6, 2.5f, C_INK, 1, 0, W);
  dotText("-", 99, W / 2, zBot + (H - zBot) / 2 - 30, 6, 2.5f, C_INK, 1, 0, W);
  // big HH:MM, current field underlined in orange
  char t[6]; snprintf(t, sizeof(t), "%02d:%02d", clkH, clkM);
  const int p = 7, adv = 6 * p, x0 = W / 2 - (5 * adv - p) / 2, y0 = H / 2 - 28;
  dotText(t, 99, W / 2, y0, p, 3.0f, C_INK, 1, 0, W);
  int ux = clkField == 0 ? x0 : x0 + 3 * adv;
  g->fillRect(ux, y0 + 7 * p + 8, 2 * adv - p, 4, C_ORANGE);
  label(clkField == 0 ? "HOURS" : "MINUTES", W / 2, y0 - 26, C_INK, 1);
  label(clkField == 0 ? "TAP MIDDLE: NEXT" : "TAP MIDDLE: SAVE", W / 2, y0 + 7 * p + 22, C_INK, 1);
  if (millis() - clkTouched > 15000) obGo(OB_FINISHED);
}

void obTilt() {
  static uint32_t tiltHeld = 0;
  uint32_t t = obT();
  drawBackground();
  stepHeaderN(profAdult ? 5 : 4, "TILT ME");
  if (t > 2000 && t < 2050) sayText("TILT ME.");
  bool tilted = sqrtf(gravX * gravX + gravY * gravY) > 0.35f;
  if (tilted) { if (!tiltHeld) tiltHeld = millis(); } else tiltHeld = 0;
  drawFace(t / 1000.0f);
  drawSpeech();
  progressDots(profAdult ? 5 : 4, totalSteps());
  if ((tiltHeld && millis() - tiltHeld > 600) || t > 12000) {
    tiltHeld = 0; sayText("WHEEE.");
    setMood(HAPPY, 1200); obGo(OB_TAP); tapEvent = false;
  }
}
void obTap() {
  uint32_t t = obT();
  drawBackground();
  stepHeaderN(profAdult ? 6 : 5, "TAP ME");
  if (t > 1600 && t < 1650) sayText("NOW TAP ME.");
  drawFace(t / 1000.0f);
  drawSpeech();
  progressDots(totalSteps(), totalSteps());
  if (tapEvent || t > 15000) { tapEvent = false; setMood(HAPPY, 2000); blushT = 1; sayText(!profAdult ? "LET'S BE FRIENDS, {N}!" : profTone == 2 ? "WELL HELLO, {N}. ;)" : profTone == 1 ? "NICE TO MEET YOU, {N}." : "YOU MAY PET ME NOW."); obGo(OB_DONE); }
}
void obDone() {
  uint32_t t = obT();
  drawBackground();
  drawFace(t / 1000.0f);
  drawSpeech();
  // orange scan bar sweeping down = "saved"
  if (t > 2600) {
    int y = (int)((t - 2600) * 0.9f) - 10;
    if (y < H) g->fillRect(0, y, W, 4, C_ORANGE);
  }
  if (t > 3200) {
    prefs.putBool("onb", true);
    prefs.putUChar("sex", profSex); prefs.putBool("adult", profAdult); prefs.putUChar("tone", profAdult ? profTone : 0);
    talkative = true; lastMotion = millis(); nextIdleTalk = millis() + 20000;
    obGo(OB_FINISHED);
  }
}

void startOnboarding() {
  talkative = false; asleep = false; special = SP_NONE; speechText = nullptr;
  brightTarget = userBright(); panel->setBrightness(userBright()); brightness = userBright();
  obGo(OB_BOOT);
}

// ============================================================================
//  BOOT button: short = toggle theme, hold 2 s = onboarding again
// ============================================================================
void pollButton() {
  static uint32_t downAt = 0; static bool longFired = false;
  bool down = digitalRead(PIN_BOOT) == LOW;
  if (down && !downAt) { downAt = millis(); longFired = false; }
  if (down && downAt && !longFired && millis() - downAt > 2000) { longFired = true; powerOff("GOODNIGHT."); }
  if (!down && downAt) {
    if (!longFired && millis() - downAt > 30 && ob == OB_FINISHED) {
      applyTheme(!darkMode); saveTheme();
      sayText(darkMode ? "LIGHTS OFF." : "LIGHTS ON.");
      wake(NEUTRAL);
    }
    downAt = 0;
  }
}

// ============================================================================
//  Power off = deep sleep (the board has no power switch).
//  Draw goes to roughly the regulator + charger quiescent current; the cell
//  still charges over USB while "off". Wake: BOOT (GPIO0, RTC-capable) or RST.
// ============================================================================
void powerOff(const char *msg) {
  Serial.println("power off");
  special = SP_NONE; asleep = false; blinkT = -1; mood = baseMood = SLEEPY; moodUntil = 0;
  panel->setBrightness(userBright());
  sayText(msg);
  uint32_t t0 = millis(), last = t0;
  while (millis() - t0 < 2200) {               // eyes droop shut while it says goodbye
    uint32_t now = millis(); float dt = (now - last) / 1000.0f; last = now;
    eyeOpen = 1.0f - clampf((now - t0 - 700) / 900.0f, 0, 1);
    tweenShape(curL, moodShape(SLEEPY, true), dt, 6); tweenShape(curR, moodShape(SLEEPY, false), dt, 6);
    drawBackground(); drawFace(now / 1000.0f); drawSpeech(); g->flush();
  }
  for (int i = 0; i <= 14; i++) {              // old-TV collapse to a dot
    drawBackground();
    float k = i / 14.0f;
    int w = (int)(W * 0.7f * (1 - k)) + 6, h = (int)(10 * (1 - k)) + 4;
    g->fillRoundRect(W / 2 - w / 2, H / 2 - h / 2, w, h, h / 2, C_INK);
    g->flush(); delay(16);
  }
  for (int b = userBright(); b > 0; b -= 32) { panel->setBrightness(b); delay(12); }
  panel->setBrightness(0);
  panel->displayOff();                         // DISPOFF + SLPIN
  i2cWrite(ADDR_TOUCH, 0x86, 0x01);            // touch: auto-drop to monitor mode
  if (imuOk) i2cWrite(ADDR_IMU, 0x08, 0x00);   // IMU: accel + gyro off
  if (micOk && micI2S) micI2S->end();
  prefs.end();
  while (digitalRead(PIN_BOOT) == LOW) delay(10);   // don't wake on the same press
  delay(80);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  Serial.flush();
  esp_deep_sleep_start();
}

// ============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  imuInit();
  analogReadResolution(12);
  micInit();

  boardV11 = detectBoardV11();
  Serial.printf("Board: ESP32-S3-Touch-AMOLED-1.64 %s\n", boardV11 ? "V1.1" : "V1");
  bus = new Arduino_ESP32QSPI(boardV11 ? PIN_QSPI_CS_V11 : PIN_QSPI_CS_V1, PIN_QSPI_CLK,
                              PIN_QSPI_D0, PIN_QSPI_D1, PIN_QSPI_D2, PIN_QSPI_D3);
  panel = new Arduino_CO5300(bus, PIN_LCD_RST, 0, false, W, H, COL_OFFSET, 0, COL_OFFSET, 0);
  g = new Arduino_Canvas(W, H, panel);
  if (!g->begin()) Serial.println("Canvas alloc failed (PSRAM?)");
  delay(200);                                  // V1.1: touch shares LCD reset, let it boot
  i2cWrite(ADDR_TOUCH, 0x00, 0x00);            // FT3168: normal operating mode
  i2cWrite(ADDR_TOUCH, 0x86, 0x00);            // stay active, don't auto-drop to monitor mode
  Serial.print("I2C:");
  for (uint8_t a = 1; a < 127; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a); }
  Serial.println();
  panel->setBrightness(255);
  for (int f = 0; f < GRAIN_FRAMES; f++) bgFrames[f] = (uint16_t *)heap_caps_malloc(W * H * 2, MALLOC_CAP_SPIRAM);
  bgBlack = (uint16_t *)heap_caps_malloc(W * H * 2, MALLOC_CAP_SPIRAM);
  for (int i = 0; i < W * H; i++) {   // near-black with faint grain for the boot/menu screens
    uint8_t n = hash32(i * 2654435761u) & 7;
    bgBlack[i] = rgb(n / 2, n, n);
  }
  uint64_t mac = ESP.getEfuseMac();
  snprintf(serialTag, sizeof(serialTag), "SN %06X", (uint32_t)(mac >> 24) & 0xFFFFFF);

  prefs.begin("doobie", false);
  bool onboarded = prefs.getBool("onb", false) && prefs.isKey("adult");   // re-run if no profile yet
  for (int i = 0; i < 3; i++) {
    char k[4];
    snprintf(k, sizeof(k), "ce%d", i); customEye[i] = prefs.getUInt(k, customEye[i]);
    snprintf(k, sizeof(k), "cb%d", i); customBg[i]  = prefs.getUInt(k, customBg[i]);
  }
  themeId = prefs.isKey("theme") ? prefs.getUChar("theme", 0) : (prefs.getBool("dark", false) ? 1 : 0);
  applyThemeId(min((int)themeId, 4));
  brightIdx = min((int)prefs.getUChar("bright", 3), 3);
  focusIdx = min((int)prefs.getUChar("fmin", 2), (int)sizeof(FOCUS_OPTS) - 1);
  breakIdx = min((int)prefs.getUChar("bmin", 1), (int)sizeof(BREAK_OPTS) - 1);
  longIdx  = min((int)prefs.getUChar("lmin", 1), (int)sizeof(LONG_OPTS) - 1);
  pomoAuto = prefs.getBool("pauto", true);
  brightness = brightTarget = userBright(); panel->setBrightness(brightness);
  profSex = prefs.getUChar("sex", 2); profAdult = prefs.getBool("adult", false);
  // 0.3 stored a "spicy" bool; map it: spicy -> 2, otherwise flirty (what 0.3 adults got)
  if (prefs.isKey("tone")) profTone = prefs.getUChar("tone", 0);
  else profTone = prefs.getBool("spicy", false) ? 2 : (profAdult ? 1 : 0);
  if (profTone >= TONE_COUNT) profTone = 0;
  if (!profAdult && (profTone == 1 || profTone == 2)) profTone = 0;
  Serial.printf("Profile: sex=%d adult=%d tone=%s\n", profSex, profAdult, TONE_NAME[profTone]);
  curL = curR = moodShape(NEUTRAL, true);
  randomSeed(esp_random());
  lastMotion = millis();

  Serial.printf("MEM internal free %u KB (largest %u KB), PSRAM free %u / %u KB\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024, heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024,
                ESP.getFreePsram() / 1024, ESP.getPsramSize() / 1024);
  if (!onboarded) startOnboarding();
  else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) { rtc_gpio_deinit(GPIO_NUM_0); pinMode(PIN_BOOT, INPUT_PULLUP); sayText("I'M BACK!"); }
  else { say(EV_GREET); }
}

void loop() {
  static uint32_t last = millis(), lastLog = 0;
  uint32_t now = millis();
  float dt = constrain((now - last) / 1000.0f, 0.001f, 0.05f);
  last = now;

  pollButton();
  pollSerial();
  talkative = (ob == OB_FINISHED);
  behave(dt);

  // Power saving for the 150 mAh cell: ~12 fps while asleep, panel fully off
  // after 5 min asleep (IMU still polled, so any motion wakes it).
  static bool panelOff = false;
  if (asleep && ob == OB_FINISHED && now - asleepSince > 300000) {
    if (!panelOff) { panel->displayOff(); panelOff = true; Serial.println("panel off"); }
    delay(150);
    return;
  }
  if (panelOff) { panel->displayOn(); panelOff = false; panel->setBrightness(brightness); }

  switch (ob) {
    case OB_BOOT:      obBoot(); break;
    case OB_HELLO:     obHello(); break;
    case OB_MODE:      obMode(); break;
    case OB_MODE_ZOOM: obModeZoom(); break;
    case OB_SEX:       obSex(); break;
    case OB_AGE:       obAge(); break;
    case OB_SPICY:     obSpicy(); break;
    case OB_TILT:      obTilt(); break;
    case OB_TAP:       obTap(); break;
    case OB_DONE:      obDone(); break;
    case OB_SETTINGS:  obSettings(); break;
    case OB_CLOCK:     obClock(); break;
    case OB_COLOR:     obColor(); break;
    default:
      if (tripleTap) { tripleTap = false; openSettings(); break; }
      tapEvent = false;
      drawBackground();
      drawFace(now / 1000.0f);
      drawSpeech();
      {   // status row: clock or focus timer (left), power (right)
        char st[8];
        if (onUsb) snprintf(st, sizeof(st), chargeFull ? "FULL" : "USB"); else snprintf(st, sizeof(st), "%d%%", batteryPct());
        label(st, W - 16 - strlen(st) * 12, H - 28, onUsb ? C_ORANGE : C_INK);
        char lt[16]; int h, m;
        if (pomo) {
          uint32_t left = (pomoLeft() + 999) / 1000;
          snprintf(lt, sizeof(lt), "%s %02lu:%02lu", pomo == POMO_FOCUS ? "FOCUS" : "BREAK", (unsigned long)(left / 60), (unsigned long)(left % 60));
          label(lt, 16, H - 28, C_ORANGE);
          int bw = (int)((W - 32) * (float)pomoLeft() / pomoLen);     // shrinking progress bar
          g->fillRect(16, H - 12, bw, 3, C_ORANGE);
        } else if (localHM(h, m)) {
          snprintf(lt, sizeof(lt), "%02d:%02d", h, m);
          label(lt, 16, H - 28, C_INK);
        }
      }
  }
  g->flush();
  if (asleep) delay(60);

  if (now - lastLog > 500) {
    lastLog = now;
    Serial.printf("t=%ld pomo=%d usb=%d batV=%.2f mic=%s%.0fdB ob=%d dark=%d a=%.2f,%.2f,%.2f touch=%d(%d,%d) bat=%.2fV mood=%d sp=%d say=\"%s\" fps=%.0f\n",
                  clockValid() ? (long)(time(nullptr) % 86400) : -1L, pomo, onUsb, batV, micOk ? "" : "off/", (float)micDb, ob, darkMode, ax, ay, az, touchDown, touchX, touchY, batteryV(), mood, special,
                  speaking() ? speechText : "", 1.0f / dt);
  }
}

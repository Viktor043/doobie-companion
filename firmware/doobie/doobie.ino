// ============================================================================
//  DOOBIE COMPANION, firmware 0.3
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
//    Triple-tap screen: settings (mode, spicy, redo onboarding)
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
#include "eyetypes.h"
#include "phrases.h"

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
void applyTheme(bool dark) {
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

// ============================================================================
//  Speech
// ============================================================================
const char *speechText = nullptr;
uint32_t speechStart = 0, nextIdleTalk = 25000;
uint16_t recent[8]; uint8_t recentHead = 0;  // recently used lines (pointer hash) to avoid repeats

uint32_t speechDuration(const char *s) { return strlen(s) * 38 + 1700 + 380; }
bool speaking() { return speechText && millis() - speechStart < speechDuration(speechText); }

// ---------- user profile (set during onboarding) ----------
uint8_t profSex = 2;            // 0 he, 1 she, 2 skip
bool profAdult = false;         // under-18 profiles never see ADULT/SPICY lines
bool profSpicy = false;         // adults opt in separately
char speechBuf[72];

const char *petName() {
  if (!profAdult)   return PET_KID[random(4)];
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
  speechText = speechBuf; speechStart = millis();
}
const Pool *pickPool(Ev e) {
  const EvPools &P = EVENTS[e];
  if (profAdult && profSpicy && P.spicy.n && random(100) < 45) return &P.spicy;
  if (profAdult && P.adult.n && random(100) < 35) return &P.adult;
  if (!profAdult && e == EV_IDLE && random(100) < 40) return &KID_IDLE;
  return &P.base;
}
void say(Ev e, bool force = true) {
  if (!force && speaking()) return;
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
  sayText(L->items[idx]);
  nextIdleTalk = millis() + random(22000, 55000);
}
void drawSpeech() {
  if (!speechText) return;
  uint32_t t = millis() - speechStart, total = speechDuration(speechText);
  if (t >= total) { speechText = nullptr; return; }
  int len = strlen(speechText);
  int visible = min(len, (int)(t / 38) + 1);
  float dissolve = t > total - 380 ? (t - (total - 380)) / 380.0f : 0;
  dotText(speechText, visible, W / 2, 46, 4, 1.7f, C_INK, 1, dissolve);
  if (visible < len && (t / 120) % 2) {   // block cursor while typing
    // cursor drawn as a small accent square at the bottom-right of the text region
    g->fillRect(W / 2 + 6, 46 + 7 * 4 + 6, 6, 3, C_ORANGE);
  }
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
  float ly = cy + gazeY * GAZE_KY + bob, ry = ly;
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
//  Behaviour
// ============================================================================
float shakeEnergy = 0, gravX = 0, gravY = 0, gravZ = 1;
uint32_t lastMotion = 0, touchStart = 0, freefallStart = 0;
bool wasTouch = false, asleep = false, talkative = true;
uint32_t asleepSince = 0;
bool tapEvent = false;            // consumed by onboarding
bool tripleTap = false;           // opens settings
uint8_t brightness = 255, brightTarget = 255;

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
    static uint32_t tapTimes[3] = {0, 0, 0};
    tapTimes[0] = tapTimes[1]; tapTimes[1] = tapTimes[2]; tapTimes[2] = now;
    if (tapTimes[0] && now - tapTimes[0] < 900) { tripleTap = true; tapTimes[0] = tapTimes[1] = tapTimes[2] = 0; }
    if (asleep) wake(ANGRY);
    else { setMood(HAPPY, 1600); blushT = 1; if (talkative) say(EV_TAP); }
    lastMotion = now;
  }
  wasTouch = touchDown;

  // upside down
  if (gravZ < -0.75f && special == SP_NONE && !asleep) { if (baseMood != ANGRY && talkative) say(EV_FLIP); baseMood = ANGRY; }
  else if (baseMood == ANGRY) baseMood = NEUTRAL;

  // drowsiness
  uint32_t still = now - lastMotion;
  if (!asleep && still > 45000) { asleep = true; asleepSince = now; special = SP_ASLEEP; specialUntil = UINT32_MAX; speechText = nullptr; }
  else if (!asleep && still > 20000 && baseMood == NEUTRAL) { baseMood = SLEEPY; if (talkative) say(EV_SLEEPY); }
  else if (still < 20000 && baseMood == SLEEPY) baseMood = NEUTRAL;
  brightTarget = asleep ? 40 : 255;

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
  if (talkative && now > nextTrick && special == SP_NONE && !asleep && mood == baseMood) {
    switch (random(5)) {
      case 0: rollPhase = 0; if (random(2)) say(EV_ROLL, false); break;
      case 1: blinkT = 0; blinkWhich = 1; say(EV_WINK, false); break;
      case 2: convergeT = 38; say(EV_CROSS, false); break;
      case 3: setMood(SUSPICIOUS, 2200); glanceX = 70; say(EV_SUS, false); break;
      case 4: setMood(HAPPY, 1200); break;
    }
    nextTrick = now + random(9000, 22000);
  }
  if (talkative && now > nextIdleTalk && !asleep && special == SP_NONE) say(EV_IDLE, false);

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
  label(num, 30, 18, C_INK);
  label(title, 30 + strlen(num) * 12 + 10, 18, C_INK);
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
    dotText("DOOBIE", vis, W / 2, 170, 7, 3.0f, C_TEAL);
  }
  if (t > 1300) {
    label("COMPANION", W / 2, 240, C_TEAL, 1);
    g->fillRect(W / 2 - 4, 262, 8, 8, C_ORANGE);
  }
  if (t > 1600) {
    label("OS 0.2", 16, 18, C_TEAL);
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
    prefs.putBool("dark", darkMode);
    obGo(OB_SEX); tapEvent = false;
  }
}

// Generic stacked-tile question. Returns the picked index once, else -1.
int choiceHover = 0, choicePicked = -1; uint32_t choiceFlash = 0;
int obChoose(int stepN, const char *title, const Choice *c, int n) {
  uint32_t t = obT();
  drawBackground();
  if (stepN > 0) stepHeaderN(stepN, title); else stepHeader("SET", title);
  const int top = 52, bottom = H - 70, gap = 12, th = (bottom - top - gap * (n - 1)) / n;
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
    dotText(c[i].big, 99, x + 14, y + 14, 4, 1.7f, C_INK, 0, 0, w - 40);
    if (c[i].small) label(c[i].small, x + 14, y + th - (strlen(c[i].small) > 18 ? 42 : 22), C_INK, 0, w - 28);
    if (hov && !flash) brackets(x, y, w, th, t / 1000.0f);
  }
  label(stepN > 0 ? "TAP TO PICK" : "TRIPLE-TAP ANYTIME", W / 2, H - 52, C_INK, 1);
  if (stepN > 0) progressDots(stepN, totalSteps());
  if (choicePicked >= 0 && millis() - choiceFlash > 480) { int r = choicePicked; choicePicked = -1; choiceHover = 0; return r; }
  return -1;
}

const Choice CH_SEX[]   = { {"HE", "HIM / MALE"}, {"SHE", "HER / FEMALE"}, {"SKIP", "RATHER NOT SAY"} };
const Choice CH_AGE[]   = { {"UNDER 18", "KID MODE"}, {"18+", "GROWN-UP MODE"} };
const Choice CH_SPICY[] = { {"KEEP IT CUTE", "FLIRTY, NEVER RUDE"}, {"UNLOCK SPICY", "CHEEKY LINES. MAY SAY THINGS IN PUBLIC."} };

void obSex() {
  int r = obChoose(2, "WHO ARE YOU?", CH_SEX, 3);
  if (r >= 0) { profSex = r; obGo(OB_AGE); }
}
void obAge() {
  int r = obChoose(3, "HOW OLD?", CH_AGE, 2);
  if (r >= 0) {
    profAdult = (r == 1); profSpicy = false;
    if (profAdult) obGo(OB_SPICY);
    else { obGo(OB_TILT); sayText("COOL! LET'S PLAY."); }
  }
}
void obSpicy() {
  int r = obChoose(4, "SPICY MODE?", CH_SPICY, 2);
  if (r >= 0) { profSpicy = (r == 1); obGo(OB_TILT); sayText(profSpicy ? "OH MY. ;)" : "CUTE IT IS."); }
}

// Settings (triple-tap). Toggles apply instantly; closes after 10 s idle.
uint32_t settingsTouched = 0;
void openSettings() { obGo(OB_SETTINGS); settingsTouched = millis(); tapEvent = false; choicePicked = -1; speechText = nullptr; }
void obSettings() {
  static char modeTxt[16], spicyTxt[16];
  snprintf(modeTxt, sizeof(modeTxt), "MODE: %s", darkMode ? "DARK" : "LIGHT");
  snprintf(spicyTxt, sizeof(spicyTxt), "SPICY: %s", profSpicy ? "ON" : "OFF");
  Choice items[5]; int n = 0, ids[5];
  items[n] = { modeTxt, "LIGHT = TEAL, DARK = SAVES BATTERY" }; ids[n++] = 0;
  if (profAdult) { items[n] = { spicyTxt, "CHEEKY LINES ON / OFF" }; ids[n++] = 1; }
  items[n] = { "PROFILE", "REDO ONBOARDING" }; ids[n++] = 2;
  items[n] = { "POWER OFF", "PRESS BOOT TO WAKE" }; ids[n++] = 4;
  items[n] = { "EXIT", nullptr }; ids[n++] = 3;
  if (touchDown) settingsTouched = millis();
  int r = obChoose(0, "SETTINGS", items, n);
  if (r >= 0) {
    settingsTouched = millis();
    switch (ids[r]) {
      case 0: applyTheme(!darkMode); prefs.putBool("dark", darkMode); break;
      case 1: profSpicy = !profSpicy; prefs.putBool("spicy", profSpicy); break;
      case 2: startOnboarding(); return;
      case 3: obGo(OB_FINISHED); sayText("BYE, SETTINGS."); lastMotion = millis(); return;
      case 4: obGo(OB_FINISHED); powerOff("GOODNIGHT."); return;
    }
  }
  if (millis() - settingsTouched > 10000) { obGo(OB_FINISHED); lastMotion = millis(); }
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
  if (tapEvent || t > 15000) { tapEvent = false; setMood(HAPPY, 2000); blushT = 1; sayText(!profAdult ? "LET'S BE FRIENDS, {N}!" : profSpicy ? "WELL HELLO, {N}. ;)" : "NICE TO MEET YOU, {N}."); obGo(OB_DONE); }
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
    prefs.putUChar("sex", profSex); prefs.putBool("adult", profAdult); prefs.putBool("spicy", profAdult && profSpicy);
    talkative = true; lastMotion = millis(); nextIdleTalk = millis() + 20000;
    obGo(OB_FINISHED);
  }
}

void startOnboarding() {
  talkative = false; asleep = false; special = SP_NONE; speechText = nullptr;
  brightTarget = 255; panel->setBrightness(255); brightness = 255;
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
      applyTheme(!darkMode); prefs.putBool("dark", darkMode);
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
  panel->setBrightness(255);
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
  for (int b = 255; b > 0; b -= 32) { panel->setBrightness(b); delay(12); }
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
  applyTheme(prefs.getBool("dark", false));
  profSex = prefs.getUChar("sex", 2); profAdult = prefs.getBool("adult", false);
  profSpicy = profAdult && prefs.getBool("spicy", false);
  Serial.printf("Profile: sex=%d adult=%d spicy=%d\n", profSex, profAdult, profSpicy);
  curL = curR = moodShape(NEUTRAL, true);
  randomSeed(esp_random());
  lastMotion = millis();

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
    default:
      if (tripleTap) { tripleTap = false; openSettings(); break; }
      tapEvent = false;
      drawBackground();
      drawFace(now / 1000.0f);
      drawSpeech();
      {   // tiny status: USB while plugged, battery % otherwise
        char st[8];
        if (onUsb) snprintf(st, sizeof(st), chargeFull ? "FULL" : "USB"); else snprintf(st, sizeof(st), "%d%%", batteryPct());
        label(st, W - 16 - strlen(st) * 12, H - 28, onUsb ? C_ORANGE : C_INK);
      }
  }
  g->flush();
  if (asleep) delay(60);

  if (now - lastLog > 500) {
    lastLog = now;
    Serial.printf("usb=%d batV=%.2f mic=%s%.0fdB ob=%d dark=%d a=%.2f,%.2f,%.2f touch=%d(%d,%d) bat=%.2fV mood=%d sp=%d say=\"%s\" fps=%.0f\n",
                  onUsb, batV, micOk ? "" : "off/", (float)micDb, ob, darkMode, ax, ay, az, touchDown, touchX, touchY, batteryV(), mood, special,
                  speaking() ? speechText : "", 1.0f / dt);
  }
}

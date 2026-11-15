#pragma once
struct EyeShape {
  float w, h, r;     // size + corner radius
  float lidTop;      // 0..1 top lid coverage
  float lidBot;      // 0..1 bottom "happy" crescent
  float slant;       // + = angry (inner side lower), - = sad
};
enum Mood { NEUTRAL, HAPPY, SLEEPY, ANGRY, SAD, SURPRISED, SUSPICIOUS, PANIC, FOCUS };
enum Special { SP_NONE, SP_LOVE, SP_DIZZY, SP_ASLEEP };
enum OnbStep { OB_BOOT, OB_HELLO, OB_MODE, OB_MODE_ZOOM, OB_SEX, OB_AGE, OB_SPICY, OB_TILT, OB_TAP, OB_DONE, OB_FINISHED, OB_SETTINGS, OB_CLOCK, OB_COLOR };
struct Choice { const char *big; const char *small; };

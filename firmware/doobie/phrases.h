#pragma once
// ============================================================================
//  Doobie's vocabulary: short dot-matrix snippets, grouped by what triggered
//  them. Keep each under ~22 chars; the renderer wraps at ~10 chars per line.
//  ASCII only, plus "\x03", which the 5x7 font draws as a heart.
// ============================================================================

enum Cat : uint8_t {
  CAT_GREET, CAT_TAP, CAT_LOVE, CAT_SHAKE, CAT_BUMP, CAT_DROP, CAT_FLIP,
  CAT_SLEEPY, CAT_WAKE, CAT_GRUMPY, CAT_IDLE, CAT_SUS, CAT_ROLL, CAT_CROSS,
  CAT_WINK, CAT_LOWBAT, CAT_COUNT
};

static const char *const P_GREET[] = {
  "HEY THERE.", "HI.", "OH, HELLO.", "MISSED ME?", "LOOKING GOOD.", "HEY YOU.",
  "WELL HELLO.", "YOU AGAIN :)", "SUP.", "HOWDY.", "BACK SO SOON?", "HELLO, HUMAN.",
};
static const char *const P_TAP[] = {
  "HEHE.", "BOOP.", "AGAIN!", "TICKLES.", "THAT'S NICE.", "OOH.", "YES?",
  "BOOP BOOP.", "MORE PLEASE.", "CHEEKY.", "I FELT THAT.", "HI, FINGER.",
  "STOP. NO, DON'T.",
};
static const char *const P_LOVE[] = {
  "\x03", "\x03 \x03 \x03", "MMMM.", "DON'T STOP.", "SO WARM.", "YOU'RE SWEET.",
  "BEST HUMAN.", "PURR.", "LOVE THAT.", "CLOSER...", "OK, I LIKE YOU.",
  "STAY A WHILE.",
};
static const char *const P_SHAKE[] = {
  "WHOA WHOA.", "EARTHQUAKE!", "STOP SHAKING!", "MY PIXELS!", "WHEEE... NO.",
  "I SEE STARS.", "BLENDER MODE?", "ROUGH NIGHT?", "TOO MUCH.",
};
static const char *const P_BUMP[] = {
  "OUCH!", "OOF.", "HEY!", "RUDE.", "WATCH IT.", "OW OW OW.", "THAT HURT.", "BONK.",
};
static const char *const P_DROP[] = {
  "AAAAH!", "I'M FALLING!", "CATCH ME!", "NOT AGAIN!", "MAYDAY!", "WHY?!",
};
static const char *const P_FLIP[] = {
  "UPSIDE DOWN?", "BLOOD TO HEAD.", "FLIP ME BACK.", "AUSSIE MODE.", "THIS IS WEIRD.",
};
static const char *const P_SLEEPY[] = {
  "YAWN.", "SO SLEEPY...", "5 MORE MIN.", "NAP TIME?", "ZZZ...", "BORED.",
};
static const char *const P_WAKE[] = {
  "HUH? WHAT?", "I WAS AWAKE.", "NOT SLEEPING.", "GOOD MORNING.", "OH, IT'S YOU.",
  "...", "WHERE AM I?",
};
static const char *const P_GRUMPY[] = {
  "HMPH.", "NOT NOW.", "GO AWAY.", "GRR.", "LET ME SLEEP.", "SERIOUSLY?",
};
static const char *const P_IDLE[] = {
  "NICE DAY.", "WHAT'S UP?", "I LIKE YOUR VIBE.", "TAKE ME OUT.", "WHERE TO?",
  "THINKING...", "HMM.", "LA LA LA.", "OH. A FLY.", "STAY HYDRATED.",
  "YOU GOT THIS.", "CUTE OUTFIT.", "SNACK TIME?", "BE NICE TODAY.", "DEEP BREATH.",
  "WANNA DANCE?", "HELLO? ANYONE?", "BEEP BOOP.", "DOOBIE TIME.", "SMELLS NICE.",
  "JUST WATCHING.", "PSST.", "HOLD ME.", "TAP ME, MAYBE?", "TICK TOCK.",
  "COOL STORY.", "WHAT'S THAT?", "GOOD VIBES ONLY.", "SO... HI.", "I'M TINY.",
};
static const char *const P_SUS[] = {
  "SUS.", "I SAW THAT.", "HMMMM.", "REALLY?", "SURE.", "SUSPICIOUS.",
};
static const char *const P_ROLL[] = {
  "UGH.", "WHATEVER.", "OH PLEASE.", "BORING.",
};
static const char *const P_CROSS[] = {
  "DERP.", "MY NOSE.", "CAN'T FOCUS.",
};
static const char *const P_WINK[] = {
  "*WINK*", ";)", "OUR SECRET.", "HEY HANDSOME.", "HEY GORGEOUS.",
};
static const char *const P_LOWBAT[] = {
  "FEED ME.", "LOW BATTERY.", "GETTING WEAK.", "CHARGE ME?", "SO HUNGRY.",
};

#define CATDEF(a) { a, (uint8_t)(sizeof(a) / sizeof(a[0])) }
struct CatList { const char *const *items; uint8_t n; };
static const CatList CATS[CAT_COUNT] = {
  CATDEF(P_GREET), CATDEF(P_TAP), CATDEF(P_LOVE), CATDEF(P_SHAKE), CATDEF(P_BUMP),
  CATDEF(P_DROP), CATDEF(P_FLIP), CATDEF(P_SLEEPY), CATDEF(P_WAKE), CATDEF(P_GRUMPY),
  CATDEF(P_IDLE), CATDEF(P_SUS), CATDEF(P_ROLL), CATDEF(P_CROSS), CATDEF(P_WINK),
  CATDEF(P_LOWBAT),
};

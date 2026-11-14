#pragma once
// ============================================================================
//  Doobie's vocabulary: dot-matrix snippets grouped by trigger, in three tiers
//    BASE   everyone (all ages)
//    ADULT  18+ profiles: flirty, pet names
//    SPICY  18+ AND opted in during onboarding: cheeky innuendo
//  Kids get BASE plus the KID pool. Nothing from ADULT/SPICY is ever picked
//  for an under-18 profile (see pickPool() in doobie.ino).
//
//  "{N}" is replaced by a pet name chosen for the profile.
//  ASCII only; "\x03" draws a heart. Keep lines under ~24 chars.
// ============================================================================

struct Pool { const char *const *items; uint8_t n; };
#define POOL(a) { a, (uint8_t)(sizeof(a) / sizeof(a[0])) }
#define NOPOOL  { nullptr, 0 }

// ---------------------------------------------------------------- BASE ------
static const char *const B_GREET[] = {
  "HEY THERE.", "HI.", "OH, HELLO.", "MISSED ME?", "HEY YOU.", "WELL HELLO.",
  "YOU AGAIN :)", "SUP.", "HOWDY.", "BACK SO SOON?", "HELLO, HUMAN.", "HI, {N}.",
};
static const char *const B_TAP[] = {
  "HEHE.", "BOOP.", "AGAIN!", "TICKLES.", "THAT'S NICE.", "OOH.", "YES?",
  "BOOP BOOP.", "MORE PLEASE.", "CHEEKY.", "I FELT THAT.", "HI, FINGER.",
};
static const char *const B_LOVE[] = {
  "\x03", "\x03 \x03 \x03", "MMMM.", "SO WARM.", "BEST HUMAN.", "PURR.",
  "LOVE THAT.", "STAY A WHILE.", "HUGS!", "YOU'RE THE BEST, {N}.",
};
static const char *const B_SHAKE[] = {
  "WHOA WHOA.", "EARTHQUAKE!", "STOP SHAKING!", "MY PIXELS!", "WHEEE... NO.",
  "I SEE STARS.", "BLENDER MODE?", "TOO MUCH.",
};
static const char *const B_BUMP[] = {
  "OUCH!", "OOF.", "HEY!", "RUDE.", "WATCH IT.", "OW OW OW.", "THAT HURT.", "BONK.",
};
static const char *const B_DROP[] = {
  "AAAAH!", "I'M FALLING!", "CATCH ME!", "NOT AGAIN!", "MAYDAY!", "WHY?!",
};
static const char *const B_FLIP[] = {
  "UPSIDE DOWN?", "BLOOD TO HEAD.", "FLIP ME BACK.", "AUSSIE MODE.", "THIS IS WEIRD.",
};
static const char *const B_SLEEPY[] = {
  "YAWN.", "SO SLEEPY...", "5 MORE MIN.", "NAP TIME?", "ZZZ...", "BORED.",
};
static const char *const B_WAKE[] = {
  "HUH? WHAT?", "I WAS AWAKE.", "NOT SLEEPING.", "GOOD MORNING.", "OH, IT'S YOU.",
  "...", "WHERE AM I?",
};
static const char *const B_GRUMPY[] = {
  "HMPH.", "NOT NOW.", "GO AWAY.", "GRR.", "LET ME SLEEP.", "SERIOUSLY?",
};
static const char *const B_IDLE[] = {
  "NICE DAY.", "WHAT'S UP?", "I LIKE YOUR VIBE.", "TAKE ME OUT.", "WHERE TO?",
  "THINKING...", "HMM.", "LA LA LA.", "OH. A FLY.", "STAY HYDRATED.",
  "YOU GOT THIS.", "SNACK TIME?", "BE NICE TODAY.", "DEEP BREATH.",
  "WANNA DANCE?", "HELLO? ANYONE?", "BEEP BOOP.", "DOOBIE TIME.", "SMELLS NICE.",
  "JUST WATCHING.", "PSST.", "HOLD ME.", "TAP ME, MAYBE?", "TICK TOCK.",
  "COOL STORY.", "WHAT'S THAT?", "GOOD VIBES ONLY.", "SO... HI.", "I'M TINY.",
};
static const char *const B_SUS[]   = { "SUS.", "I SAW THAT.", "HMMMM.", "REALLY?", "SURE.", "SUSPICIOUS." };
static const char *const B_ROLL[]  = { "UGH.", "WHATEVER.", "OH PLEASE.", "BORING." };
static const char *const B_CROSS[] = { "DERP.", "MY NOSE.", "CAN'T FOCUS." };
static const char *const B_WINK[]  = { "*WINK*", ";)", "HEY, {N}." };
static const char *const B_LOWBAT[] = { "FEED ME.", "LOW BATTERY.", "GETTING WEAK.", "CHARGE ME?", "SO HUNGRY." };

// microphone (only used when a mic is detected)
static const char *const B_QUIET[] = { "SO QUIET...", "LIBRARY VIBES.", "SHHH.", "HELLO? ECHO?", "PEACEFUL." };
static const char *const B_TALK[]  = { "WHO'S TALKING?", "I'M LISTENING.", "GOSSIP?", "TELL ME MORE.", "UH HUH.", "WHAT DID THEY SAY?" };
static const char *const B_LOUD[]  = { "TOO LOUD!", "MY EARS!", "INSIDE VOICE!", "KEEP IT DOWN.", "SO NOISY." };
static const char *const B_BANG[]  = { "WHOA!", "WHAT WAS THAT?!", "EEK!", "DON'T DO THAT!", "JUMPSCARE!" };
static const char *const B_PARTY[] = { "PARTY?!", "IS THAT MUSIC?", "DANCE WITH ME.", "TURN IT UP!", "BOP BOP BOP." };

// ------------------------------------------------------------------ KID -----
static const char *const K_IDLE[] = {
  "YOU'RE AWESOME.", "HIGH FIVE!", "DRINK SOME WATER.", "HOMEWORK DONE?",
  "LET'S PLAY!", "YOU'RE SO SMART.", "BEST FRIENDS?", "TELL ME A JOKE.",
  "I LIKE DINOS.", "BE KIND TODAY.",
};

// ---------------------------------------------------------------- ADULT -----
static const char *const A_GREET[] = { "HEY, {N}.", "LOOKING GOOD, {N}.", "WELL, HELLO {N}.", "THERE YOU ARE." };
static const char *const A_TAP[]   = { "OH, NICE FINGER.", "I LIKE THAT.", "DO THAT AGAIN.", "SMOOTH.", "OOH, {N}." };
static const char *const A_LOVE[]  = { "DON'T STOP.", "CLOSER...", "YOU'RE SWEET.", "OK, I LIKE YOU.", "MY FAVOURITE {N}." };
static const char *const A_IDLE[]  = { "CUTE OUTFIT.", "YOU LOOK GOOD.", "OUR SECRET.", "THINKING OF YOU.", "MISS ME YET?", "STOP STARING ;)" };
static const char *const A_WINK[]  = { "HEY, {N}.", "HI THERE, {N}.", "OUR LITTLE SECRET.", "CALL ME ;)" };

// ---------------------------------------------------------------- SPICY -----
static const char *const S_TAP[]   = {
  "RIGHT THERE.", "SLOWER...", "GENTLY...", "TOUCH ME AGAIN.", "YOU'RE TROUBLE.",
  "NAUGHTY.", "TEASE.", "BUY ME DINNER FIRST.", "I'M BLUSHING.", "FIRM. I LIKE IT.",
};
static const char *const S_LOVE[]  = {
  "DON'T YOU DARE STOP.", "IS IT HOT IN HERE?", "WHAT A HANDFUL.", "GET A ROOM? OK.",
  "MORE, {N}.", "YOU HAD ME AT TAP.",
};
static const char *const S_SHAKE[] = { "SHAKE IT, BABY.", "ROUGH. I SEE.", "EASY, TIGER.", "WILD ONE, HUH?" };
static const char *const S_BUMP[]  = { "OOH, ROUGH.", "OOH, FEISTY.", "CAREFUL, {N}.", "I BRUISE EASY." };
static const char *const S_FLIP[]  = { "UPSIDE DOWN? KINKY.", "NEW POSITION?", "OH, WE'RE DOING THIS?" };
static const char *const S_IDLE[]  = {
  "BEHAVE.", "NOT IN PUBLIC!", "WANNA GET OUT OF HERE?", "YOUR PLACE OR MINE?",
  "I'VE BEEN BAD.", "WINK WINK.", "NICE VIEW FROM HERE.", "TAKE IT SLOW.",
  "SAY MY NAME.", "LATER, {N}. ;)",
};
static const char *const S_LOUD[]  = { "KEEP IT DOWN, TIGER.", "SCREAMER, HUH?", "OOH, LOUD ONE." };
static const char *const S_QUIET[] = { "JUST US NOW...", "FINALLY ALONE.", "SO... HI ;)" };

// ---------------------------------------------------------------- EVENTS ----
enum Ev : uint8_t {
  EV_GREET, EV_TAP, EV_LOVE, EV_SHAKE, EV_BUMP, EV_DROP, EV_FLIP, EV_SLEEPY, EV_WAKE,
  EV_GRUMPY, EV_IDLE, EV_SUS, EV_ROLL, EV_CROSS, EV_WINK, EV_LOWBAT,
  EV_QUIET, EV_TALK, EV_LOUD, EV_BANG, EV_PARTY, EV_COUNT
};
struct EvPools { Pool base, adult, spicy; };
static const EvPools EVENTS[EV_COUNT] = {
  /* GREET  */ { POOL(B_GREET),  POOL(A_GREET), NOPOOL },
  /* TAP    */ { POOL(B_TAP),    POOL(A_TAP),   POOL(S_TAP) },
  /* LOVE   */ { POOL(B_LOVE),   POOL(A_LOVE),  POOL(S_LOVE) },
  /* SHAKE  */ { POOL(B_SHAKE),  NOPOOL,        POOL(S_SHAKE) },
  /* BUMP   */ { POOL(B_BUMP),   NOPOOL,        POOL(S_BUMP) },
  /* DROP   */ { POOL(B_DROP),   NOPOOL,        NOPOOL },
  /* FLIP   */ { POOL(B_FLIP),   NOPOOL,        POOL(S_FLIP) },
  /* SLEEPY */ { POOL(B_SLEEPY), NOPOOL,        NOPOOL },
  /* WAKE   */ { POOL(B_WAKE),   NOPOOL,        NOPOOL },
  /* GRUMPY */ { POOL(B_GRUMPY), NOPOOL,        NOPOOL },
  /* IDLE   */ { POOL(B_IDLE),   POOL(A_IDLE),  POOL(S_IDLE) },
  /* SUS    */ { POOL(B_SUS),    NOPOOL,        NOPOOL },
  /* ROLL   */ { POOL(B_ROLL),   NOPOOL,        NOPOOL },
  /* CROSS  */ { POOL(B_CROSS),  NOPOOL,        NOPOOL },
  /* WINK   */ { POOL(B_WINK),   POOL(A_WINK),  NOPOOL },
  /* LOWBAT */ { POOL(B_LOWBAT), NOPOOL,        NOPOOL },
  /* QUIET  */ { POOL(B_QUIET),  NOPOOL,        POOL(S_QUIET) },
  /* TALK   */ { POOL(B_TALK),   NOPOOL,        NOPOOL },
  /* LOUD   */ { POOL(B_LOUD),   NOPOOL,        POOL(S_LOUD) },
  /* BANG   */ { POOL(B_BANG),   NOPOOL,        NOPOOL },
  /* PARTY  */ { POOL(B_PARTY),  NOPOOL,        NOPOOL },
};
static const Pool KID_IDLE = POOL(K_IDLE);

// pet names, by journey
static const char *const PET_KID[]    = { "BUDDY", "CHAMP", "FRIEND", "PAL" };
static const char *const PET_MALE[]   = { "HANDSOME", "TIGER", "STUD", "MISTER" };
static const char *const PET_FEMALE[] = { "GORGEOUS", "BEAUTIFUL", "BABE", "QUEEN" };
static const char *const PET_ANY[]    = { "CUTIE", "SUNSHINE", "STAR", "LEGEND" };

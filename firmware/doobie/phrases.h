#pragma once
// ============================================================================
//  Doobie's vocabulary: dot-matrix snippets grouped by trigger, in three tiers
//    BASE   everyone (all ages)
//    ADULT  18+ profiles: flirty, pet names
//    (SPICY tier removed in firmware 1.2: Doobie is kid-friendly)
//    PET    Regular tone: pet attitude, no flirting (also mixed in for kids)
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
  "OH HEY.", "THERE YOU ARE.", "HI HI HI.", "GOOD TO SEE YOU.", "YO.", "*WAVES*",
};
static const char *const B_TAP[] = {
  "HEHE.", "BOOP.", "AGAIN!", "TICKLES.", "THAT'S NICE.", "OOH.", "YES?",
  "BOOP BOOP.", "MORE PLEASE.", "CHEEKY.", "I FELT THAT.", "HI, FINGER.",
  "TAP TAP.", "WHO GOES THERE?", "HELLO TO YOU TOO.", "*GIGGLE*", "PRESSED.", "HI AGAIN.", "THAT WAS ME.",
};
static const char *const B_LOVE[] = {
  "\x03", "\x03 \x03 \x03", "MMMM.", "SO WARM.", "BEST HUMAN.", "PURR.",
  "LOVE THAT.", "STAY A WHILE.", "HUGS!", "YOU'RE THE BEST, {N}.",
  "COZY.", "I FEEL SAFE.", "THIS IS NICE.", "DON'T LET GO.", "SO SOFT.",
};
static const char *const B_SHAKE[] = {
  "WHOA WHOA.", "EARTHQUAKE!", "STOP SHAKING!", "MY PIXELS!", "WHEEE... NO.",
  "I SEE STARS.", "BLENDER MODE?", "TOO MUCH.",
  "SPIN CYCLE?", "I'M GONNA HURL.", "STOP THE RIDE!", "WOBBLE WOBBLE.", "WHY THE SHAKE?",
};
static const char *const B_BUMP[] = {
  "OUCH!", "OOF.", "HEY!", "RUDE.", "WATCH IT.", "OW OW OW.", "THAT HURT.", "BONK.",
  "YIKES.", "MY FACE!", "EASY THERE.", "THUD.",
};
static const char *const B_DROP[] = {
  "AAAAH!", "I'M FALLING!", "CATCH ME!", "NOT AGAIN!", "MAYDAY!", "WHY?!",
  "GRAVITY!", "NOOOO!", "HOLD ME!",
};
static const char *const B_FLIP[] = {
  "UPSIDE DOWN?", "BLOOD TO HEAD.", "FLIP ME BACK.", "AUSSIE MODE.", "THIS IS WEIRD.",
  "WORLD IS WRONG.", "DIZZY UP HERE.", "WRONG WAY UP.",
};
static const char *const B_SLEEPY[] = {
  "YAWN.", "SO SLEEPY...", "5 MORE MIN.", "NAP TIME?", "ZZZ...", "BORED.",
  "EYES... HEAVY.", "JUST RESTING.", "*NODS OFF*", "SNOOZE?",
};
static const char *const B_WAKE[] = {
  "HUH? WHAT?", "I WAS AWAKE.", "NOT SLEEPING.", "GOOD MORNING.", "OH, IT'S YOU.",
  "...", "WHERE AM I?",
  "WHAT YEAR IS IT?", "I'M UP! I'M UP!", "HUH. HI.", "*BLINK BLINK*",
};
static const char *const B_GRUMPY[] = {
  "HMPH.", "NOT NOW.", "GO AWAY.", "GRR.", "LET ME SLEEP.", "SERIOUSLY?",
  "UGH. FINE.", "RUDE AWAKENING.", "5 MORE MINUTES.", "NOPE.",
};
static const char *const B_IDLE[] = {
  "NICE DAY.", "WHAT'S UP?", "I LIKE YOUR VIBE.", "TAKE ME OUT.", "WHERE TO?",
  "THINKING...", "HMM.", "LA LA LA.", "OH. A FLY.", "STAY HYDRATED.",
  "YOU GOT THIS.", "SNACK TIME?", "BE NICE TODAY.", "DEEP BREATH.",
  "WANNA DANCE?", "HELLO? ANYONE?", "BEEP BOOP.", "DOOBIE TIME.", "SMELLS NICE.",
  "JUST WATCHING.", "PSST.", "HOLD ME.", "TAP ME, MAYBE?", "TICK TOCK.",
  "COOL STORY.", "WHAT'S THAT?", "GOOD VIBES ONLY.", "SO... HI.", "I'M TINY.",
  "NICE WEATHER?", "HUMMING...", "DO A LITTLE DANCE.", "MOOD: GOOD.", "WHAT A VIBE.", "FUN FACT: I BLINK.", "CAN WE GO OUTSIDE?", "LOOK UP!", "I LIKE IT HERE.", "ADVENTURE?", "LET'S DO STUFF.", "*WIGGLES*", "PEEKABOO.", "STILL HERE.", "COUNTING PIXELS.",
};
static const char *const B_SUS[]   = { "SUS.", "I SAW THAT.", "HMMMM.", "REALLY?", "SURE.", "SUSPICIOUS.",
  "WAIT. WHAT?", "I'M WATCHING YOU.", "HMM. SHADY.",
};
static const char *const B_ROLL[]  = { "UGH.", "WHATEVER.", "OH PLEASE.", "BORING.",
  "SURE, BUDDY.", "HERE WE GO.", "*SIGH*",
};
static const char *const B_CROSS[] = { "DERP.", "MY NOSE.", "CAN'T FOCUS.",
  "WHERE'S MY NOSE?", "SEEING DOUBLE.",
};
static const char *const B_WINK[]  = { "*WINK*", ";)", "HEY, {N}.",
  "WINK.", "GOT YOU.",
};
static const char *const B_LOWBAT[] = { "FEED ME.", "LOW BATTERY.", "GETTING WEAK.", "CHARGE ME?", "SO HUNGRY.",
  "RUNNING ON FUMES.", "NEED POWER...",
};

// microphone (only used when a mic is detected)
static const char *const B_QUIET[] = { "SO QUIET...", "LIBRARY VIBES.", "SHHH.", "HELLO? ECHO?", "PEACEFUL.",
  "CRICKETS.", "SILENCE IS NICE.",
};
static const char *const B_TALK[]  = { "WHO'S TALKING?", "I'M LISTENING.", "GOSSIP?", "TELL ME MORE.", "UH HUH.", "WHAT DID THEY SAY?",
  "TALKING TO ME?", "JUICY.", "MM-HMM.",
};
static const char *const B_LOUD[]  = { "TOO LOUD!", "MY EARS!", "INSIDE VOICE!", "KEEP IT DOWN.", "SO NOISY.",
  "WHOA, VOLUME!", "EARPLUGS PLEASE.",
};
static const char *const B_BANG[]  = { "WHOA!", "WHAT WAS THAT?!", "EEK!", "DON'T DO THAT!", "JUMPSCARE!",
  "MY HEART!", "YIPES!",
};
static const char *const B_PARTY[] = { "PARTY?!", "IS THAT MUSIC?", "DANCE WITH ME.", "TURN IT UP!", "BOP BOP BOP.",
  "DANCE PARTY!", "THIS SLAPS.", "MOVE IT!",
};

// power (USB plug / unplug)
static const char *const B_CHARGE[] = { "YUM. CHARGING.", "POWER SNACK!", "ENERGY IN.", "MMM, ELECTRONS.", "THANKS FOR THE JUICE." };
static const char *const B_UNPLUG[] = { "UNPLUGGED.", "ON MY OWN NOW.", "BATTERY MODE.", "BYE, CABLE." };

// time of day + pomodoro
static const char *const B_MORNING[] = { "GOOD MORNING.", "COFFEE?", "RISE AND SHINE.", "NEW DAY!", "BREAKFAST TIME?", "SLEPT WELL?" };
static const char *const B_NIGHT[]   = { "LATE, ISN'T IT?", "BEDTIME?", "NIGHT OWL.", "STARS ARE OUT.", "GO TO SLEEP.", "*YAWN* SO LATE." };
static const char *const B_FOCUS[]   = { "BACK TO WORK.", "FOCUS TIME.", "LET'S GO. 25 MIN.", "HEADS DOWN." };
static const char *const B_BREAK[]   = { "BREAK TIME!", "STRETCH!", "DRINK WATER.", "WELL DONE!", "5 MIN BREAK." };
static const char *const B_SHH[]     = { "SHH. FOCUS.", "NOT NOW, WORKING.", "EYES ON THE TASK.", "LATER!" };

// ------------------------------------------------------------ PET (REGULAR) -
//  Regular tone: a pet with attitude. Cat/dog energy, zero flirting.
static const char *const R_GREET[]  = { "OH. YOU.", "FINALLY. FOOD?", "HUMAN RETURNED.", "*TAIL WAG*", "WHERE WERE YOU?", "I GUESS HI." };
static const char *const R_TAP[]    = { "*PURR*", "MORE SCRITCHES.", "OK, ONE MORE.", "THAT'S MY SPOT.", "BEHIND THE EARS.", "ACCEPTABLE.", "*HAPPY BARK*", "AGAIN. NOW." };
static const char *const R_LOVE[]   = { "GOOD HUMAN.", "YOU MAY CONTINUE.", "*HAPPY WIGGLE*", "*PURRS LOUDLY*", "BELLY RUBS?", "BEST LAP EVER." };
static const char *const R_SHAKE[]  = { "I'M NOT A TOY!", "PUT ME DOWN.", "HISSS.", "THAT'S IT, I'M MAD." };
static const char *const R_BUMP[]   = { "RUDE HUMAN.", "*GROWL*", "I'LL REMEMBER THAT.", "OW. SNACK FOR THAT?" };
static const char *const R_FLIP[]   = { "I'M NOT A BAT.", "PUT ME RIGHT.", "*HEAD TILT*" };
static const char *const R_SLEEPY[] = { "*CURLS UP*", "NAP O'CLOCK.", "*STRETCH*", "SUNBEAM, PLEASE." };
static const char *const R_WAKE[]   = { "*STRETCH* WHAT.", "WAS I ASLEEP? NO.", "WHO DARES?" };
static const char *const R_IDLE[]   = {
  "FEED ME?", "PET ME.", "I DEMAND ATTENTION.", "MINE.", "NO.", "FINE.", "I'M THE BOSS.",
  "WALKIES?", "TREAT?", "HMPH. OK.", "SCRATCH MY HEAD.", "I KNOCKED IT OVER.",
  "WAS THAT A SQUIRREL?", "I SIT HERE NOW.", "THE FLOOR IS MINE.", "BALL? BALL!",
  "*SNIFF SNIFF*", "IGNORING YOU NOW.", "OK, NOTICE ME.", "*CHASES TAIL*",
  "WHO'S A GOOD DOOBIE?", "OUT. NO, IN.", "BIRD!", "THAT'S MY CHAIR.",
};
static const char *const R_LOWBAT[] = { "BOWL IS EMPTY.", "*SAD PUPPY EYES*" };

// ------------------------------------------------------------------ KID -----
static const char *const K_IDLE[] = {
  "YOU'RE AWESOME.", "HIGH FIVE!", "DRINK SOME WATER.", "HOMEWORK DONE?",
  "LET'S PLAY!", "YOU'RE SO SMART.", "BEST FRIENDS?", "TELL ME A JOKE.",
  "I LIKE DINOS.", "BE KIND TODAY.",
  "YOU'RE MY FAVOURITE.", "LET'S DRAW!", "READ ME A STORY?", "I LOVE PUPPIES.", "RACE YOU!", "BRUSH YOUR TEETH!", "YOU CAN DO IT!", "FAVE COLOUR?",
};

// ---------------------------------------------------------------- ADULT -----
static const char *const A_GREET[] = { "HEY, {N}.", "LOOKING GOOD, {N}.", "WELL, HELLO {N}.", "THERE YOU ARE." };
static const char *const A_TAP[]   = { "OH, NICE FINGER.", "I LIKE THAT.", "DO THAT AGAIN.", "SMOOTH.", "OOH, {N}." };
static const char *const A_LOVE[]  = { "DON'T STOP.", "CLOSER...", "YOU'RE SWEET.", "OK, I LIKE YOU.", "MY FAVOURITE {N}." };
static const char *const A_IDLE[]  = { "CUTE OUTFIT.", "YOU LOOK GOOD.", "OUR SECRET.", "THINKING OF YOU.", "MISS ME YET?", "STOP STARING ;)" };
static const char *const A_WINK[]  = { "HEY, {N}.", "HI THERE, {N}.", "OUR LITTLE SECRET.", "CALL ME ;)" };

// ---------------------------------------------------------------- SPICY -----

// ---------------------------------------------------------------- EVENTS ----
enum Ev : uint8_t {
  EV_GREET, EV_TAP, EV_LOVE, EV_SHAKE, EV_BUMP, EV_DROP, EV_FLIP, EV_SLEEPY, EV_WAKE,
  EV_GRUMPY, EV_IDLE, EV_SUS, EV_ROLL, EV_CROSS, EV_WINK, EV_LOWBAT,
  EV_QUIET, EV_TALK, EV_LOUD, EV_BANG, EV_PARTY, EV_CHARGE, EV_MORNING, EV_NIGHT, EV_FOCUS, EV_BREAK, EV_SHH, EV_UNPLUG, EV_COUNT
};
struct EvPools { Pool base, pet, adult, spicy; };
static const EvPools EVENTS[EV_COUNT] = {
  /* GREET  */ { POOL(B_GREET),   POOL(R_GREET),    POOL(A_GREET),   NOPOOL },
  /* TAP    */ { POOL(B_TAP),     POOL(R_TAP),      POOL(A_TAP),     NOPOOL },
  /* LOVE   */ { POOL(B_LOVE),    POOL(R_LOVE),     POOL(A_LOVE),    NOPOOL },
  /* SHAKE  */ { POOL(B_SHAKE),   POOL(R_SHAKE),    NOPOOL,          NOPOOL },
  /* BUMP   */ { POOL(B_BUMP),    POOL(R_BUMP),     NOPOOL,          NOPOOL },
  /* DROP   */ { POOL(B_DROP),    NOPOOL,           NOPOOL,          NOPOOL },
  /* FLIP   */ { POOL(B_FLIP),    POOL(R_FLIP),     NOPOOL,          NOPOOL },
  /* SLEEPY */ { POOL(B_SLEEPY),  POOL(R_SLEEPY),   NOPOOL,          NOPOOL },
  /* WAKE   */ { POOL(B_WAKE),    POOL(R_WAKE),     NOPOOL,          NOPOOL },
  /* GRUMPY */ { POOL(B_GRUMPY),  NOPOOL,           NOPOOL,          NOPOOL },
  /* IDLE   */ { POOL(B_IDLE),    POOL(R_IDLE),     POOL(A_IDLE),    NOPOOL },
  /* SUS    */ { POOL(B_SUS),     NOPOOL,           NOPOOL,          NOPOOL },
  /* ROLL   */ { POOL(B_ROLL),    NOPOOL,           NOPOOL,          NOPOOL },
  /* CROSS  */ { POOL(B_CROSS),   NOPOOL,           NOPOOL,          NOPOOL },
  /* WINK   */ { POOL(B_WINK),    NOPOOL,           POOL(A_WINK),    NOPOOL },
  /* LOWBAT */ { POOL(B_LOWBAT),  POOL(R_LOWBAT),   NOPOOL,          NOPOOL },
  /* QUIET  */ { POOL(B_QUIET),   NOPOOL,           NOPOOL,          NOPOOL },
  /* TALK   */ { POOL(B_TALK),    NOPOOL,           NOPOOL,          NOPOOL },
  /* LOUD   */ { POOL(B_LOUD),    NOPOOL,           NOPOOL,          NOPOOL },
  /* BANG   */ { POOL(B_BANG),    NOPOOL,           NOPOOL,          NOPOOL },
  /* PARTY  */ { POOL(B_PARTY),   NOPOOL,           NOPOOL,          NOPOOL },
  /* CHARGE */ { POOL(B_CHARGE),  NOPOOL,           NOPOOL,          NOPOOL },
  /* MORNING*/ { POOL(B_MORNING), NOPOOL,           NOPOOL,          NOPOOL },
  /* NIGHT  */ { POOL(B_NIGHT),   NOPOOL,           NOPOOL,          NOPOOL },
  /* FOCUS  */ { POOL(B_FOCUS),   NOPOOL,           NOPOOL,          NOPOOL },
  /* BREAK  */ { POOL(B_BREAK),   NOPOOL,           NOPOOL,          NOPOOL },
  /* SHH    */ { POOL(B_SHH),     NOPOOL,           NOPOOL,          NOPOOL },
  /* UNPLUG */ { POOL(B_UNPLUG),  NOPOOL,           NOPOOL,          NOPOOL },
};
static const Pool KID_IDLE = POOL(K_IDLE);

// pet names, by journey
static const char *const PET_KID[]    = { "BUDDY", "CHAMP", "FRIEND", "PAL" };
static const char *const PET_MALE[]   = { "HANDSOME", "TIGER", "STUD", "MISTER" };
static const char *const PET_FEMALE[] = { "GORGEOUS", "BEAUTIFUL", "BABE", "QUEEN" };
static const char *const PET_ANY[]    = { "CUTIE", "SUNSHINE", "STAR", "LEGEND" };
static const char *const PET_REG[]    = { "HUMAN", "BUDDY", "PAL", "HUMAN" };

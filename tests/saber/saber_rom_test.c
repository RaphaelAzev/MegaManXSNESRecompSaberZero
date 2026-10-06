/* Saber-owned reference checks for the unmodified X3 Zero and native X1
 * weapon paths. The runner supplies a copied catalog, an isolated cache, the
 * original ROMs, and the standing Highway save0 fixture. */
#define MMX_DESKTOP_ENTRY MmxDesktopMain
#include "desktop/host_main.c"
#include MMX_GAME_MAIN
#include "mmx_zero.h"
#include "mmx_weapons.h"
#include "saber/mmx_saber_attack.h"
#include "saber/mmx_saber_frame.h"
#include "saber/mmx_saber_plugin.h"
#include "saber/mmx_saber_sfx.h"
#include "mod_runtime.h"
#include "recomp_launcher.h"
#include "snes/interp_bridge.h"

enum {
  SABER_CHARGE_TIER_1_FRAME = 21,
  SABER_CHARGE_TIER_2_FRAME = 81,
  SABER_CHARGE_TIER_3_FRAME = 141,
  SABER_CHARGE_FULL_FRAME = 201,
  SABER_TIER_4_RELEASE_CLASS = 1,
  SABER_TIER_6_RELEASE_CLASS = 3,
  SABER_TIER_8_RELEASE_CLASS = 3,
  SABER_FULL_RELEASE_CLASS = 3,
  SABER_FULL_RELEASE_SHOTS = 2,
  SABER_FIRE_WAVE_FRAMES = 90,
  SABER_FIRE_WAVE_BIRTHS = 4,
  SABER_FIRE_WAVE_LIVE_FRAMES = 41,
  SABER_FIRE_WAVE_LIVE_SAMPLES = 85,
  SABER_FIRE_WAVE_PEAK = 4,
  /* These are measured from the pure upstream X3 Zero group below. */
  X3_ZERO_FIRE_WAVE_BIRTHS = 4,
  X3_ZERO_FIRE_WAVE_LIVE_FRAMES = 39,
  X3_ZERO_FIRE_WAVE_LIVE_SAMPLES = 79,
  X3_ZERO_FIRE_WAVE_PEAK = 4,
  SABER_ONE_SHOT_FRAMES = 60,
  SABER_ONE_SHOT_PROJECTILES = 1,
  X3_ZERO_STORM_TORNADO_PROJECTILES = 1,
  /* X1's native command-6 charged buster path publishes class 2. */
  SABER_X1_CHARGED_RELEASE_CLASS = 2,
  /* Independent oracle copied from oldsaber/saber-zero-variant:
   * src/mmx_saber.c:266-268. Keep these literals separate from the new table
   * so a timing-table mutation cannot make the test pass. */
  OLD_SABER_GROUND1_STARTUP = 4,
  OLD_SABER_GROUND1_ACTIVE = 8,
  OLD_SABER_GROUND1_RECOVERY = 18,
  OLD_SABER_GROUND1_TOTAL = 30,
};

static const char *const kMmxRomDigest =
    "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32";

static unsigned projectiles(unsigned kind);
static bool saber_track_x1_charged;
static bool saber_saw_x1_charged;

typedef struct {
  unsigned births;
  unsigned live_frames;
  unsigned live_samples;
  unsigned peak;
} FireWaveCounts;

typedef struct {
  FireWaveCounts fire_wave;
  unsigned storm_tornado_projectiles;
} SpecialCounts;

static void check(int ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", what);
    exit(1);
  }
  printf("ok: %s\n", what);
}

static int readable_file(const char *path) {
  FILE *f = path ? fopen(path, "rb") : NULL;
  if (!f) return 0;
  fclose(f);
  return 1;
}

static void frame(unsigned input) {
  MmxBeforeFrame();
  RtlRunFrame(input | (1u << 30));
  CaptureSimulationFrame(1);
  if (saber_track_x1_charged && projectiles(SABER_X1_CHARGED_RELEASE_CLASS))
    saber_saw_x1_charged = true;
}

static void idle(unsigned count) {
  while (count--) frame(0);
}

static unsigned projectiles(unsigned kind) {
  unsigned count = 0;
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    count += g_ram[d] && g_ram[d + 10] == kind;
  return count;
}

static unsigned all_projectiles(void) {
  unsigned count = 0;
  for (unsigned d = 0x1228; d < 0x1428; d += 64) count += g_ram[d] != 0;
  return count;
}

static void shot_presence(unsigned kind, unsigned char present[8]) {
  for (unsigned i = 0; i < 8; ++i) {
    unsigned d = 0x1228 + i * 64;
    present[i] = (unsigned char)(g_ram[d] && g_ram[d + 10] == kind);
  }
}

static unsigned new_projectiles(unsigned kind, unsigned char previous[8]) {
  unsigned char now[8];
  unsigned births = 0;
  shot_presence(kind, now);
  for (unsigned i = 0; i < 8; ++i) births += now[i] && !previous[i];
  memcpy(previous, now, sizeof(now));
  return births;
}

static void load_fixture(const char *fixture) {
  check(RtlLoadSnapshot(fixture), "save0.sav loads");
  check(MmxZeroActive() && !MmxZeroModern(), "fixture runs as X3 Zero");
  check(g_ram[0xba9] == 2 && g_ram[0xbaa] == 0 && g_ram[0xbdb] == 0 &&
            !g_ram[0x1f99] && (g_ram[0xbd3] & 4),
        "fixture is Highway standing with the buster selected");
  MmxZeroCancel(g_ram);
}

static void hold_charge_button(unsigned frames, unsigned button) {
  while (frames--) frame(button);
}

static void release_charge_button(unsigned frames, unsigned button) {
  hold_charge_button(frames, button);
  frame(0);
}

static void hold_charge(unsigned frames) {
  hold_charge_button(frames, SNES_PAD_Y);
}

static void release_charge(unsigned frames) {
  release_charge_button(frames, SNES_PAD_Y);
}

static unsigned read_ram_word(const uint8_t *ram, unsigned offset) {
  return ram[offset] | (unsigned)ram[offset + 1] << 8;
}

static struct {
  unsigned pre_calls, end_calls, frame_counter;
  unsigned pre_frames[32], end_frames[32];
  uint16_t first_pre_x, last_end_x;
  bool have_end, order_ok, movement_seen, slide_precondition_seen;
} zero_extension_observer;

static bool zero_extension_solid(const uint8_t *ram, int x, int y) {
  (void)ram;
  (void)x;
  (void)y;
  return true;
}

static void zero_extension_pre_player(uint8_t *ram) {
  unsigned call = zero_extension_observer.pre_calls++;
  if (call < 32) zero_extension_observer.pre_frames[call] = zero_extension_observer.frame_counter;
  unsigned x = read_ram_word(ram, 0xbad);
  if (ram[0xbaa] == 0 && read_ram_word(ram, 0xbc8) == 0xa552)
    zero_extension_observer.slide_precondition_seen = true;
  if (!zero_extension_observer.have_end) zero_extension_observer.first_pre_x = (uint16_t)x;
  else if (x != zero_extension_observer.last_end_x) zero_extension_observer.order_ok = false;
}

static void zero_extension_player_end(uint8_t *ram) {
  unsigned call = zero_extension_observer.end_calls++;
  if (call < 32) zero_extension_observer.end_frames[call] = zero_extension_observer.frame_counter;
  unsigned x = read_ram_word(ram, 0xbad);
  if (zero_extension_observer.have_end && x != zero_extension_observer.last_end_x)
    zero_extension_observer.movement_seen = true;
  if (!zero_extension_observer.have_end && x != zero_extension_observer.first_pre_x)
    zero_extension_observer.movement_seen = true;
  zero_extension_observer.last_end_x = (uint16_t)x;
  zero_extension_observer.have_end = true;
}

static unsigned zero_extension_intent_calls;
static bool zero_extension_saw_mapped_charge;

static bool zero_extension_intent_override(const uint8_t *ram, MmxZeroLegacyIntent *intent) {
  if ((ram[0xbdf] & 64) || (ram[0xbe3] & 64)) zero_extension_saw_mapped_charge = true;
  unsigned call = zero_extension_intent_calls++;
  if (call < 30) {
    intent->held = true;
    intent->pressed = false;
    intent->released = false;
  } else {
    intent->held = false;
    intent->pressed = false;
    intent->released = true;
  }
  return true;
}

static bool zero_extension_intent_reject(const uint8_t *ram, MmxZeroLegacyIntent *intent) {
  if ((ram[0xbdf] & 64) || (ram[0xbe3] & 64)) zero_extension_saw_mapped_charge = true;
  intent->held = true;
  intent->pressed = false;
  intent->released = false;
  return false;
}

static void zero_extension_checks(const char *fixture) {
  static const MmxZeroExtension hooks = {
    .pre_player = zero_extension_pre_player,
    .player_end = zero_extension_player_end,
  };
  static const MmxZeroExtension intent_hooks = {
    .legacy_intent = zero_extension_intent_override,
  };
  static const MmxZeroExtension reject_hooks = {
    .legacy_intent = zero_extension_intent_reject,
  };

  load_fixture(fixture);
  memset(&zero_extension_observer, 0, sizeof(zero_extension_observer));
  zero_extension_observer.order_ok = true;
  MmxZeroSetTerrainQuery(zero_extension_solid);
  g_ram[0xbaa] = 0;
  g_ram[0xbc8] = 0x52;
  g_ram[0xbc9] = 0xa5;
  MmxZeroSetExtension(&hooks);
  for (unsigned i = 0; i < 30; ++i) {
    zero_extension_observer.frame_counter = i;
    frame(SNES_PAD_RIGHT);
  }
  check(zero_extension_observer.pre_calls == 30 && zero_extension_observer.end_calls == 30,
        "zero extension calls pre-player and player-end once per frame");
  bool frame_records_match = true;
  for (unsigned i = 0; i < 30; ++i)
    if (zero_extension_observer.pre_frames[i] != i || zero_extension_observer.end_frames[i] != i)
      frame_records_match = false;
  check(frame_records_match, "zero extension callbacks record each frame in order");
  check(zero_extension_observer.order_ok && zero_extension_observer.movement_seen &&
            zero_extension_observer.slide_precondition_seen,
        "zero extension pre-player runs before native rightward movement");

  MmxZeroRegisterHooks();
  MmxZeroSetExtension(NULL);
  load_fixture(fixture);
  hold_charge(30);
  unsigned physical_charge = MmxZeroGetState().charge;
  frame(0);
  unsigned physical_class = projectiles(SABER_TIER_4_RELEASE_CLASS);
  check(physical_charge == 30 && physical_class == 1,
        "physical 30-frame charge establishes the legacy release reference");

  load_fixture(fixture);
  zero_extension_intent_calls = 0;
  zero_extension_saw_mapped_charge = false;
  MmxZeroSetExtension(&intent_hooks);
  for (unsigned i = 0; i < 30; ++i) frame(0);
  unsigned virtual_charge = MmxZeroGetState().charge;
  check(!zero_extension_saw_mapped_charge && virtual_charge == physical_charge,
        "legacy intent override charges without a physical charge button");
  frame(0);
  check(projectiles(SABER_TIER_4_RELEASE_CLASS) == physical_class &&
            !projectiles(2) && !projectiles(3),
        "legacy released intent fires the physical release shot class");

  load_fixture(fixture);
  zero_extension_intent_calls = 0;
  zero_extension_saw_mapped_charge = false;
  MmxZeroSetExtension(&reject_hooks);
  hold_charge(30);
  unsigned rejected_charge = MmxZeroGetState().charge;
  frame(0);
  check(zero_extension_saw_mapped_charge && rejected_charge == physical_charge &&
            projectiles(SABER_TIER_4_RELEASE_CLASS) == physical_class,
        "legacy intent callback returning false preserves mapped behavior");

  MmxZeroSetExtension(NULL);
}

static void select_native_weapon(unsigned weapon) {
  check(weapon >= 1 && weapon <= 8, "native weapon index is valid");
  g_ram[0x1f85 + weapon * 2] = 0;
  g_ram[0x1f86 + weapon * 2] = 0xdc;
  for (unsigned i = 0; i < 6; ++i) frame(SNES_PAD_R);
  frame(0);
  check(g_ram[0xbdb] == weapon * 2, "native weapon selection reaches the requested slot");
}

static void switch_to_x(void) {
  frame(SNES_PAD_SELECT);
  check(MmxZeroSwapping(), "Select starts the native X/Zero exchange");
  for (unsigned i = 0; i < 180 && MmxZeroSwapping(); ++i) frame(0);
  check(!MmxZeroSwapping() && !MmxZeroActive(), "native exchange arrives as X");
}

static void x3_plain_checks(const char *fixture, unsigned button) {
  load_fixture(fixture);
  unsigned char previous[8] = {0};
  unsigned births = 0;
  for (unsigned tap = 0; tap < 3; ++tap) {
    frame(button);
    births += new_projectiles(0, previous);
    frame(0);
    births += new_projectiles(0, previous);
    idle(40);
    births += new_projectiles(0, previous);
  }
  check(births == 3, "X3 Zero repeated buster taps spawn one plain shot each");
  check(projectiles(0) <= 1 && all_projectiles() <= 1,
        "X3 Zero plain taps do not become charged or duplicate shots");
  puts("ok: x3-zero-plain");
}

static void x3_charge_checks(const char *fixture, unsigned button) {
  static const struct {
    unsigned frames, charge, tier;
  } checkpoints[] = {
    {20, 20, 0},
    {SABER_CHARGE_TIER_1_FRAME, 21, 4},
    {80, 80, 4},
    {SABER_CHARGE_TIER_2_FRAME, 81, 6},
    {140, 140, 6},
    {SABER_CHARGE_TIER_3_FRAME, 141, 8},
    {200, 200, 8},
    {SABER_CHARGE_FULL_FRAME, 201, 10},
  };
  for (unsigned i = 0; i < sizeof(checkpoints) / sizeof(checkpoints[0]); ++i) {
    load_fixture(fixture);
    hold_charge_button(checkpoints[i].frames, button);
    MmxZeroState state = MmxZeroGetState();
    char label[96];
    snprintf(label, sizeof(label), "X3 Zero charge checkpoint %u reaches %u (tier %u)",
             checkpoints[i].frames, checkpoints[i].charge, checkpoints[i].tier);
    check(state.charge == checkpoints[i].charge && MmxZeroChargeTier(&state) == checkpoints[i].tier,
          label);
  }

  load_fixture(fixture);
  release_charge_button(30, button);
  check(projectiles(SABER_TIER_4_RELEASE_CLASS) == 1 &&
            !projectiles(2) && !projectiles(3),
        "tier-4 release fires exactly one class-1 buster projectile");

  load_fixture(fixture);
  release_charge_button(90, button);
  check(projectiles(SABER_TIER_6_RELEASE_CLASS) == 1 &&
            !projectiles(1) && !projectiles(2),
        "tier-6 release fires exactly one class-3 buster projectile");

  load_fixture(fixture);
  release_charge_button(150, button);
  check(MmxZeroGetState().combo == 1 && !MmxZeroGetState().saber_ready,
        "tier-8 release stores one X3 charged shot without saber readiness");
  idle(30);
  check(projectiles(SABER_TIER_8_RELEASE_CLASS) == 1 &&
            !projectiles(SABER_X1_CHARGED_RELEASE_CLASS),
        "tier-8 release emits one class-3 buster projectile");

  load_fixture(fixture);
  release_charge_button(SABER_CHARGE_FULL_FRAME, button);
  check(MmxZeroGetState().combo == 1 && MmxZeroGetState().saber_ready,
        "full release stores the X3 two-shot combo and saber readiness");
  idle(17);
  frame(button);
  idle(9);
  check(projectiles(SABER_FULL_RELEASE_CLASS) == SABER_FULL_RELEASE_SHOTS &&
            MmxZeroGetState().combo == 2 &&
            !projectiles(SABER_X1_CHARGED_RELEASE_CLASS),
        "full charge release produces the observed two class-3 shots");
  printf("reference: X3 charge thresholds frames %u/%u/%u/%u; release classes 1/3/3; full shots=%u\n",
         SABER_CHARGE_TIER_1_FRAME, SABER_CHARGE_TIER_2_FRAME,
         SABER_CHARGE_TIER_3_FRAME, SABER_CHARGE_FULL_FRAME,
         projectiles(SABER_FULL_RELEASE_CLASS));
  puts("ok: x3-zero-charge-tiers");
}

static void x3_hurt_checks(const char *fixture, unsigned button) {
  load_fixture(fixture);
  hold_charge_button(60, button);
  unsigned before = MmxZeroGetState().charge;
  /* This is the same native hurt-state injection used by the upstream Zero
   * ROM checks. It avoids depending on an enemy being in the first few
   * seconds of save0.sav. */
  g_ram[0xbaa] = 0x0e;
  g_ram[0xbab] = 0;
  unsigned hurt_duration = 0;
  unsigned lowest_charge = before;
  bool hurt_ended = false;
  for (unsigned i = 0; i < 120; ++i) {
    frame(button);
    unsigned charge = MmxZeroGetState().charge;
    if (g_ram[0xbaa] == 0x0e) {
      ++hurt_duration;
      if (charge < lowest_charge) lowest_charge = charge;
    } else {
      hurt_ended = true;
      break;
    }
  }
  unsigned after = MmxZeroGetState().charge;
  check(hurt_ended, "X3 Zero hurt state ends within 120 held-charge frames");
  check(hurt_duration >= 2, "X3 Zero hurt state is observed for at least two frames");
  check(lowest_charge >= before,
        "X3 Zero never drops below its pre-hurt charge while hurt");
  printf("reference: X3 hurt duration=%u charge %u -> %u\n",
         hurt_duration, before, after);
  puts("ok: x3-zero-charge-through-hurt");
}

static void x3_jump_checks(const char *fixture, unsigned button) {
  load_fixture(fixture);
  hold_charge_button(10, button);
  unsigned before = MmxZeroGetState().charge;
  bool was_grounded = (g_ram[0xbd3] & 4) || (g_ram[0xbd4] & 4);
  unsigned airborne_frames = 0;
  unsigned airborne_streak = 0;
  unsigned landing_frame = 0, charge_at_landing = 0;
  bool landed = false;
  bool charge_never_lowered = true;
  for (unsigned i = 1; i <= 20; ++i) {
    frame(SNES_PAD_B | button);
    bool grounded = (g_ram[0xbd3] & 4) || (g_ram[0xbd4] & 4);
    unsigned charge = MmxZeroGetState().charge;
    if (!grounded) {
      ++airborne_frames;
      ++airborne_streak;
    } else {
      airborne_streak = 0;
    }
    if (charge < before) charge_never_lowered = false;
    was_grounded = grounded;
  }
  for (unsigned i = 21; i <= 150; ++i) {
    frame(button);
    bool grounded = (g_ram[0xbd3] & 4) || (g_ram[0xbd4] & 4);
    unsigned charge = MmxZeroGetState().charge;
    if (!grounded) {
      ++airborne_frames;
      ++airborne_streak;
    } else if (!was_grounded) {
      landed = true;
      landing_frame = i;
      charge_at_landing = charge;
      break;
    } else {
      airborne_streak = 0;
    }
    if (charge < before) charge_never_lowered = false;
    was_grounded = grounded;
  }
  check(landed, "X3 Zero reaches a landing edge after the held jump");
  check(airborne_streak >= 10,
        "X3 Zero is airborne for at least ten consecutive frames before landing");
  check(charge_never_lowered && charge_at_landing >= before,
        "X3 Zero never lowers charge through jump and landing");
  unsigned previous_charge = charge_at_landing;
  bool charge_grew_after_landing = true;
  for (unsigned i = 0; i < 5; ++i) {
    frame(button);
    unsigned charge = MmxZeroGetState().charge;
    if (charge <= previous_charge) charge_grew_after_landing = false;
    previous_charge = charge;
  }
  check(charge_grew_after_landing,
        "X3 Zero keeps growing charge for five frames after landing");
  printf("reference: X3 held-jump airborne_frames=%u landing_frame=%u charge=%u\n",
         airborne_frames, landing_frame, charge_at_landing);
  puts("ok: x3-zero-charge-through-jump");
}

static void x3_post_charge_checks(const char *fixture, unsigned button) {
  load_fixture(fixture);
  release_charge_button(SABER_CHARGE_FULL_FRAME, button);
  idle(17);
  frame(button);
  idle(9);
  for (unsigned i = 0; i < 240 &&
       (MmxZeroGetState().burst || MmxZeroGetState().shot_mask || g_ram[0xc25]); ++i)
    frame(0);
  check(MmxZeroGetState().combo == 2, "full X3 buster sequence reaches its stored second-shot state");
  frame(button);
  idle(55);
  check(!MmxZeroGetState().combo && !MmxZeroGetState().slash,
        "full X3 buster sequence finishes before the plain-buster probe");
  unsigned char previous[8] = {0};
  frame(button);
  unsigned births = new_projectiles(0, previous);
  check(births == 1 && projectiles(0) == 1,
        "plain Zero buster still fires once after a full charged release");
  puts("ok: x3-zero-post-full-charge-plain");
}

static void native_x1_checks(const char *fixture) {
  load_fixture(fixture);
  switch_to_x();
  select_native_weapon(2); /* Fire Wave. */
  unsigned char previous[8] = {0};
  unsigned births = 0, live_frames = 0, peak_live = 0, live_samples = 0;
  for (unsigned i = 0; i < SABER_FIRE_WAVE_FRAMES; ++i) {
    frame(SNES_PAD_Y);
    unsigned live = projectiles(8); /* X1 Fire Wave's native flame class. */
    births += new_projectiles(8, previous);
    live_frames += live != 0;
    live_samples += live;
    if (live > peak_live) peak_live = live;
  }
  check(births == SABER_FIRE_WAVE_BIRTHS &&
            live_frames == SABER_FIRE_WAVE_LIVE_FRAMES &&
            live_samples == SABER_FIRE_WAVE_LIVE_SAMPLES &&
            peak_live == SABER_FIRE_WAVE_PEAK,
        "native X1 Fire Wave matches its 90-frame reference counts");
  printf("reference: Fire Wave held %u frames births=%u live_frames=%u live_samples=%u peak=%u\n",
         SABER_FIRE_WAVE_FRAMES, births, live_frames, live_samples, peak_live);

  load_fixture(fixture);
  switch_to_x();
  select_native_weapon(5); /* Storm Tornado: one normal shot per tap. */
  memset(previous, 0, sizeof(previous));
  births = 0;
  frame(SNES_PAD_Y);
  births += new_projectiles(11, previous);
  for (unsigned i = 1; i < SABER_ONE_SHOT_FRAMES; ++i) {
    frame(0);
    births += new_projectiles(11, previous);
  }
  check(births == SABER_ONE_SHOT_PROJECTILES,
        "native X1 one-shot weapon fires exactly once per tap");
  printf("reference: Storm Tornado one tap frames=%u projectiles=%u\n",
         SABER_ONE_SHOT_FRAMES, births);
  puts("ok: x1-native-weapons");
}

static FireWaveCounts measure_fire_wave(const char *fixture, unsigned button) {
  FireWaveCounts counts = {0};
  unsigned char previous[8] = {0};

  load_fixture(fixture);
  MmxSaberFrameReset();
  select_native_weapon(2); /* Fire Wave. */
  for (unsigned i = 0; i < SABER_FIRE_WAVE_FRAMES; ++i) {
    frame(button);
    unsigned live = projectiles(8);
    counts.births += new_projectiles(8, previous);
    counts.live_frames += live != 0;
    counts.live_samples += live;
    if (live > counts.peak) counts.peak = live;
  }
  return counts;
}

static unsigned measure_storm_tornado(const char *fixture, unsigned button) {
  unsigned char previous[8] = {0};
  unsigned births = 0;

  load_fixture(fixture);
  MmxSaberFrameReset();
  select_native_weapon(5); /* Storm Tornado: one normal shot per tap. */
  frame(button);
  births += new_projectiles(11, previous);
  for (unsigned i = 1; i < SABER_ONE_SHOT_FRAMES; ++i) {
    frame(0);
    births += new_projectiles(11, previous);
  }
  return births;
}

static SpecialCounts measure_specials(const char *fixture, unsigned button) {
  SpecialCounts counts;
  counts.fire_wave = measure_fire_wave(fixture, button);
  counts.storm_tornado_projectiles = measure_storm_tornado(fixture, button);
  return counts;
}

static void print_special_reference(const char *label,
                                    const SpecialCounts *counts) {
  printf("reference: %s Fire Wave held %u frames births=%u live_frames=%u "
         "live_samples=%u peak=%u\n",
         label, SABER_FIRE_WAVE_FRAMES, counts->fire_wave.births,
         counts->fire_wave.live_frames, counts->fire_wave.live_samples,
         counts->fire_wave.peak);
  printf("reference: %s Storm Tornado one tap frames=%u projectiles=%u\n",
         label, SABER_ONE_SHOT_FRAMES, counts->storm_tornado_projectiles);
}

static void check_fire_wave_equal(const char *what,
                                  const FireWaveCounts *reference,
                                  const FireWaveCounts *actual) {
  bool equal = reference->births == actual->births &&
               reference->live_frames == actual->live_frames &&
               reference->live_samples == actual->live_samples &&
               reference->peak == actual->peak;
  if (!equal) {
    fprintf(stderr,
            "FAIL: %s upstream=%u/%u/%u/%u actual=%u/%u/%u/%u\n",
            what, reference->births, reference->live_frames,
            reference->live_samples, reference->peak, actual->births,
            actual->live_frames, actual->live_samples, actual->peak);
  }
  check(equal, what);
}

static void check_specials_equal(const char *what,
                                 const SpecialCounts *reference,
                                 const SpecialCounts *actual) {
  bool equal = reference->fire_wave.births == actual->fire_wave.births &&
               reference->fire_wave.live_frames == actual->fire_wave.live_frames &&
               reference->fire_wave.live_samples == actual->fire_wave.live_samples &&
               reference->fire_wave.peak == actual->fire_wave.peak &&
               reference->storm_tornado_projectiles ==
                   actual->storm_tornado_projectiles;
  if (!equal) {
    fprintf(stderr,
            "FAIL: %s upstream Fire Wave=%u/%u/%u/%u Storm=%u; "
            "actual Fire Wave=%u/%u/%u/%u Storm=%u\n",
            what, reference->fire_wave.births,
            reference->fire_wave.live_frames,
            reference->fire_wave.live_samples, reference->fire_wave.peak,
            reference->storm_tornado_projectiles, actual->fire_wave.births,
            actual->fire_wave.live_frames, actual->fire_wave.live_samples,
            actual->fire_wave.peak, actual->storm_tornado_projectiles);
  }
  check(equal, what);
}

static SpecialCounts x3_zero_specials_checks(const char *fixture) {
  SpecialCounts counts = measure_specials(fixture, SNES_PAD_Y);
  print_special_reference("X3 Zero", &counts);
  check(counts.fire_wave.births == X3_ZERO_FIRE_WAVE_BIRTHS &&
            counts.fire_wave.live_frames == X3_ZERO_FIRE_WAVE_LIVE_FRAMES &&
            counts.fire_wave.live_samples == X3_ZERO_FIRE_WAVE_LIVE_SAMPLES &&
            counts.fire_wave.peak == X3_ZERO_FIRE_WAVE_PEAK,
        "upstream X3 Zero Fire Wave matches its 90-frame reference counts");
  check(counts.storm_tornado_projectiles == X3_ZERO_STORM_TORNADO_PROJECTILES,
        "upstream X3 Zero Storm Tornado fires once per tap");
  puts("ok: x3-zero-specials");
  return counts;
}

static void saber_y_checks(const char *fixture) {
  load_fixture(fixture);
  for (unsigned i = 0; i < 200; ++i) frame(SNES_PAD_Y);
  check(MmxZeroGetState().charge == 0 && all_projectiles() == 0,
        "physical Y hold never charges or fires Zero's buster");
  frame(0);
  for (unsigned tap = 0; tap < 3; ++tap) {
    frame(SNES_PAD_Y);
    frame(0);
    idle(20);
    check(MmxZeroGetState().charge == 0 && all_projectiles() == 0,
          "physical Y taps never fire a Zero shot");
  }
  puts("ok: saber-input-y-blocked");
}

static void saber_ground_1_checks(const char *fixture) {
  MmxSaberAttackSnapshot snapshot;
  MmxSaberAttackSnapshot previous;
  bool timing_ok = true;
  bool mask_ok = true;
  bool position_ok = true;
  bool charge_not_lost = true;
  bool shot_before_idle = false;
  bool previous_projectile = false;
  unsigned starts = 0;
  unsigned births = 0;
  unsigned charge_before_release;
  uint16_t standing_x;

  printf("reference: old Saber ground-1 timing startup=%u active=%u "
         "recovery=%u total=%u (oldsaber src/mmx_saber.c:266-269)\n",
         OLD_SABER_GROUND1_STARTUP, OLD_SABER_GROUND1_ACTIVE,
         OLD_SABER_GROUND1_RECOVERY, OLD_SABER_GROUND1_TOTAL);

  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL; ++i) {
    const MmxSaberPadPhase expected =
        i < OLD_SABER_GROUND1_STARTUP ? SABER_PHASE_STARTUP :
        i < OLD_SABER_GROUND1_STARTUP + OLD_SABER_GROUND1_ACTIVE ?
            SABER_PHASE_ACTIVE : SABER_PHASE_RECOVERY;
    frame(i == 0 ? SNES_PAD_Y : 0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind != SABER_KIND_GROUND1 || snapshot.index != 0 ||
        snapshot.anim_id != 1 || snapshot.tick != i ||
        snapshot.phase != expected)
      timing_ok = false;
  }
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(timing_ok && snapshot.phase == SABER_PHASE_IDLE &&
            snapshot.kind == SABER_KIND_NONE && snapshot.anim_id == 0,
        "Saber ground-1 publishes animation 1 through old startup/active/recovery timing");

  load_fixture(fixture);
  MmxSaberFrameReset();
  previous = MmxSaberAttackSnapshotGet();
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 12; ++i) {
    frame(SNES_PAD_Y);
    snapshot = MmxSaberAttackSnapshotGet();
    if (previous.phase == SABER_PHASE_IDLE &&
        snapshot.phase != SABER_PHASE_IDLE)
      ++starts;
    previous = snapshot;
  }
  check(starts == 1 && snapshot.phase == SABER_PHASE_IDLE,
        "holding Y starts exactly one ground-1 attack");

  load_fixture(fixture);
  MmxSaberFrameReset();
  g_ram[0x00ac] = 0x40;
  g_ram[0x00a7] = 0;
  g_ram[0x00a9] = 0;
  starts = 0;
  previous = MmxSaberAttackSnapshotGet();
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 2; ++i) {
    MmxZeroExtPrePlayer(g_ram);
    snapshot = MmxSaberAttackSnapshotGet();
    if (previous.phase == SABER_PHASE_IDLE &&
        snapshot.phase != SABER_PHASE_IDLE)
      ++starts;
    previous = snapshot;
  }
  check(starts == 1 && snapshot.phase == SABER_PHASE_IDLE,
        "the pre-player Y hold edge remains one-shot across the idle boundary");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y | SNES_PAD_RIGHT);
  standing_x = (uint16_t)read_ram_word(g_ram, 0x0bad);
  if (g_ram[0x0bdf] & MMX_SABER_NATIVE_HORIZONTAL_BITS)
    mask_ok = false;
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL - 1; ++i) {
    frame(SNES_PAD_RIGHT);
    if (g_ram[0x0bdf] & MMX_SABER_NATIVE_HORIZONTAL_BITS)
      mask_ok = false;
    if (read_ram_word(g_ram, 0x0bad) != standing_x)
      position_ok = false;
  }
  check(mask_ok && position_ok,
        "ground-1 feeds the live phase back to clear horizontal pad bits and halt Zero");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y | SNES_PAD_X);
  for (unsigned i = 0; i < 6; ++i) {
    frame(SNES_PAD_X);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.phase != SABER_PHASE_IDLE && all_projectiles())
      shot_before_idle = true;
  }
  charge_before_release = MmxZeroGetState().charge;
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  if (snapshot.phase == SABER_PHASE_IDLE || all_projectiles())
    shot_before_idle = true;
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 5; ++i) {
    bool live;
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    live = all_projectiles() != 0;
    if (snapshot.phase != SABER_PHASE_IDLE &&
        MmxZeroGetState().charge < charge_before_release)
      charge_not_lost = false;
    if (live && !previous_projectile)
      ++births;
    if (live && snapshot.phase != SABER_PHASE_IDLE)
      shot_before_idle = true;
    previous_projectile = live;
  }
  check(charge_before_release > 0 && charge_not_lost && births == 1 &&
            !shot_before_idle,
        "X held during ground-1 charges without a shot; release buffers exactly one post-idle shot");
  puts("ok: saber-ground-1");
}

static void saber_special_checks(const char *fixture,
                                 const SpecialCounts *upstream) {
  SpecialCounts saber = measure_specials(fixture, SNES_PAD_X);
  print_special_reference("Saber Zero", &saber);
  check_specials_equal("Saber Zero specials equal upstream X3 Zero", upstream,
                       &saber);

  {
    unsigned char previous[8] = {0};
    unsigned slash_births = 0;
    unsigned held_births = 0;
    unsigned fresh_births = 0;
    bool saw_slash = false;
    bool reached_idle = false;
    MmxSaberAttackSnapshot snapshot;

    load_fixture(fixture);
    MmxSaberFrameReset();
    select_native_weapon(2); /* Fire Wave. */
    shot_presence(8, previous);
    for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 2; ++i) {
      const unsigned input = i == 0 ? SNES_PAD_X | SNES_PAD_Y : SNES_PAD_X;
      frame(input);
      snapshot = MmxSaberAttackSnapshotGet();
      const unsigned births = new_projectiles(8, previous);
      if (snapshot.phase != SABER_PHASE_IDLE) {
        saw_slash = true;
        slash_births += births;
      } else {
        reached_idle = true;
        held_births += births;
        break;
      }
    }
    for (unsigned i = 0; reached_idle && i < 60; ++i) {
      frame(SNES_PAD_X);
      held_births += new_projectiles(8, previous);
    }
    printf("reference: OD4 Fire Wave saw_slash=%d reached_idle=%d slash_births=%u held_births=%u\n",
           saw_slash, reached_idle, slash_births, held_births);
    check(saw_slash && reached_idle && slash_births == 0,
          "OD4 blocks new Fire Wave flames throughout ground-1 while X is held");
    if (held_births) {
      printf("reference: Saber Fire Wave post-idle held-X semantics births=%u in 60 frames\n",
             held_births);
    } else {
      frame(0);
      new_projectiles(8, previous);
      frame(SNES_PAD_X);
      fresh_births = new_projectiles(8, previous);
      check(fresh_births > 0,
            "Fire Wave produces a flame after a fresh X press following ground-1");
      printf("reference: Saber Fire Wave requires a fresh X press after idle; births=%u\n",
             fresh_births);
    }
  }

  {
    unsigned char previous[8] = {0};
    unsigned slash_births = 0;
    unsigned after_idle_births = 0;
    bool saw_slash = false;
    bool reached_idle = false;
    MmxSaberAttackSnapshot snapshot;

    load_fixture(fixture);
    MmxSaberFrameReset();
    select_native_weapon(5); /* Storm Tornado. */
    shot_presence(11, previous);
    for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 2; ++i) {
      const unsigned input = i == 0 ? SNES_PAD_Y :
          i == 1 ? SNES_PAD_X : 0;
      frame(input);
      snapshot = MmxSaberAttackSnapshotGet();
      const unsigned births = new_projectiles(11, previous);
      if (snapshot.phase != SABER_PHASE_IDLE) {
        saw_slash = true;
        slash_births += births;
      } else {
        reached_idle = true;
        break;
      }
    }
    check(saw_slash && reached_idle && slash_births == 0,
          "OD4 blocks a Storm Tornado X press throughout ground-1");
    frame(SNES_PAD_X);
    after_idle_births += new_projectiles(11, previous);
    for (unsigned i = 1; i < SABER_ONE_SHOT_FRAMES; ++i) {
      frame(0);
      after_idle_births += new_projectiles(11, previous);
    }
    check(after_idle_births == 1,
          "Storm Tornado fires exactly one projectile from an X press after ground-1");
  }
  puts("ok: saber-input-specials-zero");
}

static void saber_pass_through_checks(const char *fixture) {
  load_fixture(fixture);
  switch_to_x();
  for (unsigned i = 0; i < 5; ++i) {
    frame(i == 0 ? SNES_PAD_X : 0);
    check(!MmxSaberFrameLastWroteInput(),
          "Saber frame hook writes nothing after exchange to X");
  }
  puts("ok: saber-input-x-pass-through");
}

static void saber_legacy_intent_bridge_check(const char *fixture) {
  load_fixture(fixture);
  /* Run the real Saber pre-player hook, then clear the mapped fire bytes
   * before the Zero legacy tick. This isolates the legacy_intent seam from
   * the native mapped view: a rejected override must not charge. */
  g_ram[0x00a7] = 0x40;
  g_ram[0x00a9] = 0;
  g_ram[0x00ab] = 0x40;
  g_ram[0x00ac] = 0;
  MmxZeroExtPrePlayer(g_ram);
  g_ram[0x0bdf] = 0;
  g_ram[0x0be3] = 0;
  MmxZeroPlayerTick(g_ram);
  check(MmxZeroGetState().charge == 1,
        "Saber legacy intent drives Zero charge after mapped fire is cleared");
  load_fixture(fixture);
}

static void saber_input_checks(const char *fixture,
                               const SpecialCounts *upstream) {
  /* Exercise the native X1 arm-upgrade branch while the Saber buster path
   * still owns the legacy X3 charge chain. */
  saber_legacy_intent_bridge_check(fixture);
  load_fixture(fixture);
  g_ram[0x1f99] |= 2;
  saber_track_x1_charged = true;
  saber_saw_x1_charged = false;
  release_charge_button(SABER_CHARGE_FULL_FRAME, SNES_PAD_X);
  idle(17);
  frame(SNES_PAD_X);
  idle(9);
  saber_track_x1_charged = false;
  check(!saber_saw_x1_charged && projectiles(SABER_FULL_RELEASE_CLASS) == 2,
        "Saber X buster upgrade still emits only the two X3 class-3 shots");

  saber_track_x1_charged = true;
  saber_saw_x1_charged = false;
  x3_plain_checks(fixture, SNES_PAD_X);
  x3_charge_checks(fixture, SNES_PAD_X);
  x3_hurt_checks(fixture, SNES_PAD_X);
  x3_jump_checks(fixture, SNES_PAD_X);
  x3_post_charge_checks(fixture, SNES_PAD_X);
  saber_track_x1_charged = false;
  check(!saber_saw_x1_charged,
        "Saber X3 buster paths never spawn X1 charged-shot class 2");
  saber_y_checks(fixture);
  saber_special_checks(fixture, upstream);
  saber_pass_through_checks(fixture);
  native_x1_checks(fixture);
  puts("ok: saber-input");
}

static const RecompLauncherCModProvider *g_mod_provider;

static int set_test_env(const char *name, const char *value) {
#ifdef _WIN32
  return _putenv_s(name, value);
#else
  return setenv(name, value, 1);
#endif
}

static void activate_zero(const char *x1_rom, const char *x3_rom,
                          const char *assets, bool saber_package,
                          bool expect_saber_assets) {
  const char *root = getenv("MMX_COOP_LAUNCHER_ROOT");
  const char *package = saber_package ? "megaman-x.character.saber-zero"
                                      : "megaman-x.character.zero";
  const char *feature = saber_package ? "saber-zero" : "zero";
  if (!g_mod_provider) {
    check(root && root[0], "Saber runner supplies an isolated mod catalog");
    check(readable_file(x1_rom), "X1 ROM exists");
    check(readable_file(x3_rom), "X3 ROM exists");
    check(readable_file(getenv("MMX_ZERO_TEST_FIXTURE")), "save0.sav exists");
    if (!snes_mod_runtime_initialize_c(root, "megaman-x-us", kMmxRomDigest)) {
      fprintf(stderr, "Saber catalog error: %s\n",
              snes_mod_runtime_last_error_c());
      check(0, "Saber catalog initializes");
    }
    g_mod_provider = snes_mod_runtime_launcher_provider_c();
    check(g_mod_provider && g_mod_provider->feature_enable &&
              g_mod_provider->feature_set_option &&
              g_mod_provider->feature_resource_set_path && g_mod_provider->commit,
          "Saber catalog exposes the feature/resource provider");
  }
  check(g_mod_provider->feature_enable(g_mod_provider->ctx, package, feature, 1),
        saber_package ? "Saber package enables" : "upstream Zero package enables");
  check(g_mod_provider->feature_set_option(g_mod_provider->ctx, package, feature,
                                           "start", "zero"),
        saber_package ? "Saber package starts as Zero" :
                        "upstream Zero package starts as Zero");
  if (!saber_package) {
    check(g_mod_provider->feature_set_option(g_mod_provider->ctx,
                                             "megaman-x.character.zero", "zero",
                                             "behavior", "x3"),
          "upstream Zero package selects behavior=x3");
  }
  check(g_mod_provider->feature_resource_set_path(g_mod_provider->ctx, package, feature,
                                                  "x3-rom", x3_rom),
        saber_package ? "Saber package selects the X3 ROM" :
                        "upstream Zero package selects the X3 ROM");
  check(g_mod_provider->commit(g_mod_provider->ctx, x1_rom),
        saber_package ? "Saber package commits for the X1 ROM" :
                        "upstream Zero package commits for the X1 ROM");
  snes_mod_runtime_activate_plugins_c();
  check(MmxZeroEnabled() && MmxZeroActive() && !MmxZeroModern(),
        saber_package ? "Saber package activates the legacy X3 controller" :
                        "upstream Zero package activates the legacy X3 controller");
  check(expect_saber_assets ? MmxSaberEnabled() : !MmxSaberEnabled(),
        expect_saber_assets ? "Saber package enables the Saber plugin" :
                              "missing Saber assets leave the Saber plugin disabled");
  if (saber_package && expect_saber_assets)
    check(MmxSaberAssetsLoaded() && MmxSaberRideAssetsLoaded() &&
              MmxSaberWaveLoaded(),
          "Saber loader reports private sprite and wave caches loaded");
  check(readable_file(assets), "isolated X3 Zero asset cache exists");
}

static void saber_assets_checks(const char *x1_rom, const char *x3_rom,
                                const char *fixture, const char *assets) {
  const char *empty_cache = getenv("MMX_SABER_EMPTY_CACHE");
  const char *cache = getenv("MMX_SABER_TEST_CACHE");
  char sfx_path[4096];
  static const MmxSaberSfxAttackCue kCues[] = {
    MMX_SABER_SFX_ATTACK_GROUND_SLASH_1,
    MMX_SABER_SFX_ATTACK_GROUND_SLASH_2,
    MMX_SABER_SFX_ATTACK_GROUND_SLASH_3,
    MMX_SABER_SFX_ATTACK_X3_FINISHER,
    MMX_SABER_SFX_ATTACK_AIR,
    MMX_SABER_SFX_ATTACK_WALL,
    MMX_SABER_SFX_ATTACK_DASH
  };
  static const unsigned kExpectedClips[] = {
    MMX_SABER_SFX_CLIP_SABER_1,
    MMX_SABER_SFX_CLIP_SABER_2,
    MMX_SABER_SFX_CLIP_SABER_3,
    MMX_SABER_SFX_CLIP_SABER_3,
    MMX_SABER_SFX_CLIP_SABER_1,
    MMX_SABER_SFX_CLIP_SABER_1,
    MMX_SABER_SFX_CLIP_SABER_2
  };
  check(empty_cache && empty_cache[0], "runner supplies an empty Saber cache");
  check(cache && cache[0] &&
            snprintf(sfx_path, sizeof(sfx_path), "%s/mmx-source/%s", cache,
                     "saber-sfx-v2.bin") < (int)sizeof(sfx_path),
        "runner supplies an isolated Saber SFX cache path");

  activate_zero(x1_rom, x3_rom, assets, true, true);
  check(MmxSaberAssetsLoaded() && MmxSaberRideAssetsLoaded() &&
            MmxSaberWaveLoaded() && MmxSaberEnabled(),
        "Saber asset activation enables sprite and wave caches");
  check(MmxSaberSfxLoaded(), "Saber SFX sidecar loads at activation");
  check(MmxSaberSfxVolume() == 50,
        "Saber SFX package option defaults to 50 percent");
  for (unsigned i = 0; i < sizeof(kCues) / sizeof(kCues[0]); ++i) {
    MmxSaberSfxPlayForAttack(kCues[i]);
    check(MmxSaberSfxLastClip() == kExpectedClips[i],
          "Saber attack cue maps to the expected clip");
  }
  check(MmxSaberSfxRegisteredClipCount() == MMX_SABER_SFX_CLIP_COUNT,
        "Saber SFX clips register lazily on first cue");

  check(remove(sfx_path) == 0, "temporary SFX cache can be removed");
  snes_mod_runtime_activate_plugins_c();
  check(MmxSaberEnabled() && MmxSaberAssetsLoaded() &&
            MmxSaberRideAssetsLoaded() && MmxSaberWaveLoaded(),
        "missing SFX cache does not disable Saber");
  check(!MmxSaberSfxLoaded(), "missing SFX cache leaves SFX unavailable only");

  check(set_test_env("MMX_SABER_TEST_CACHE", empty_cache) == 0,
        "Saber test redirects to the empty cache");
  check(set_test_env("MMX_SABER_TEST_CACHE_ONLY", "1") == 0,
        "Saber missing-cache run disables message boxes");
  snes_mod_runtime_activate_plugins_c();
  check(MmxZeroEnabled() && MmxZeroActive() && !MmxZeroModern(),
        "X3 Zero remains active when Saber caches are missing");
  check(!MmxSaberEnabled() && !MmxSaberAssetsLoaded() &&
            !MmxSaberRideAssetsLoaded() && !MmxSaberWaveLoaded(),
        "missing Saber caches leave Saber disabled");
  x3_plain_checks(fixture, SNES_PAD_Y);
  puts("ok: saber-assets");
}

int main(int argc, char **argv) {
  check(argc == 2, "X1 ROM supplied");
  const char *fixture = getenv("MMX_ZERO_TEST_FIXTURE");
  const char *x3_rom = getenv("MMX_COOP_X3_ROM");
  const char *assets = getenv("MMX_ZERO_TEST_ASSETS");
  const char *only = getenv("MMX_SABER_TEST_ONLY");
  check(fixture && fixture[0], "MMX_ZERO_TEST_FIXTURE supplied");
  check(x3_rom && x3_rom[0], "MMX_COOP_X3_ROM supplied");
  check(assets && assets[0], "MMX_ZERO_TEST_ASSETS supplied");

  SDL_SetMainReady();
  check(snesrecomp_sdl_init(SDL_INIT_EVENTS), "SDL initializes");
  g_audio_mutex = SDL_CreateMutex();
  check(g_audio_mutex != NULL, "audio mutex initializes");
  static const SnesDesktopHostGame game = {
    .display_name = "MMX Saber ROM reference",
    .build_version = "saber-rom-reference-1",
    .num_players = 1,
    .before_run_frame = MmxBeforeFrame,
    .native_widescreen = 0,
    .state_menu_hotkeys = 1,
    .prepare_frame = MmxPrepareFrame,
    .begin_sim_frame = MmxBeginFrame,
    .end_sim_frame = MmxEndFrame,
    .draw_frame = MmxDrawFrame,
  };
  g_game = &game;
  ConfigUseStateMenuDefaults();
  FILE *config = fopen("config.ini", "w");
  check(config != NULL, "temporary test config opens");
  fputs("[Graphics]\nDisplayAspect=8:7\n", config);
  check(fclose(config) == 0, "temporary test config closes");
  ParseConfigFile("config.ini");
  g_config.new_renderer = true;
  g_config.widescreen = false;

  FILE *rom_file = fopen(argv[1], "rb");
  check(rom_file != NULL, "X1 ROM opens");
  check(fseek(rom_file, 0, SEEK_END) == 0, "X1 ROM seeks");
  long rom_size = ftell(rom_file);
  check(rom_size > 0, "X1 ROM has content");
  check(fseek(rom_file, 0, SEEK_SET) == 0, "X1 ROM rewinds");
  uint8 *rom = malloc((size_t)rom_size);
  check(rom != NULL, "X1 ROM allocates");
  check(fread(rom, 1, (size_t)rom_size, rom_file) == (size_t)rom_size,
        "X1 ROM reads");
  check(fclose(rom_file) == 0, "X1 ROM closes");
  if ((rom_size & 0x7fff) == 512) {
    rom_size -= 512;
    memmove(rom, rom + 512, (size_t)rom_size);
  }
  g_mmx_custom_renderer = true;
  MmxRendererSetRom(rom, (size_t)rom_size);
  g_last_drawable_width = 1280;
  g_last_drawable_height = 720;
  g_ppu_render_flags = kPpuRenderFlags_NewRenderer;
  RtlRegisterGame(&kMmxGameInfo);
  check(SnesInit(rom, (size_t)rom_size) != NULL, "X1 game initializes");
  g_spc_player = SmwSpcPlayer_Create();
  check(g_spc_player != NULL, "SPC player initializes");
  g_spc_player->initialize(g_spc_player);

  const bool saber_package = only && !strcmp(only, "saber-package");
  const bool zero_extension = only && !strcmp(only, "zero-extension");
  const bool saber_input = only && !strcmp(only, "saber-input");
  const bool saber_ground_1 = only && !strcmp(only, "saber-ground-1");
  const bool x3_zero_specials = only && !strcmp(only, "x3-zero-specials");
  const bool saber_enabled_group = saber_package || zero_extension ||
      saber_input || saber_ground_1;
  SpecialCounts upstream_specials = {0};
  if (saber_input) {
    activate_zero(argv[1], x3_rom, assets, false, false);
    upstream_specials = x3_zero_specials_checks(fixture);
    activate_zero(argv[1], x3_rom, assets, true, true);
  } else {
    activate_zero(argv[1], x3_rom, assets, saber_enabled_group,
                  saber_enabled_group);
  }
  if (saber_input) {
    check(MmxSaberEnabled(), "saber-input runs with the Saber package enabled");
    saber_input_checks(fixture, &upstream_specials);
  } else if (saber_ground_1) {
    check(MmxSaberEnabled(), "saber-ground-1 runs with the Saber package enabled");
    saber_ground_1_checks(fixture);
  } else if (x3_zero_specials) {
    upstream_specials = x3_zero_specials_checks(fixture);
  } else if (zero_extension) {
    check(MmxSaberEnabled(), "zero-extension runs with the Saber package enabled");
    zero_extension_checks(fixture);
    x3_plain_checks(fixture, SNES_PAD_Y);
    x3_charge_checks(fixture, SNES_PAD_Y);
    x3_hurt_checks(fixture, SNES_PAD_Y);
    x3_jump_checks(fixture, SNES_PAD_Y);
    x3_post_charge_checks(fixture, SNES_PAD_Y);
    puts("ok: zero-extension");
  } else if (saber_package) {
    check(MmxSaberEnabled(), "saber-package reports the Saber plugin enabled");
    x3_plain_checks(fixture, SNES_PAD_X);
    x3_charge_checks(fixture, SNES_PAD_X);
    puts("ok: saber-package");
  } else {
    if (!only || !strcmp(only, "x3-plain")) x3_plain_checks(fixture, SNES_PAD_Y);
    if (!only || !strcmp(only, "x3-charge")) x3_charge_checks(fixture, SNES_PAD_Y);
    if (!only || !strcmp(only, "x3-hurt")) x3_hurt_checks(fixture, SNES_PAD_Y);
    if (!only || !strcmp(only, "x3-jump")) x3_jump_checks(fixture, SNES_PAD_Y);
    if (!only || !strcmp(only, "x3-post-charge")) x3_post_charge_checks(fixture, SNES_PAD_Y);
    if (!only || !strcmp(only, "x1-native")) native_x1_checks(fixture);
    if (!only) {
      upstream_specials = x3_zero_specials_checks(fixture);
      activate_zero(argv[1], x3_rom, assets, true, true);
      check(MmxSaberEnabled(), "default run enables the Saber package group");
      zero_extension_checks(fixture);
      MmxZeroSetExtension(MmxSaberFrameExtension());
      x3_plain_checks(fixture, SNES_PAD_X);
      x3_charge_checks(fixture, SNES_PAD_X);
      saber_special_checks(fixture, &upstream_specials);
      saber_ground_1_checks(fixture);
      puts("ok: saber-package");
    }
    if (only && !strcmp(only, "saber-assets")) {
      saber_assets_checks(argv[1], x3_rom, fixture, assets);
    } else if (only && strcmp(only, "x3-plain") && strcmp(only, "x3-charge") &&
        strcmp(only, "x3-hurt") && strcmp(only, "x3-jump") &&
        strcmp(only, "x3-post-charge") && strcmp(only, "x1-native") &&
        strcmp(only, "x3-zero-specials") &&
        strcmp(only, "saber-package") && strcmp(only, "saber-input") &&
        strcmp(only, "saber-ground-1") &&
        strcmp(only, "zero-extension") &&
        strcmp(only, "saber-assets")) {
      fprintf(stderr, "FAIL: unknown MMX_SABER_TEST_ONLY group: %s\n", only);
      return 1;
    }
  }
  puts("SABER ROM CHECKS PASSED");
  return 0;
}

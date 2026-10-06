/* Saber-owned reference checks for the unmodified X3 Zero and native X1
 * weapon paths. The runner supplies a copied catalog, an isolated cache, the
 * original ROMs, and the standing Highway save0 fixture. */
#define MMX_DESKTOP_ENTRY MmxDesktopMain
#include "desktop/host_main.c"
#include MMX_GAME_MAIN
#include <stdint.h>
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
   * src/mmx_saber.c:266-273. Keep these literals separate from the new table
   * so a timing-table mutation cannot make the test pass. */
  OLD_SABER_GROUND1_STARTUP = 4,
  OLD_SABER_GROUND1_ACTIVE = 8,
  OLD_SABER_GROUND1_RECOVERY = 18,
  OLD_SABER_GROUND1_TOTAL = 30,
  /* Combo windows and phase lengths copied from the old table, not from
   * src/saber/mmx_saber_attack.c: oldsaber src/mmx_saber.c:260-319. */
  OLD_SABER_GROUND1_CHAIN_OPEN = 12,
  OLD_SABER_GROUND1_CHAIN_CLOSE = 29,
  OLD_SABER_GROUND1_BUFFER_OPEN = 4,
  OLD_SABER_GROUND1_BUFFER_CLOSE = 11,
  OLD_SABER_GROUND2_STARTUP = 0,
  OLD_SABER_GROUND2_ACTIVE = 12,
  OLD_SABER_GROUND2_RECOVERY = 18,
  OLD_SABER_GROUND2_TOTAL = 30,
  OLD_SABER_GROUND2_CHAIN_OPEN = 12,
  OLD_SABER_GROUND2_CHAIN_CLOSE = 29,
  OLD_SABER_GROUND2_BUFFER_OPEN = 0,
  OLD_SABER_GROUND2_BUFFER_CLOSE = 11,
  OLD_SABER_GROUND3_STARTUP = 0,
  OLD_SABER_GROUND3_ACTIVE = 14,
  OLD_SABER_GROUND3_RECOVERY = 25,
  OLD_SABER_GROUND3_TOTAL = 39,
  /* Air record copied from oldsaber/saber-zero-variant:
   * src/mmx_saber.c:322-339. */
  OLD_SABER_AIR_STARTUP = 4,
  OLD_SABER_AIR_ACTIVE = 8,
  OLD_SABER_AIR_RECOVERY = 6,
  OLD_SABER_AIR_TOTAL = 18,
  /* Wall record copied from oldsaber/saber-zero-variant:
   * src/mmx_saber.c:307-324. */
  OLD_SABER_WALL_STARTUP = 0,
  OLD_SABER_WALL_ACTIVE = 12,
  OLD_SABER_WALL_RECOVERY = 8,
  OLD_SABER_WALL_TOTAL = 20,
  /* Dash record copied from oldsaber/saber-zero-variant:
   * src/mmx_saber.c:362-379. */
  OLD_SABER_DASH_STARTUP = 2,
  OLD_SABER_DASH_ACTIVE = 10,
  OLD_SABER_DASH_RECOVERY = 18,
  OLD_SABER_DASH_TOTAL = 30,
  OLD_SABER_LAND_TOTAL = 18,
};

static const char *const kMmxRomDigest =
    "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32";

static unsigned projectiles(unsigned kind);
static bool saber_test_grounded(void);
static bool saber_track_x1_charged;
static bool saber_saw_x1_charged;
static unsigned zero_state_reset_calls;

static void zero_state_reset_probe(uint8_t *ram) {
  (void)ram;
  zero_state_reset_calls++;
}

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

static bool saber_tagged_projectile(unsigned d) {
  return g_ram[d] &&
      ((((unsigned)g_ram[d + 0x3e] | (unsigned)g_ram[d + 0x3f] << 8) &
          MMX_SABER_PROJECTILE_TAG_FAMILY_MASK) ==
              MMX_SABER_PROJECTILE_TAG_FAMILY);
}

static unsigned native_projectiles(void) {
  unsigned count = 0;
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    count += g_ram[d] && !saber_tagged_projectile(d);
  return count;
}

static unsigned tagged_projectiles(void) {
  unsigned count = 0;
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    count += saber_tagged_projectile(d);
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

static unsigned read_ram_word(const uint8_t *ram, unsigned offset);
static unsigned empty_enemy_slot(void);
static unsigned saber_active_slot(void);
static bool saber_lifecycle_idle(void);

static const uint8_t kOldSaberWallBounds[12] = {
    31, 246, 33, 19, 22, 0, 23, 15, 14, 1, 16, 12};

typedef struct SaberWallRoute {
  const char *side;
  unsigned walk_input;
  unsigned jump_input;
  unsigned travel_input;
  unsigned walk_frames;
  unsigned expected_wall_frame;
  unsigned expected_x;
  unsigned expected_y;
  uint8_t expected_native_facing;
  int open_sign;
} SaberWallRoute;

static const MmxSaberBoundsSegment *saber_wall_segment(uint8_t tick) {
  const MmxSaberAttack *wall = MmxSaberAttackRecord(SABER_KIND_WALL, 0);
  if (!wall) return NULL;
  for (unsigned i = 0; i < wall->bounds_segment_count; ++i) {
    const MmxSaberBoundsSegment *segment = wall->bounds_segments + i;
    if (tick >= segment->first_tick && tick <= segment->last_tick)
      return segment;
  }
  return NULL;
}

static int saber_wall_effective_x(const MmxSaberBoundsSegment *segment,
                                  uint8_t slot_facing) {
  if (!segment) return 0;
  return slot_facing & 0x40 ? -(int)segment->bounds_x :
      (int)segment->bounds_x;
}

static unsigned saber_wall_setup(const char *path, const SaberWallRoute *route,
                                 bool print_reference) {
  check(RtlLoadSnapshot(path), "wall fixture route loads");
  check(MmxZeroActive() && !MmxZeroModern(),
        "wall fixture route is legacy X3 Zero");
  MmxZeroCancel(g_ram);
  MmxSaberFrameReset();
  for (unsigned i = 0; i < route->walk_frames; ++i)
    frame(route->walk_input);
  /* The release frame makes the following B edge physical after the setup
   * walk; the held travel direction remains native wall-slide input. */
  frame(0);
  for (unsigned i = 0; i < 240; ++i) {
    frame(i < 20 ? route->jump_input : route->travel_input);
    if (g_ram[0x0baa] == 0x12) {
      if (print_reference)
        printf("reference: wall fixture=armadillo-fight.sav side=%s "
               "script=hold %s for %u frames; 0; hold B+direction for "
               "20 frames; hold direction first_wall_frame=%u Zero=(%u,%u) "
               "native_facing=0x%02X\n",
               route->side,
               route->walk_input == SNES_PAD_LEFT ? "LEFT" : "RIGHT",
               route->walk_frames, i, read_ram_word(g_ram, 0x0bad),
               read_ram_word(g_ram, 0x0bb0), g_ram[0x0c11] & 0x40);
      for (unsigned settle = 0; settle < 6; ++settle)
        frame(route->travel_input);
      if (print_reference)
        printf("reference: wall settled side=%s settle_frames=6 Zero=(%u,%u) "
               "native_facing=0x%02X action=0x%02X\n", route->side,
               read_ram_word(g_ram, 0x0bad), read_ram_word(g_ram, 0x0bb0),
               g_ram[0x0c11] & 0x40, g_ram[0x0baa]);
      return i;
    }
  }
  return ~0u;
}

static unsigned saber_wall_charge_setup(const char *path,
                                        const SaberWallRoute *route) {
  check(RtlLoadSnapshot(path), "charged wall fixture route loads");
  check(MmxZeroActive() && !MmxZeroModern(),
        "charged wall fixture route is legacy X3 Zero");
  MmxZeroCancel(g_ram);
  MmxSaberFrameReset();
  for (unsigned i = 0; i < route->walk_frames; ++i)
    frame(route->walk_input | SNES_PAD_X);
  frame(SNES_PAD_X);
  for (unsigned i = 0; i < 240; ++i) {
    frame(route->jump_input | SNES_PAD_X);
    if (g_ram[0x0baa] == 0x12) {
      for (unsigned settle = 0; settle < 6; ++settle)
        frame(route->travel_input | SNES_PAD_X);
      return i;
    }
  }
  return ~0u;
}

static MmxSaberPadPhase saber_wall_phase(unsigned tick) {
  return tick < OLD_SABER_WALL_STARTUP ? SABER_PHASE_STARTUP :
      tick < OLD_SABER_WALL_STARTUP + OLD_SABER_WALL_ACTIVE ?
          SABER_PHASE_ACTIVE : SABER_PHASE_RECOVERY;
}

static void saber_wall_checks(const char *fixture_dir) {
  static const SaberWallRoute routes[] = {
    {"OPEN-RIGHT", SNES_PAD_LEFT, SNES_PAD_B | SNES_PAD_LEFT,
     SNES_PAD_LEFT, 60, 20, 5142, 2665, 0x40, 1},
    {"OPEN-LEFT", SNES_PAD_RIGHT, SNES_PAD_B | SNES_PAD_RIGHT,
     SNES_PAD_RIGHT, 180, 20, 5353, 2666, 0x00, -1},
  };
  const char *const fixture_name = "armadillo-fight.sav";
  const MmxSaberAttack *wall = MmxSaberAttackRecord(SABER_KIND_WALL, 0);
  char path[4096];
  int written = snprintf(path, sizeof(path), "%s/%s", fixture_dir,
                         fixture_name);
  check(written >= 0 && written < (int)sizeof(path),
        "wall fixture path fits");
  check(wall && wall->visual_animation == 5 && wall->facing_xor == 1 &&
            wall->startup_ticks == OLD_SABER_WALL_STARTUP &&
            wall->active_ticks == OLD_SABER_WALL_ACTIVE &&
            wall->recovery_ticks == OLD_SABER_WALL_RECOVERY &&
            wall->total_ticks == OLD_SABER_WALL_TOTAL && wall->damage == 3 &&
            wall->bounds_pointer == MMX_SABER_WALL_BOUNDS_POINTER,
        "wall record keeps animation 5, old timing, $FF50, and damage 3");
  printf("reference: old wall timing startup=%u active=%u recovery=%u "
         "total=%u record=oldsaber/src/mmx_saber.c:307-324 "
         "context=oldsaber/src/mmx_saber.c:974-978\n",
         OLD_SABER_WALL_STARTUP, OLD_SABER_WALL_ACTIVE,
         OLD_SABER_WALL_RECOVERY, OLD_SABER_WALL_TOTAL);

  for (unsigned route_number = 0;
       route_number < sizeof(routes) / sizeof(routes[0]); ++route_number) {
    const SaberWallRoute *route = routes + route_number;
    unsigned baseline_y[OLD_SABER_WALL_TOTAL];
    unsigned baseline_x[OLD_SABER_WALL_TOTAL];
    unsigned baseline_vy[OLD_SABER_WALL_TOTAL];
    bool baseline_wall = true;
    bool timing_ok = true;
    bool facing_ok = true;
    bool anchor_ok = true;
    bool damage_ok = false;
    bool slide_ok = true;
    unsigned active_frames = 0;
    unsigned slot = 0;
    unsigned wall_frame = saber_wall_setup(path, route, true);
    check(wall_frame == route->expected_wall_frame,
          "wall setup reaches its recorded native contact frame");
    check(read_ram_word(g_ram, 0x0bad) == route->expected_x &&
              read_ram_word(g_ram, 0x0bb0) == route->expected_y &&
              (g_ram[0x0c11] & 0x40) == route->expected_native_facing &&
              g_ram[0x0baa] == 0x12,
          "wall setup position and settled native facing match reference");
    check(!memcmp(g_snes->cart->rom + 0x37f50, kOldSaberWallBounds,
                  sizeof(kOldSaberWallBounds)),
          "wall setup has the old $FF50 records in the tagged $37F40 window");

    /* Native-only control trace: the same contact and held direction, with no
     * Y edge, is the movement oracle for the slash trace below. */
    for (unsigned i = 0; i < OLD_SABER_WALL_TOTAL; ++i) {
      frame(route->travel_input);
      baseline_x[i] = read_ram_word(g_ram, 0x0bad);
      baseline_y[i] = read_ram_word(g_ram, 0x0bb0);
      baseline_vy[i] = read_ram_word(g_ram, 0x0bc4);
      if (g_ram[0x0baa] != 0x12) baseline_wall = false;
    }

    wall_frame = saber_wall_setup(path, route, false);
    check(wall_frame == route->expected_wall_frame,
          "wall slash reload reaches the same native contact frame");
    for (unsigned i = 0; i < OLD_SABER_WALL_TOTAL; ++i) {
      const unsigned input = route->travel_input |
          (i == 0 ? SNES_PAD_Y : 0);
      frame(input);
      MmxSaberAttackSnapshot snapshot = MmxSaberAttackSnapshotGet();
      const MmxSaberPadPhase expected_phase = saber_wall_phase(i);
      if (snapshot.kind != SABER_KIND_WALL || snapshot.index != 0 ||
          snapshot.anim_id != 5 || snapshot.tick != i ||
          snapshot.phase != expected_phase)
        timing_ok = false;
      if (baseline_x[i] != read_ram_word(g_ram, 0x0bad) ||
          baseline_y[i] != read_ram_word(g_ram, 0x0bb0) ||
          baseline_vy[i] != read_ram_word(g_ram, 0x0bc4))
        slide_ok = false;
      if (g_ram[0x0baa] != 0x12)
        slide_ok = false;

      if (snapshot.phase == SABER_PHASE_ACTIVE) {
        const MmxSaberBoundsSegment *segment =
            saber_wall_segment(snapshot.tick);
        slot = saber_active_slot();
        const unsigned slot_facing = slot ? g_ram[slot + 0x11] & 0x40 : 0xff;
        const unsigned expected_slot_facing =
            ((snapshot.facing != 0) ^ (wall->facing_xor != 0)) ? 0x40 : 0;
        const unsigned effective_x = saber_wall_effective_x(segment,
                                                              (uint8_t)slot_facing);
        const int slot_delta_x = slot ?
            (int)read_ram_word(g_ram, slot + 5) -
                (int)read_ram_word(g_ram, 0x0bad) : 0;
        ++active_frames;
        if (snapshot.facing != (g_ram[0x0c11] & 0x40) ||
            slot == 0 || tagged_projectiles() != 1 ||
            slot_facing != expected_slot_facing ||
            ((slot_delta_x + (int)effective_x) * route->open_sign) <= 0) {
          facing_ok = false;
        }
        if (!segment || slot == 0 || slot_delta_x < -1 || slot_delta_x > 1 ||
            read_ram_word(g_ram, slot + 8) != read_ram_word(g_ram, 0x0bb0) ||
            read_ram_word(g_ram, slot + 0x20) !=
                wall->bounds_pointer + (unsigned)(segment - wall->bounds_segments) * 4) {
          anchor_ok = false;
        }
        if (slot && !damage_ok) {
          const unsigned enemy = empty_enemy_slot();
          const unsigned first_damage =
              MmxSaberAttackDamage(g_ram, enemy, slot, 1);
          const unsigned second_damage =
              MmxSaberAttackDamage(g_ram, enemy, slot, 1);
          damage_ok = first_damage == 3 && second_damage == 0 &&
              (MmxSaberAttackHitSlots() &
               (uint16_t)(1u << ((enemy - 0xe68) / 64)));
        }
      } else if (tagged_projectiles() != 0) {
        anchor_ok = false;
      }
    }
    check(timing_ok && active_frames == OLD_SABER_WALL_ACTIVE,
          "wall Y starts animation 5 with old startup/active/recovery timing");
    check(facing_ok,
          "wall ACTIVE slot facing follows settled $0C11 and points to the open side");
    check(anchor_ok && damage_ok,
          "wall ACTIVE uses old bounds and deals 3 once per swing");
    check(baseline_wall && slide_ok,
          "wall slash leaves native wall-slide Y motion unchanged");
    check(MmxSaberAttackCueCount() == 1 &&
              MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_1,
          "wall slash emits exactly one saber_1 cue");
    frame(route->travel_input);
    check(saber_lifecycle_idle(),
          "wall slash releases its tagged slot at natural end");

    /* The old wall context is also the post-native wall-loss boundary.  A
     * native wall jump therefore retires the wall owner instead of becoming a
     * new Saber air owner. */
    wall_frame = saber_wall_setup(path, route, false);
    check(wall_frame == route->expected_wall_frame,
          "wall leave probe reaches the same native contact frame");
    frame(route->travel_input | SNES_PAD_Y);
    for (unsigned i = 0; i < 4; ++i) frame(route->travel_input);
    frame(route->travel_input | SNES_PAD_B);
    check(g_ram[0x0baa] != 0x12 && saber_lifecycle_idle(),
          "wall jump leaves the wall through the old central Saber exit");
  }
  puts("ok: saber-wall");
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

enum {
  FIXTURE_REPLAY_FRAMES = 120,
  FIXTURE_WRAM_HASH_BYTES = 0x2000,
};

typedef struct {
  const char *name;
  unsigned stage;
  unsigned scene;
  unsigned x;
  unsigned y;
  unsigned hp;
  unsigned expected_stage;
  const char *expected_stage_name;
} FixtureReference;

/* These are identity oracles captured from the converted private fixtures in
 * the upstream Zero path. Keep each fixture's values named at the definition
 * site so a fixture replacement cannot silently change the reference set. */
static const FixtureReference kReferenceArmadilloFight = {
  "armadillo-fight.sav", 0x03, 0x04, 0x141B, 0x0A9F, 18, 0x03,
  "Armored Armadillo",
};
static const FixtureReference kReferenceArmadilloRoom = {
  "armadillo-room.sav", 0x03, 0x04, 0x1380, 0x0A6F, 18, 0x03,
  "Armored Armadillo",
};
static const FixtureReference kReferenceLogPlatform = {
  "log-platform.sav", 0x08, 0x04, 0x01E0, 0x0450, 8, 0x00, "Highway",
};
static const FixtureReference kReferenceMammothFight = {
  "mammoth-fight.sav", 0x04, 0x04, 0x1ED5, 0x02AF, 11, 0x04,
  "Flame Mammoth",
};
static const FixtureReference kReferenceMammothRoom = {
  "mammoth-room.sav", 0x04, 0x04, 0x1E2E, 0x02AF, 11, 0x04,
  "Flame Mammoth",
};
static const FixtureReference kReferenceMammothStun = {
  "mammoth-stun.sav", 0x04, 0x04, 0x1F3A, 0x02A0, 4, 0x04,
  "Flame Mammoth",
};
static const FixtureReference kReferencePenguinFight = {
  "penguin-fight.sav", 0x08, 0x04, 0x1E1B, 0x01AF, 16, 0x08,
  "Chill Penguin",
};
static const FixtureReference kReferencePenguinRoom = {
  "penguin-room.sav", 0x08, 0x04, 0x1D61, 0x018F, 16, 0x08,
  "Chill Penguin",
};
static const FixtureReference kReferenceRideArmor = {
  "ride-armor.sav", 0x08, 0x04, 0x1212, 0x038E, 14, 0x00, "Highway",
};

static const FixtureReference *const kFixtureReferences[] = {
  &kReferenceArmadilloFight,
  &kReferenceArmadilloRoom,
  &kReferenceLogPlatform,
  &kReferenceMammothFight,
  &kReferenceMammothRoom,
  &kReferenceMammothStun,
  &kReferencePenguinFight,
  &kReferencePenguinRoom,
  &kReferenceRideArmor,
};

static uint64_t fixture_wram_hash(void) {
  uint64_t hash = UINT64_C(1469598103934665603);
  for (unsigned i = 0; i < FIXTURE_WRAM_HASH_BYTES; ++i) {
    hash ^= g_ram[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static void fixture_replay(const char *path, uint64_t hashes[FIXTURE_REPLAY_FRAMES],
                           const char *label) {
  char message[256];
  int written = snprintf(message, sizeof(message), "%s fresh load succeeds", label);
  check(written >= 0 && written < (int)sizeof(message),
        "fixture fresh-load message fits");
  check(RtlLoadSnapshot(path), message);
  for (unsigned frame_number = 0; frame_number < FIXTURE_REPLAY_FRAMES;
       ++frame_number) {
    frame(0);
    hashes[frame_number] = fixture_wram_hash();
  }
}

static void fixture_checks(const char *fixture_dir) {
  char path[4096];
  char message[256];
  for (unsigned i = 0; i < sizeof(kFixtureReferences) / sizeof(kFixtureReferences[0]); ++i) {
    const FixtureReference *reference = kFixtureReferences[i];
    int written = snprintf(path, sizeof(path), "%s/%s", fixture_dir, reference->name);
    check(written >= 0 && written < (int)sizeof(path),
          "fixture path fits the test buffer");
    written = snprintf(message, sizeof(message), "%s exists", reference->name);
    check(written >= 0 && written < (int)sizeof(message),
          "fixture existence message fits");
    check(readable_file(path), message);
    written = snprintf(message, sizeof(message), "%s loads", reference->name);
    check(written >= 0 && written < (int)sizeof(message),
          "fixture load message fits");
    check(RtlLoadSnapshot(path), message);
    check(!MmxSaberEnabled() && MmxZeroActive() && !MmxZeroModern(),
          "fixture identity uses upstream Zero with Saber disabled");

    const unsigned stage = g_ram[0x1f7a];
    const unsigned scene = g_ram[0x00d3];
    const unsigned x = read_ram_word(g_ram, 0x0bad);
    const unsigned y = read_ram_word(g_ram, 0x0bb0);
    const unsigned hp = g_ram[0x0bcf] & 0x7f;
    printf("reference: %s stage=0x%02X scene=0x%02X x=0x%04X y=0x%04X hp=%u\n",
           reference->name, reference->stage, reference->scene, reference->x,
           reference->y, reference->hp);
    check(stage == reference->stage && scene == reference->scene &&
              x == reference->x && y == reference->y && hp == reference->hp,
          snprintf(message, sizeof(message), "%s identity matches reference",
                   reference->name) < (int)sizeof(message) ? message :
              "fixture identity message fits");
    if (stage == reference->expected_stage) {
      printf("mapping: %s -> stage 0x%02X (%s)\n", reference->name, stage,
             reference->expected_stage_name);
    } else {
      printf("mapping: %s -> stage 0x%02X (name suggests %s stage 0x%02X; "
             "mismatch reported, fixture not changed)\n", reference->name, stage,
             reference->expected_stage_name, reference->expected_stage);
    }

    uint64_t first[FIXTURE_REPLAY_FRAMES];
    uint64_t second[FIXTURE_REPLAY_FRAMES];
    fixture_replay(path, first, reference->name);
    fixture_replay(path, second, reference->name);
    unsigned mismatch = FIXTURE_REPLAY_FRAMES;
    for (unsigned frame_number = 0; frame_number < FIXTURE_REPLAY_FRAMES;
         ++frame_number) {
      if (first[frame_number] != second[frame_number]) {
        mismatch = frame_number;
        break;
      }
    }
    check(mismatch == FIXTURE_REPLAY_FRAMES,
          snprintf(message, sizeof(message), "%s has identical per-frame hashes "
                   "for %u neutral frames (WRAM $0000-$1FFF)", reference->name,
                   FIXTURE_REPLAY_FRAMES) < (int)sizeof(message) ? message :
              "fixture replay message fits");
    if (mismatch != FIXTURE_REPLAY_FRAMES)
      printf("replay: %s first mismatch at frame %u\n", reference->name, mismatch);
  }
  puts("ok: fixture identity and replay checks");
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
    .collision_rom = NULL,
  };
  static const MmxZeroExtension intent_hooks = {
    .legacy_intent = zero_extension_intent_override,
    .collision_rom = NULL,
  };
  static const MmxZeroExtension reject_hooks = {
    .legacy_intent = zero_extension_intent_reject,
    .collision_rom = NULL,
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
  check(MmxZeroGetState().charge == 0 && native_projectiles() == 0,
        "physical Y hold never charges or fires Zero's buster");
  frame(0);
  for (unsigned tap = 0; tap < 3; ++tap) {
    frame(SNES_PAD_Y);
    frame(0);
    idle(20);
    check(MmxZeroGetState().charge == 0 && native_projectiles() == 0,
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
    if (snapshot.phase != SABER_PHASE_IDLE && native_projectiles())
      shot_before_idle = true;
  }
  charge_before_release = MmxZeroGetState().charge;
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  if (snapshot.phase == SABER_PHASE_IDLE || native_projectiles())
    shot_before_idle = true;
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 5; ++i) {
    bool live;
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    live = native_projectiles() != 0;
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

static MmxSaberPadPhase old_ground_phase(unsigned startup, unsigned active,
                                         unsigned tick) {
  return tick < startup ? SABER_PHASE_STARTUP :
      tick < startup + active ? SABER_PHASE_ACTIVE : SABER_PHASE_RECOVERY;
}

static bool ground_snapshot_matches(const MmxSaberAttackSnapshot *snapshot,
                                    unsigned index, unsigned animation,
                                    unsigned startup, unsigned active,
                                    unsigned tick) {
  const MmxSaberPadPhase phase = old_ground_phase(startup, active, tick);
  return snapshot->kind == (MmxSaberPadKind)(SABER_KIND_GROUND1 + index) &&
      snapshot->index == index && snapshot->anim_id == animation &&
      snapshot->tick == tick && snapshot->phase == phase;
}

static void saber_ground_combo_checks(const char *fixture) {
  MmxSaberAttackSnapshot snapshot;
  bool phase_ok = true;
  bool buffer_ok = true;
  bool held_ok = true;
  bool boundary_ok = true;
  bool facing_ok = true;
  bool position_ok = true;
  bool velocity_ok = true;
  bool saw_ground3 = false;
  uint16_t running_x;
  uint16_t stopped_x;

  printf("reference: old Saber combo windows g1 chain=%u..%u buffer=%u..%u; "
         "g2 chain=%u..%u buffer=%u..%u "
         "(oldsaber src/mmx_saber.c:260-319)\n",
         OLD_SABER_GROUND1_CHAIN_OPEN, OLD_SABER_GROUND1_CHAIN_CLOSE,
         OLD_SABER_GROUND1_BUFFER_OPEN, OLD_SABER_GROUND1_BUFFER_CLOSE,
         OLD_SABER_GROUND2_CHAIN_OPEN, OLD_SABER_GROUND2_CHAIN_CLOSE,
         OLD_SABER_GROUND2_BUFFER_OPEN, OLD_SABER_GROUND2_BUFFER_CLOSE);

  /* Separate Y edges at the inclusive chain-window endpoint publish all
   * three records. Validate the phase oracle independently for every visible
   * tick, including the recovery portions before each accepted edge. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  phase_ok = ground_snapshot_matches(&snapshot, 0, 1,
                                     OLD_SABER_GROUND1_STARTUP,
                                     OLD_SABER_GROUND1_ACTIVE, 0);
  for (unsigned tick = 1; tick < OLD_SABER_GROUND1_CHAIN_CLOSE; ++tick) {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (!ground_snapshot_matches(&snapshot, 0, 1,
                                 OLD_SABER_GROUND1_STARTUP,
                                 OLD_SABER_GROUND1_ACTIVE, tick))
      phase_ok = false;
  }
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 1, 2,
                               OLD_SABER_GROUND2_STARTUP,
                               OLD_SABER_GROUND2_ACTIVE, 0))
    phase_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND2_CHAIN_CLOSE; ++tick) {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (!ground_snapshot_matches(&snapshot, 1, 2,
                                 OLD_SABER_GROUND2_STARTUP,
                                 OLD_SABER_GROUND2_ACTIVE, tick))
      phase_ok = false;
  }
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 2, 3,
                               OLD_SABER_GROUND3_STARTUP,
                               OLD_SABER_GROUND3_ACTIVE, 0))
    phase_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND3_TOTAL; ++tick) {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (!ground_snapshot_matches(&snapshot, 2, 3,
                                 OLD_SABER_GROUND3_STARTUP,
                                 OLD_SABER_GROUND3_ACTIVE, tick))
      phase_ok = false;
  }
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(phase_ok && snapshot.phase == SABER_PHASE_IDLE,
        "separate Y taps chain ground animations 1, 2, 3 with old phases");

  /* One early edge is held until the first chain-open tick, while a second
   * early edge cannot create a second queued entry. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned tick = 1; tick < OLD_SABER_GROUND1_BUFFER_OPEN; ++tick)
    frame(0);
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 0, 1,
                               OLD_SABER_GROUND1_STARTUP,
                               OLD_SABER_GROUND1_ACTIVE,
                               OLD_SABER_GROUND1_BUFFER_OPEN))
    buffer_ok = false;
  for (unsigned tick = OLD_SABER_GROUND1_BUFFER_OPEN + 1;
       tick < OLD_SABER_GROUND1_CHAIN_OPEN; ++tick)
    frame(0);
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 1, 2,
                               OLD_SABER_GROUND2_STARTUP,
                               OLD_SABER_GROUND2_ACTIVE, 0))
    buffer_ok = false;

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned tick = 1; tick < OLD_SABER_GROUND1_BUFFER_OPEN; ++tick)
    frame(0);
  frame(SNES_PAD_Y);
  frame(0);
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 0, 1,
                               OLD_SABER_GROUND1_STARTUP,
                               OLD_SABER_GROUND1_ACTIVE,
                               OLD_SABER_GROUND1_BUFFER_OPEN + 2))
    buffer_ok = false;
  for (unsigned tick = OLD_SABER_GROUND1_BUFFER_OPEN + 3;
       tick < OLD_SABER_GROUND1_CHAIN_OPEN; ++tick)
    frame(0);
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 1, 2,
                               OLD_SABER_GROUND2_STARTUP,
                               OLD_SABER_GROUND2_ACTIVE, 0))
    buffer_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND2_TOTAL; ++tick) {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind == SABER_KIND_GROUND3) saw_ground3 = true;
  }
  frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(buffer_ok && !saw_ground3 && snapshot.phase == SABER_PHASE_IDLE,
        "one early Y edge buffers at most one combo entry");

  /* A held Y is one physical edge, even when the chain windows pass. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned tick = 0; tick < OLD_SABER_GROUND1_TOTAL + 2; ++tick) {
    frame(SNES_PAD_Y);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind != SABER_KIND_NONE && snapshot.kind != SABER_KIND_GROUND1)
      held_ok = false;
  }
  check(held_ok && snapshot.phase == SABER_PHASE_IDLE,
        "holding Y never chains beyond ground slash 1");

  /* A press on the frame after chain-close is rejected while the swing is
   * completing; a later edge after the idle frame starts slash 1. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned tick = 1; tick < OLD_SABER_GROUND1_TOTAL; ++tick)
    frame(0);
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (snapshot.phase != SABER_PHASE_IDLE)
    boundary_ok = false;
  frame(0);
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 0, 1,
                               OLD_SABER_GROUND1_STARTUP,
                               OLD_SABER_GROUND1_ACTIVE, 0))
    boundary_ok = false;
  check(boundary_ok, "out-of-window Y is ignored until the post-IDLE press");

  /* Direction is locked on the first frame of the accepted next swing. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y | SNES_PAD_RIGHT);
  if (g_ram[0x0c11] != 0x40 || !(g_ram[0x0bb9] & 0x40))
    facing_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND1_CHAIN_OPEN; ++tick) {
    frame(SNES_PAD_LEFT);
    if (g_ram[0x0c11] != 0x40 || !(g_ram[0x0bb9] & 0x40))
      facing_ok = false;
  }
  frame(SNES_PAD_Y | SNES_PAD_LEFT);
  snapshot = MmxSaberAttackSnapshotGet();
  if (!ground_snapshot_matches(&snapshot, 1, 2,
                               OLD_SABER_GROUND2_STARTUP,
                               OLD_SABER_GROUND2_ACTIVE, 0) ||
      g_ram[0x0c11] != 0 || (g_ram[0x0bb9] & 0x40))
    facing_ok = false;
  check(facing_ok,
        "held direction turns only when the next ground swing is accepted");

  /* Preserve the old pre-player VX stop through all three accepted swings. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  uint16_t initial_x = (uint16_t)read_ram_word(g_ram, 0x0bad);
  for (unsigned tick = 0; tick < 12; ++tick)
    frame(SNES_PAD_RIGHT);
  running_x = (uint16_t)read_ram_word(g_ram, 0x0bad);
  check(running_x != initial_x, "right input establishes horizontal movement");
  frame(SNES_PAD_Y | SNES_PAD_RIGHT);
  stopped_x = (uint16_t)read_ram_word(g_ram, 0x0bad);
  if (stopped_x != running_x || g_ram[0x0bc2] != 0 || g_ram[0x0bc3] != 0)
    velocity_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND1_CHAIN_CLOSE; ++tick) {
    frame(SNES_PAD_RIGHT);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.phase != SABER_PHASE_IDLE) {
      if (read_ram_word(g_ram, 0x0bad) != stopped_x) position_ok = false;
      if (g_ram[0x0bc2] != 0 || g_ram[0x0bc3] != 0) velocity_ok = false;
    }
  }
  frame(SNES_PAD_Y | SNES_PAD_RIGHT);
  snapshot = MmxSaberAttackSnapshotGet();
  if (snapshot.kind != SABER_KIND_GROUND2 ||
      read_ram_word(g_ram, 0x0bad) != stopped_x ||
      g_ram[0x0bc2] != 0 || g_ram[0x0bc3] != 0)
    velocity_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND2_CHAIN_CLOSE; ++tick) {
    frame(SNES_PAD_RIGHT);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.phase != SABER_PHASE_IDLE) {
      if (read_ram_word(g_ram, 0x0bad) != stopped_x) position_ok = false;
      if (g_ram[0x0bc2] != 0 || g_ram[0x0bc3] != 0) velocity_ok = false;
    }
  }
  frame(SNES_PAD_Y | SNES_PAD_RIGHT);
  snapshot = MmxSaberAttackSnapshotGet();
  if (snapshot.kind != SABER_KIND_GROUND3 ||
      read_ram_word(g_ram, 0x0bad) != stopped_x ||
      g_ram[0x0bc2] != 0 || g_ram[0x0bc3] != 0)
    velocity_ok = false;
  for (unsigned tick = 1; tick < OLD_SABER_GROUND3_TOTAL; ++tick) {
    frame(SNES_PAD_RIGHT);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.phase != SABER_PHASE_IDLE) {
      if (read_ram_word(g_ram, 0x0bad) != stopped_x) position_ok = false;
      if (g_ram[0x0bc2] != 0 || g_ram[0x0bc3] != 0) velocity_ok = false;
    }
  }
  check(position_ok && velocity_ok,
        "ground combo holds position and writes zero VX on every swing frame");
  puts("ok: saber-ground-combo");
}

static bool saber_lifecycle_idle(void) {
  MmxSaberAttackSnapshot snapshot = MmxSaberAttackSnapshotGet();
  return snapshot.phase == SABER_PHASE_IDLE &&
      snapshot.kind == SABER_KIND_NONE && tagged_projectiles() == 0 &&
      MmxSaberAttackHitSlots() == 0;
}

static void saber_ground_lifecycle_checks(const char *fixture) {
  MmxSaberAttackSnapshot snapshot;
  unsigned cues_before;
  unsigned charge_before;

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  check(MmxSaberAttackCueCount() == 1 &&
            MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_1,
        "one accepted ground slash emits exactly one saber_1 cue");
  for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 5; ++i)
    frame(SNES_PAD_Y);
  check(MmxSaberAttackCueCount() == 1,
        "holding Y emits no second cue for the same slash");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  check(MmxSaberAttackCueCount() == 1 &&
            MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_1,
        "ground combo cue 1 is emitted after native player end");
  for (unsigned i = 1; i < OLD_SABER_GROUND1_CHAIN_CLOSE; ++i)
    frame(0);
  frame(SNES_PAD_Y);
  check(MmxSaberAttackCueCount() == 2 &&
            MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_2,
        "ground combo cue 2 is emitted once in order");
  for (unsigned i = 1; i < OLD_SABER_GROUND2_CHAIN_CLOSE; ++i)
    frame(0);
  frame(SNES_PAD_Y);
  check(MmxSaberAttackCueCount() == 3 &&
            MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_3,
        "ground combo cue 3 is emitted once in order");
  idle(OLD_SABER_GROUND3_TOTAL + 2);
  check(saber_lifecycle_idle(),
        "natural recovery end returns Saber idle and clears ownership");

  load_fixture(fixture);
  MmxSaberFrameReset();
  hold_charge_button(10, SNES_PAD_X);
  frame(SNES_PAD_X | SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  for (unsigned i = 0; i < 8 && snapshot.phase != SABER_PHASE_ACTIVE; ++i) {
    frame(SNES_PAD_X);
    snapshot = MmxSaberAttackSnapshotGet();
  }
  check(snapshot.phase == SABER_PHASE_ACTIVE,
        "hurt lifecycle probe reaches an active Saber frame");
  charge_before = MmxZeroGetState().charge;
  cues_before = MmxSaberAttackCueCount();
  g_ram[0xbaa] = 0x0e;
  g_ram[0xbab] = 0;
  frame(SNES_PAD_X);
  check(saber_lifecycle_idle() &&
            MmxSaberAttackCueCount() == cues_before &&
            MmxZeroGetState().charge == charge_before,
        "hurt exits idle, releases the slot, clears the mask, and preserves charge");
  idle(12);
  check(MmxSaberAttackCueCount() == cues_before,
        "hurt exit emits no later Saber cue");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned i = 0; i < 5; ++i) frame(0);
  charge_before = MmxZeroGetState().charge;
  MmxSaberAttackExit(g_ram, MMX_SABER_ATTACK_EXIT_HURT);
  check(saber_lifecycle_idle() && MmxZeroGetState().charge == charge_before,
        "central hurt exit releases ownership without touching charge");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned i = 0; i < 5; ++i) frame(0);
  cues_before = MmxSaberAttackCueCount();
  switch_to_x();
  if (!saber_lifecycle_idle()) frame(0);
  check(saber_lifecycle_idle() && MmxSaberAttackCueCount() == cues_before,
        "exchange to X returns Saber idle and releases its slot");
  frame(SNES_PAD_X);
  check(!MmxSaberFrameLastWroteInput(),
        "X controls remain native after exchanging out of a slash");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned i = 0; i < 5; ++i) frame(0);
  check(tagged_projectiles() == 1,
        "plugin reset probe has a live Saber-tagged slot");
  MmxSaberFrameReset();
  check(saber_lifecycle_idle(),
        "plugin reset returns Saber idle and releases tagged RAM ownership");
  puts("ok: saber-ground-lifecycle");
}

static const uint8_t kOldSaberGroundBounds[40] = {
    7, 232, 11, 14, 29, 241, 18, 23, 37, 253, 16, 11, 37, 0, 16, 8,
    24, 251, 14, 13, 13, 251, 42, 13, 235, 251, 18, 13,
    248, 240, 18, 19, 30, 240, 36, 24, 39, 245, 29, 19};

static const uint8_t kOldSaberAirBounds[40] = {
    17, 240, 16, 13, 15, 249, 30, 20, 14, 253, 42, 24, 244, 250, 18, 12,
    31, 246, 33, 19, 22, 0, 23, 15, 14, 1, 16, 12, 25, 255, 36, 9,
    46, 254, 29, 10, 14, 245, 24, 4};

static unsigned abs_difference(unsigned a, unsigned b) {
  return a > b ? a - b : b - a;
}

static unsigned reachable_ground_enemy(void) {
  const unsigned zero_x = read_ram_word(g_ram, 0x0bad);
  const unsigned zero_y = read_ram_word(g_ram, 0x0bb0);
  unsigned best = 0;
  unsigned best_distance = 0xffff;
  for (unsigned d = 0xe68; d < 0x1228; d += 64) {
    unsigned enemy_x, enemy_y, distance;
    if (!g_ram[d] || !g_ram[d + 14] || (g_ram[d + 0x27] & 127) <= 3 ||
        g_ram[d + 0x28] >= 6)
      continue;
    enemy_x = read_ram_word(g_ram, d + 5);
    enemy_y = read_ram_word(g_ram, d + 8);
    if (enemy_x < zero_x || enemy_x - zero_x > 48 ||
        abs_difference(enemy_y, zero_y) > 48)
      continue;
    distance = (enemy_x - zero_x) + abs_difference(enemy_y, zero_y);
    if (distance < best_distance) {
      best = d;
      best_distance = distance;
    }
  }
  return best;
}

static unsigned walk_to_ground_enemy(const char *fixture,
                                     unsigned *walk_frames) {
  unsigned target = 0;
  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned i = 0; i < 300 && !target; ++i) {
    frame(i < 180 ? SNES_PAD_RIGHT : SNES_PAD_RIGHT | SNES_PAD_R);
    target = reachable_ground_enemy();
    if (target && walk_frames) *walk_frames = i + 1;
  }
  return target;
}

static unsigned saber_record_pointer(const MmxSaberAttackSnapshot *snapshot) {
  const MmxSaberAttack *attack;
  if (!snapshot) return 0;
  attack = MmxSaberAttackRecord(snapshot->kind, snapshot->index);
  if (!attack) return 0;
  for (unsigned i = 0; i < attack->bounds_segment_count; ++i) {
    const MmxSaberBoundsSegment *segment = attack->bounds_segments + i;
    if (snapshot->tick >= segment->first_tick &&
        snapshot->tick <= segment->last_tick)
      return attack->bounds_pointer + i * 4;
  }
  return 0;
}

static unsigned saber_active_slot(void) {
  unsigned slot = 0;
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    if (saber_tagged_projectile(d)) {
      if (slot) return 0;
      slot = d;
    }
  return slot;
}

static void saber_ground_hit_checks(const char *fixture) {
  uint8_t saved_ground[40];
  unsigned target, walk_frames = 0, slot, bit, initial_hp;
  unsigned damage, contacts, first_contact_frame;
  uint16_t mask_on_contact = 0;
  bool startup_empty, active_ok, release_ok, mask_ok;
  uint16_t active_slot_x = 0;
  MmxSaberAttackSnapshot snapshot;

  target = walk_to_ground_enemy(fixture, &walk_frames);
  check(target != 0, "Highway walk reaches an ordinary ground-slash target");
  printf("reference: Highway ground-hit walk_frames=%u Zero=(%u,%u) "
         "enemy_slot=0x%X enemy=(%u,%u)\n",
         walk_frames, read_ram_word(g_ram, 0x0bad), read_ram_word(g_ram, 0x0bb0),
         target, read_ram_word(g_ram, target + 5), read_ram_word(g_ram, target + 8));

  frame(0);
  check(!memcmp(g_snes->cart->rom + 0x37fd8, kOldSaberGroundBounds, 40) &&
            !memcmp(g_snes->cart->rom + 0x37f40, kOldSaberAirBounds, 40),
        "Saber collision windows equal the old ground and air rectangles");

  initial_hp = g_ram[target + 0x27] & 127;
  bit = 1u << ((target - 0xe68) / 64);
  startup_empty = true;
  active_ok = true;
  release_ok = true;
  mask_ok = false;
  damage = 0;
  contacts = 0;
  first_contact_frame = 0;

  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (tagged_projectiles() != 0) startup_empty = false;
  for (unsigned i = 1; i < 45 && snapshot.phase != SABER_PHASE_IDLE; ++i) {
    unsigned hp_before = g_ram[target + 0x27] & 127;
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    slot = saber_active_slot();
    if (snapshot.phase == SABER_PHASE_STARTUP && tagged_projectiles() != 0)
      startup_empty = false;
    if (snapshot.phase == SABER_PHASE_ACTIVE) {
      unsigned hp_after = g_ram[target + 0x27] & 127;
      if (slot == 0 || tagged_projectiles() != 1 ||
          read_ram_word(g_ram, slot + 5) != read_ram_word(g_ram, 0x0bad) ||
          read_ram_word(g_ram, slot + 8) != read_ram_word(g_ram, 0x0bb0) ||
          read_ram_word(g_ram, slot + 0x20) != saber_record_pointer(&snapshot))
        active_ok = false;
      else if (!active_slot_x)
        active_slot_x = read_ram_word(g_ram, slot + 5);
      if (hp_after < hp_before) {
        unsigned delta = hp_before - hp_after;
        ++contacts;
        damage += delta;
        if (!first_contact_frame) first_contact_frame = i;
        if (delta != 3 || damage != 3) active_ok = false;
        mask_on_contact = MmxSaberAttackHitSlots();
        if (mask_on_contact & bit) mask_ok = true;
      }
    } else if (snapshot.phase == SABER_PHASE_RECOVERY && active_slot_x) {
      if (tagged_projectiles() != 0) release_ok = false;
    }
  }
  check(startup_empty && active_ok && release_ok && contacts == 1 &&
            damage == 3 && (g_ram[target + 0x27] & 127) == initial_hp - 3,
        "ground slash 1 creates one anchored tagged slot and deals 3 once");
  check(mask_ok, "ground slash 1 sets the Saber enemy mask bit");
  printf("reference: ground-hit slash1 first_contact_frame=%u "
         "damage=%u mask=0x%X\n", first_contact_frame, damage,
         mask_on_contact);

  load_fixture(fixture);
  MmxSaberFrameReset();
  target = walk_to_ground_enemy(fixture, &walk_frames);
  check(target != 0, "Highway combo route retains an ordinary target");
  initial_hp = g_ram[target + 0x27] & 127;
  bit = 1u << ((target - 0xe68) / 64);
  damage = 0;
  contacts = 0;
  bool first_seen = false;
  bool second_mask = false;
  frame(SNES_PAD_Y);
  for (unsigned i = 1; i <= 29; ++i) {
    unsigned hp_before = g_ram[target + 0x27] & 127;
    frame(i == 29 ? SNES_PAD_Y : 0);
    snapshot = MmxSaberAttackSnapshotGet();
    unsigned hp_after = g_ram[target + 0x27] & 127;
    if (hp_after < hp_before) {
      unsigned delta = hp_before - hp_after;
      ++contacts;
      damage += delta;
      if (delta == 3) {
        if (!first_seen) first_seen = (MmxSaberAttackHitSlots() & bit) != 0;
        else second_mask = (MmxSaberAttackHitSlots() & bit) != 0;
      }
    }
  }
  check(snapshot.kind == SABER_KIND_GROUND2 &&
            snapshot.phase == SABER_PHASE_ACTIVE && tagged_projectiles() == 1,
        "ground slash 2 is accepted at the combo window with one new slot");
  for (unsigned i = 0; i < 32 && snapshot.phase != SABER_PHASE_IDLE; ++i) {
    unsigned hp_before = g_ram[target + 0x27] & 127;
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    unsigned hp_after = g_ram[target + 0x27] & 127;
    if (hp_after < hp_before) {
      unsigned delta = hp_before - hp_after;
      ++contacts;
      damage += delta;
      if (delta == 3 && contacts == 2)
        second_mask = (MmxSaberAttackHitSlots() & bit) != 0;
    }
  }
  check(first_seen && contacts == 2 && damage == 6 && second_mask &&
            (g_ram[target + 0x27] & 127) == initial_hp - 6,
        "ground slash 2 can hit the same enemy again for 3 with a fresh mask");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(0);
  memcpy(saved_ground, g_snes->cart->rom + 0x37fd8, sizeof(saved_ground));
  unsigned warning_before = MmxSaberAttackCollisionWarningCount();
  g_snes->cart->rom[0x37fd8] ^= 1;
  MmxSaberAttackCollisionRom(g_snes->cart->rom, g_snes->cart->romSize);
  MmxSaberAttackCollisionRom(g_snes->cart->rom, g_snes->cart->romSize);
  check(MmxSaberAttackCollisionWarningCount() == warning_before + 1,
        "foreign collision data logs once and fails closed");
  target = walk_to_ground_enemy(fixture, &walk_frames);
  check(target != 0, "foreign-window route reaches an ordinary target");
  frame(SNES_PAD_Y);
  bool foreign_animation = false;
  bool foreign_no_hitbox = true;
  for (unsigned i = 0; i < 18; ++i) {
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.phase != SABER_PHASE_IDLE) foreign_animation = true;
    slot = saber_active_slot();
    if (snapshot.phase == SABER_PHASE_ACTIVE &&
        (slot == 0 || read_ram_word(g_ram, slot + 0x20) != 0))
      foreign_no_hitbox = false;
    frame(0);
  }
  check(foreign_animation && foreign_no_hitbox,
        "foreign collision data leaves slashes animating with no hitbox");
  memcpy(g_snes->cart->rom + 0x37fd8, saved_ground, sizeof(saved_ground));
  MmxSaberAttackCollisionRom(g_snes->cart->rom, g_snes->cart->romSize);
  check(!memcmp(g_snes->cart->rom + 0x37fd8, kOldSaberGroundBounds, 40),
        "restored collision window is owned and idempotent");
  puts("ok: saber-ground-hit");
}

static MmxSaberPadPhase old_air_phase(unsigned tick) {
  return tick < OLD_SABER_AIR_STARTUP ? SABER_PHASE_STARTUP :
      tick < OLD_SABER_AIR_STARTUP + OLD_SABER_AIR_ACTIVE ?
          SABER_PHASE_ACTIVE : SABER_PHASE_RECOVERY;
}

static unsigned record_jump_arc(const char *fixture, bool slash,
                                bool short_hop, unsigned y[96]) {
  bool airborne = false;
  unsigned count = 0;

  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned frame_number = 1; frame_number < 96; ++frame_number) {
    unsigned input = short_hop ?
        (frame_number < 6 ? SNES_PAD_B : 0) :
        (frame_number <= 20 ? SNES_PAD_B : 0);
    bool grounded;
    if (slash && frame_number == 3) input |= SNES_PAD_Y;
    frame(input);
    y[count++] = read_ram_word(g_ram, 0x0bb0);
    grounded = (g_ram[0xbd3] & 4) || (g_ram[0xbd4] & 4);
    if (!grounded)
      airborne = true;
    else if (airborne)
      return count;
  }
  return 0;
}

static unsigned empty_enemy_slot(void) {
  for (unsigned d = 0xe68; d < 0x1228; d += 64)
    if (!g_ram[d]) return d;
  return 0xe68;
}

static void saber_air_checks(const char *fixture) {
  const MmxSaberAttack *air = MmxSaberAttackRecord(SABER_KIND_AIR, 0);
  unsigned reference[96], slash[96], short_reference[96], short_slash[96];
  unsigned reference_count, slash_count, short_reference_count, short_slash_count;
  MmxSaberAttackSnapshot snapshot;
  bool phase_ok = true;
  bool startup_empty = true;
  bool active_ok = true;
  bool press_during_ok = false;
  bool first_end = false;
  bool second_start = false;
  unsigned starts = 0;
  bool was_active = false;
  unsigned slot = 0;
  unsigned direct_enemy = 0;
  bool direct_damage_ok = false;

  printf("reference: old Saber air timing startup=%u active=%u recovery=%u "
         "total=%u (oldsaber src/mmx_saber.c:322-339)\n",
         OLD_SABER_AIR_STARTUP, OLD_SABER_AIR_ACTIVE,
         OLD_SABER_AIR_RECOVERY, OLD_SABER_AIR_TOTAL);
  check(air && air->visual_animation == 4 &&
            air->startup_ticks == OLD_SABER_AIR_STARTUP &&
            air->active_ticks == OLD_SABER_AIR_ACTIVE &&
            air->recovery_ticks == OLD_SABER_AIR_RECOVERY &&
            air->total_ticks == OLD_SABER_AIR_TOTAL && air->damage == 3 &&
            air->bounds_pointer == MMX_SABER_AIR_BOUNDS_POINTER,
        "air record publishes animation 4, old timing, $FF40, and damage 3");

  reference_count = record_jump_arc(fixture, false, false, reference);
  slash_count = record_jump_arc(fixture, true, false, slash);
  short_reference_count = record_jump_arc(fixture, false, true, short_reference);
  short_slash_count = record_jump_arc(fixture, true, true, short_slash);
  check(reference_count != 0 && slash_count == reference_count &&
            !memcmp(reference, slash, reference_count * sizeof(reference[0])),
        "early air slash with B held preserves the complete native jump arc");
  check(short_reference_count != 0 &&
            short_slash_count == short_reference_count &&
            !memcmp(short_reference, short_slash,
                    short_reference_count * sizeof(short_reference[0])),
        "air slash preserves the native short-hop B-release cutoff");
  printf("reference: native air arc frames=%u short-hop frames=%u\n",
         reference_count, short_reference_count);

  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned frame_number = 1; frame_number <= 20; ++frame_number) {
    const unsigned input = frame_number <= 20 ?
        (SNES_PAD_B | (frame_number == 3 ? SNES_PAD_Y : 0)) : 0;
    frame(input);
    snapshot = MmxSaberAttackSnapshotGet();
    if (frame_number < 3) {
      if (snapshot.phase != SABER_PHASE_IDLE)
        phase_ok = false;
    } else {
      const unsigned tick = frame_number - 3;
      if (snapshot.kind != SABER_KIND_AIR || snapshot.index != 0 ||
          snapshot.anim_id != 4 || snapshot.tick != tick ||
          snapshot.phase != old_air_phase(tick))
        phase_ok = false;
    }
  }
  check(phase_ok && MmxSaberAttackCueCount() == 1 &&
            MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_1,
        "air slash publishes animation 4 through the old startup/active/recovery phases and cues saber_1");

  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned frame_number = 1; frame_number <= 20; ++frame_number) {
    frame(frame_number <= 20 ?
        (SNES_PAD_B | (frame_number == 3 ? SNES_PAD_Y : 0)) : 0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.phase == SABER_PHASE_STARTUP && tagged_projectiles() != 0)
      startup_empty = false;
    if (snapshot.phase == SABER_PHASE_ACTIVE) {
      unsigned expected_pointer = saber_record_pointer(&snapshot);
      slot = saber_active_slot();
      if (slot == 0 || tagged_projectiles() != 1 || expected_pointer == 0 ||
          read_ram_word(g_ram, slot + 0x20) != expected_pointer)
        active_ok = false;
      else if (!direct_enemy) {
        direct_enemy = empty_enemy_slot();
        direct_damage_ok = direct_enemy != 0 &&
            MmxSaberAttackDamage(g_ram, direct_enemy, slot, 1) == 3 &&
            MmxSaberAttackDamage(g_ram, direct_enemy, slot, 1) == 0 &&
            (MmxSaberAttackHitSlots() &
             (uint16_t)(1u << ((direct_enemy - 0xe68) / 64)));
      }
    }
  }
  check(!memcmp(g_snes->cart->rom + 0x37f40, kOldSaberAirBounds, 40),
        "air slash keeps the old $37F40 collision records installed");
  check(startup_empty && active_ok,
        "air ACTIVE owns one tagged slot anchored to its old air collision record");
  check(direct_damage_ok,
        "air collision damage is 3 once per enemy per swing (direct fallback)");
  puts("reference: airborne enemy contact is not required by this fixture; the air-hit fallback asserts the live tagged slot, $FF40 record, and damage callback");

  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned frame_number = 1; frame_number <= 22; ++frame_number) {
    unsigned input = SNES_PAD_B;
    if (frame_number == 3 || frame_number == 5 || frame_number == 22)
      input |= SNES_PAD_Y;
    frame(input);
    snapshot = MmxSaberAttackSnapshotGet();
    if (!was_active && snapshot.phase != SABER_PHASE_IDLE) ++starts;
    if (frame_number == 5)
      press_during_ok = snapshot.kind == SABER_KIND_AIR &&
          snapshot.tick == 2 && MmxSaberAttackCueCount() == 1;
    if (frame_number == 21)
      first_end = snapshot.phase == SABER_PHASE_IDLE &&
          snapshot.kind == SABER_KIND_NONE;
    if (frame_number == 22)
      second_start = snapshot.kind == SABER_KIND_AIR &&
          snapshot.anim_id == 4 && snapshot.tick == 0;
    was_active = snapshot.phase != SABER_PHASE_IDLE;
  }
  check(press_during_ok,
        "a Y press during an air slash does not restart or cue another swing");
  check(first_end && second_start && starts == 2 &&
            MmxSaberAttackCueCount() == 2,
        "a second airborne Y press after completion starts a second air slash and cue in one jump");
  idle(40);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE &&
            tagged_projectiles() == 0,
        "air slash landing/natural cleanup reaches idle after SaberLand");
  puts("ok: saber-air");
}

static bool saber_test_grounded(void) {
  return (g_ram[0xbd3] & 4) || (g_ram[0xbd4] & 4);
}

typedef struct SaberDashSample {
  unsigned x;
  int vx;
  uint8_t facing;
} SaberDashSample;

static int saber_signed_ram_word(const uint8_t *ram, unsigned offset) {
  return (int)(int16_t)read_ram_word(ram, offset);
}

static unsigned start_saber_dash_right(unsigned extra_input) {
  for (unsigned i = 0; i < 40; ++i) {
    frame(extra_input | SNES_PAD_A | SNES_PAD_RIGHT);
    if (g_ram[0xbaa] == 0x14 && saber_test_grounded()) return i + 1;
  }
  return 0;
}

static SaberDashSample saber_dash_sample(void) {
  return (SaberDashSample){
      read_ram_word(g_ram, 0x0bad),
      saber_signed_ram_word(g_ram, 0x0bc2),
      (uint8_t)(g_ram[0x0c11] & 0x40)};
}

static unsigned trace_native_dash(const char *fixture,
                                  SaberDashSample samples[OLD_SABER_DASH_TOTAL]) {
  unsigned started;
  load_fixture(fixture);
  MmxSaberFrameReset();
  started = start_saber_dash_right(0);
  if (!started) return 0;
  for (unsigned i = 0; i < OLD_SABER_DASH_TOTAL; ++i) {
    frame(SNES_PAD_A | SNES_PAD_RIGHT);
    samples[i] = saber_dash_sample();
  }
  return started;
}

static unsigned trace_dash_slash_motion(
    const char *fixture, unsigned direction,
    SaberDashSample samples[OLD_SABER_DASH_TOTAL]) {
  unsigned started;
  load_fixture(fixture);
  MmxSaberFrameReset();
  started = start_saber_dash_right(0);
  if (!started) return 0;
  for (unsigned i = 0; i < OLD_SABER_DASH_TOTAL; ++i) {
    frame(SNES_PAD_A | direction | (i == 0 ? SNES_PAD_Y : 0));
    samples[i] = saber_dash_sample();
  }
  return started;
}

static MmxSaberPadPhase old_dash_phase(unsigned tick) {
  return tick < OLD_SABER_DASH_STARTUP ? SABER_PHASE_STARTUP :
      tick < OLD_SABER_DASH_STARTUP + OLD_SABER_DASH_ACTIVE ?
          SABER_PHASE_ACTIVE : SABER_PHASE_RECOVERY;
}

static void saber_dash_checks(const char *fixture) {
  const MmxSaberAttack *dash = MmxSaberAttackRecord(SABER_KIND_DASH, 0);
  SaberDashSample native[OLD_SABER_DASH_TOTAL];
  SaberDashSample slash[OLD_SABER_DASH_TOTAL];
  SaberDashSample no_direction[OLD_SABER_DASH_TOTAL];
  SaberDashSample opposite[OLD_SABER_DASH_TOTAL];
  MmxSaberAttackSnapshot snapshot;
  unsigned native_start;
  unsigned slash_start;
  unsigned no_direction_start;
  unsigned opposite_start;
  bool timing_ok = true;
  bool motion_ok = true;
  bool slot_ok = true;
  bool startup_empty = true;
  bool cue_ok = true;
  bool damage_ok = false;
  unsigned active_frames = 0;
  unsigned damage_enemy = 0;

  printf("reference: old Saber dash timing startup=%u active=%u recovery=%u "
         "total=%u (oldsaber src/mmx_saber.c:362-379)\n",
         OLD_SABER_DASH_STARTUP, OLD_SABER_DASH_ACTIVE,
         OLD_SABER_DASH_RECOVERY, OLD_SABER_DASH_TOTAL);
  check(dash && dash->visual_animation == 6 &&
            dash->startup_ticks == OLD_SABER_DASH_STARTUP &&
            dash->active_ticks == OLD_SABER_DASH_ACTIVE &&
            dash->recovery_ticks == OLD_SABER_DASH_RECOVERY &&
            dash->total_ticks == OLD_SABER_DASH_TOTAL && dash->damage == 3 &&
            dash->bounds_pointer == MMX_SABER_DASH_BOUNDS_POINTER &&
            MmxSaberSfxAttackClip(MMX_SABER_SFX_ATTACK_DASH) ==
                MMX_SABER_SFX_CLIP_SABER_2,
        "dash record keeps animation 6, old timing, $FF5C, damage 3, and saber_2");

  native_start = trace_native_dash(fixture, native);
  check(native_start != 0, "Highway starts a native grounded dash to the right");
  printf("reference: Highway native dash start_frames=%u x/vx=", native_start);
  for (unsigned i = 0; i < OLD_SABER_DASH_TOTAL; ++i)
    printf("%u/%d%s", native[i].x, native[i].vx,
           i + 1 == OLD_SABER_DASH_TOTAL ? "" : ",");
  printf(" facing=0x%02X\n", native[0].facing);
  check(native[0].vx == 0x0375,
        "native Highway dash reference keeps the old X1 VX 0x0375");
  check(!memcmp(g_snes->cart->rom + 0x37f40 + 28,
                kOldSaberAirBounds + 28, 12),
        "dash slash keeps the old $FF5C records in the $37F40 window");

  load_fixture(fixture);
  MmxSaberFrameReset();
  slash_start = start_saber_dash_right(0);
  check(slash_start == native_start,
        "dash slash starts from the same native dash frame as its control");
  for (unsigned i = 0; i < OLD_SABER_DASH_TOTAL; ++i) {
    frame(SNES_PAD_A | SNES_PAD_RIGHT | (i == 0 ? SNES_PAD_Y : 0));
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind != SABER_KIND_DASH || snapshot.index != 0 ||
        snapshot.anim_id != 6 || snapshot.tick != i ||
        snapshot.phase != old_dash_phase(i))
      timing_ok = false;
    slash[i] = saber_dash_sample();
    if (slash[i].x != native[i].x || slash[i].vx != native[i].vx)
      motion_ok = false;
    if (snapshot.phase == SABER_PHASE_STARTUP && tagged_projectiles() != 0)
      startup_empty = false;
    if (snapshot.phase == SABER_PHASE_ACTIVE) {
      const unsigned slot = saber_active_slot();
      ++active_frames;
      if (!slot || tagged_projectiles() != 1 ||
          read_ram_word(g_ram, slot + 8) != read_ram_word(g_ram, 0x0bb0) ||
          read_ram_word(g_ram, slot + 0x20) !=
              saber_record_pointer(&snapshot))
        slot_ok = false;
      if (!damage_enemy && slot) {
        damage_enemy = empty_enemy_slot();
        damage_ok = MmxSaberAttackDamage(g_ram, damage_enemy, slot, 1) == 3 &&
            MmxSaberAttackDamage(g_ram, damage_enemy, slot, 1) == 0 &&
            (MmxSaberAttackHitSlots() &
             (uint16_t)(1u << ((damage_enemy - 0xe68) / 64)));
      }
    } else if (tagged_projectiles() != 0) {
      slot_ok = false;
    }
    if (i == 0 && (MmxSaberAttackCueCount() != 1 ||
                   MmxSaberSfxLastClip() != MMX_SABER_SFX_CLIP_SABER_2))
      cue_ok = false;
  }
  printf("reference: dash slash active_frames=%u startup_empty=%u slot_ok=%u "
         "damage_ok=%u\n", active_frames, startup_empty, slot_ok, damage_ok);
  check(timing_ok,
        "dash Y starts animation 6 with the old startup/active/recovery timing");
  check(motion_ok,
        "dash slash X/VX matches the plain native dash reference every frame");
  check(active_frames == OLD_SABER_DASH_ACTIVE && startup_empty && slot_ok &&
            damage_ok,
        "dash ACTIVE owns one tagged slot and deals 3 once per swing");
  check(cue_ok && MmxSaberAttackCueCount() == 1 &&
            MmxSaberSfxLastClip() == MMX_SABER_SFX_CLIP_SABER_2,
        "dash slash emits exactly one saber_2 cue");
  frame(SNES_PAD_A | SNES_PAD_RIGHT);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE &&
            tagged_projectiles() == 0,
        "dash slash natural end releases its tagged slot");

  no_direction_start = trace_dash_slash_motion(fixture, 0, no_direction);
  opposite_start = trace_dash_slash_motion(fixture, SNES_PAD_LEFT, opposite);
  bool opposite_ok = no_direction_start == native_start &&
      opposite_start == native_start;
  for (unsigned i = 0; i < OLD_SABER_DASH_TOTAL; ++i) {
    if (no_direction[i].facing != opposite[i].facing ||
        no_direction[i].vx != opposite[i].vx ||
        no_direction[i].facing != native[i].facing ||
        no_direction[i].vx != native[i].vx)
      opposite_ok = false;
  }
  check(opposite_ok,
        "holding LEFT during dash slash cannot turn or change native VX");

  load_fixture(fixture);
  MmxSaberFrameReset();
  check(start_saber_dash_right(0) != 0, "active-jump probe starts a native dash");
  frame(SNES_PAD_A | SNES_PAD_RIGHT | SNES_PAD_Y);
  frame(SNES_PAD_A | SNES_PAD_RIGHT);
  frame(SNES_PAD_A | SNES_PAD_RIGHT);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_DASH && snapshot.phase == SABER_PHASE_ACTIVE,
        "active-jump probe reaches dash ACTIVE");
  frame(SNES_PAD_A | SNES_PAD_RIGHT | SNES_PAD_B);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE &&
            !saber_test_grounded(),
        "jump during dash ACTIVE exits that frame and leaves Zero airborne");

  load_fixture(fixture);
  MmxSaberFrameReset();
  check(start_saber_dash_right(0) != 0, "startup-jump probe starts a native dash");
  frame(SNES_PAD_A | SNES_PAD_RIGHT | SNES_PAD_Y);
  frame(SNES_PAD_A | SNES_PAD_RIGHT | SNES_PAD_B);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_DASH && snapshot.tick == 1 &&
            snapshot.phase == SABER_PHASE_STARTUP && saber_test_grounded(),
        "jump during dash STARTUP is masked and does not leave the slash");

  load_fixture(fixture);
  MmxSaberFrameReset();
  hold_charge_button(30, SNES_PAD_X);
  const unsigned charge_before_dash = MmxZeroGetState().charge;
  bool charge_never_decreased = charge_before_dash != 0;
  bool shot_fired = false;
  unsigned previous_charge = charge_before_dash;
  check(start_saber_dash_right(SNES_PAD_X) != 0,
        "charged dash probe starts while X remains held");
  for (unsigned i = 0; i < 3; ++i) {
    frame(SNES_PAD_A | SNES_PAD_RIGHT | SNES_PAD_X |
          (i == 0 ? SNES_PAD_Y : 0));
    if (MmxZeroGetState().charge < previous_charge)
      charge_never_decreased = false;
    previous_charge = MmxZeroGetState().charge;
    if (native_projectiles() || MmxZeroGetState().burst) shot_fired = true;
  }
  snapshot = MmxSaberAttackSnapshotGet();
  const unsigned charge_before_jump = MmxZeroGetState().charge;
  frame(SNES_PAD_A | SNES_PAD_RIGHT | SNES_PAD_X | SNES_PAD_B);
  if (MmxZeroGetState().charge < previous_charge)
    charge_never_decreased = false;
  if (native_projectiles() || MmxZeroGetState().burst) shot_fired = true;
  check(snapshot.kind == SABER_KIND_DASH &&
            snapshot.phase == SABER_PHASE_ACTIVE && charge_before_jump >=
                charge_before_dash,
        "charged dash probe reaches ACTIVE without consuming charge");
  snapshot = MmxSaberAttackSnapshotGet();
  check(charge_never_decreased && snapshot.phase == SABER_PHASE_IDLE &&
            !saber_test_grounded() && !shot_fired && native_projectiles() == 0,
        "held X charge survives dash slash and jump-out without firing");
  puts("ok: saber-dash");
}

static bool saber_ground_cancel_charge_probe(
    const char *fixture, MmxSaberPadPhase cancel_phase, unsigned cancel_input,
    bool startup_probe) {
  MmxSaberAttackSnapshot snapshot;
  bool no_shot;
  bool accepted;
  unsigned before;
  unsigned after;

  load_fixture(fixture);
  MmxSaberFrameReset();
  hold_charge_button(30, SNES_PAD_X);
  before = MmxZeroGetState().charge;
  frame(SNES_PAD_X | SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  if (startup_probe) {
    frame(SNES_PAD_X | SNES_PAD_B);
    snapshot = MmxSaberAttackSnapshotGet();
    accepted = snapshot.kind == SABER_KIND_GROUND1 &&
        snapshot.phase == SABER_PHASE_STARTUP && saber_test_grounded();
    no_shot = native_projectiles() == 0 && tagged_projectiles() == 0;
    for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 2 &&
         MmxSaberAttackSnapshotGet().phase != SABER_PHASE_IDLE; ++i)
      frame(SNES_PAD_X);
  } else {
    for (unsigned i = 0; i < OLD_SABER_GROUND1_TOTAL + 2 &&
         snapshot.phase != cancel_phase; ++i) {
      frame(SNES_PAD_X);
      snapshot = MmxSaberAttackSnapshotGet();
    }
    check(snapshot.phase == cancel_phase,
          "ground cancel charge probe reaches its requested phase");
    frame(SNES_PAD_X | cancel_input);
    snapshot = MmxSaberAttackSnapshotGet();
    accepted = snapshot.phase == SABER_PHASE_IDLE &&
        snapshot.kind == SABER_KIND_NONE;
    no_shot = native_projectiles() == 0 && tagged_projectiles() == 0 &&
        projectiles(SABER_TIER_4_RELEASE_CLASS) == 0 &&
        projectiles(SABER_FULL_RELEASE_CLASS) == 0;
  }
  after = MmxZeroGetState().charge;
  frame(0);
  return accepted && no_shot && after >= before &&
      projectiles(SABER_TIER_4_RELEASE_CLASS) == 1 &&
      projectiles(SABER_FULL_RELEASE_CLASS) == 0;
}

static void saber_cancel_checks(const char *fixture, const char *fixture_dir) {
  static const SaberWallRoute route = {
    "OPEN-RIGHT", SNES_PAD_LEFT, SNES_PAD_B | SNES_PAD_LEFT,
    SNES_PAD_LEFT, 60, 20, 5142, 2665, 0x40, 1};
  const char *const fixture_name = "armadillo-fight.sav";
  char wall_path[4096];
  MmxSaberAttackSnapshot snapshot;
  unsigned wall_frame;
  int written;

  written = snprintf(wall_path, sizeof(wall_path), "%s/%s", fixture_dir,
                     fixture_name);
  check(written >= 0 && written < (int)sizeof(wall_path),
        "cancel wall fixture path fits");
  printf("reference: W3.3 post-native cancel observation uses old "
         "oldsaber/src/mmx_saber.c:1197-1210 action values "
         "$04/$06/$08/$12/$14; $10 remains ordinary per "
         "oldsaber/src/mmx_saber.c:1045-1065\n");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  frame(SNES_PAD_B);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_GROUND1 &&
            snapshot.phase == SABER_PHASE_STARTUP && saber_test_grounded(),
        "ground slash 1 STARTUP masks jump and does not cancel or leave ground");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  do {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
  } while (snapshot.phase != SABER_PHASE_ACTIVE);
  frame(SNES_PAD_B);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE &&
            !saber_test_grounded() &&
            (g_ram[0x0baa] == 0x04 || g_ram[0x0baa] == 0x06 ||
             g_ram[0x0baa] == 0x08),
        "ground slash 1 ACTIVE exits in the native accepted jump frame");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  do {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
  } while (snapshot.phase != SABER_PHASE_RECOVERY);
  frame(SNES_PAD_A | SNES_PAD_RIGHT);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE &&
            saber_test_grounded() && g_ram[0x0baa] == 0x14,
        "ground slash 1 RECOVERY exits in the native accepted dash frame");

  check(saber_ground_cancel_charge_probe(
            fixture, SABER_PHASE_STARTUP, SNES_PAD_B, true),
        "held tier-4 charge survives the masked STARTUP jump and one release fires class 1");
  check(saber_ground_cancel_charge_probe(
            fixture, SABER_PHASE_ACTIVE, SNES_PAD_B, false),
        "held tier-4 charge survives the accepted ACTIVE jump and one release fires class 1");
  check(saber_ground_cancel_charge_probe(
            fixture, SABER_PHASE_RECOVERY, SNES_PAD_A | SNES_PAD_RIGHT, false),
        "held tier-4 charge survives the accepted RECOVERY dash and one release fires class 1");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  do {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
  } while (snapshot.phase != SABER_PHASE_RECOVERY);
  frame(SNES_PAD_A | SNES_PAD_RIGHT);
  snapshot = MmxSaberAttackSnapshotGet();
  for (unsigned i = 0; i < 80 && g_ram[0x0baa] == 0x14; ++i)
    frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_IDLE && saber_test_grounded() &&
            g_ram[0x0baa] != 0x14,
        "cancelled ground combo returns to idle after native dash completion");
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_GROUND1 && snapshot.tick == 0,
        "the next ground Y after a cancelled combo starts slash 1");

  wall_frame = saber_wall_charge_setup(wall_path, &route);
  check(wall_frame != ~0u && g_ram[0x0baa] == 0x12 &&
            read_ram_word(g_ram, 0x0bad) == route.expected_x &&
            read_ram_word(g_ram, 0x0bb0) == route.expected_y,
        "charged cancel wall setup reaches the recorded native cling");
  const unsigned wall_charge_before = MmxZeroGetState().charge;
  frame(SNES_PAD_X | route.travel_input | SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  check(wall_charge_before >= SABER_CHARGE_TIER_1_FRAME &&
            snapshot.kind == SABER_KIND_WALL &&
            snapshot.phase == SABER_PHASE_ACTIVE,
        "wall slash ACTIVE starts from the armadillo-fight cling");
  for (unsigned i = 0; i < 3; ++i) frame(SNES_PAD_X | route.travel_input);
  const unsigned wall_cues_before_jump = MmxSaberAttackCueCount();
  frame(SNES_PAD_X | route.jump_input);
  snapshot = MmxSaberAttackSnapshotGet();
  const unsigned wall_charge_after_jump = MmxZeroGetState().charge;
  check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE &&
            wall_charge_after_jump >= wall_charge_before &&
            tagged_projectiles() == 0,
        "wall slash ACTIVE exits on the native accepted wall jump and keeps charge");
  bool saw_normal_action10 = false;
  bool action10_side_effects_ok = true;
  for (unsigned i = 0; i < 5; ++i) {
    frame(SNES_PAD_X);
    if (g_ram[0x0baa] == 0x10) {
      saw_normal_action10 = true;
      snapshot = MmxSaberAttackSnapshotGet();
      if (snapshot.phase != SABER_PHASE_IDLE || snapshot.kind != SABER_KIND_NONE ||
          tagged_projectiles() != 0 ||
          MmxSaberAttackCueCount() != wall_cues_before_jump ||
          MmxZeroGetState().charge < wall_charge_after_jump)
        action10_side_effects_ok = false;
    }
  }
  check(saw_normal_action10 && action10_side_effects_ok,
        "the post-jump native $10 frame is playable, idle, side-effect free, and charge-safe");
  frame(0);

  wall_frame = saber_wall_setup(wall_path, &route, false);
  check(wall_frame == route.expected_wall_frame && g_ram[0x0baa] == 0x12,
        "air cancel wall setup reaches the recorded native cling");
  frame(route.jump_input);
  check(g_ram[0x0baa] == 0x10 && !saber_test_grounded(),
        "air cancel probe enters native wall-jump action $10");
  for (unsigned i = 0; i < 4; ++i) frame(0);
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_AIR && snapshot.phase == SABER_PHASE_STARTUP,
        "air slash starts from the native wall-jump bridge");
  for (unsigned i = 0; i < 4; ++i) frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_AIR && snapshot.phase == SABER_PHASE_ACTIVE,
        "air cancel probe reaches ACTIVE before the wall-cling observation");
  bool air_wall_cancelled = false;
  for (unsigned i = 0; i < 16 && snapshot.phase != SABER_PHASE_IDLE; ++i) {
    frame(route.travel_input);
    snapshot = MmxSaberAttackSnapshotGet();
    if (g_ram[0x0baa] == 0x12) {
      check(snapshot.phase == SABER_PHASE_IDLE && snapshot.kind == SABER_KIND_NONE,
            "air ACTIVE wall cling retires the Saber owner through the cancel path");
      air_wall_cancelled = true;
      break;
    }
  }
  check(air_wall_cancelled,
        "air ACTIVE reaches a native wall cling for its old context rule");

  /* Separate Highway probe for the negative observation: B is pressed while
   * an air slash is ACTIVE, but native has no accepted midair jump action. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_B);
  for (unsigned i = 0; i < 4; ++i) frame(SNES_PAD_B);
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_AIR && snapshot.phase == SABER_PHASE_STARTUP,
        "Highway negative air-cancel probe starts an air slash");
  for (unsigned i = 0; i < 4; ++i) frame(0);
  frame(SNES_PAD_B);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_AIR && snapshot.phase == SABER_PHASE_ACTIVE &&
            !saber_test_grounded() &&
            g_ram[0x0baa] != 0x12 && g_ram[0x0baa] != 0x14,
        "an unaccepted airborne jump press does not cancel the air slash");
  puts("ok: saber-cancel");
}

static void saber_land_checks(const char *fixture) {
  const MmxSaberAttack *land =
      MmxSaberAttackRecord(SABER_KIND_SABER_LAND, 0);
  MmxSaberAttackSnapshot snapshot;
  unsigned land_starts = 0;
  unsigned landing_frame = 0;
  unsigned cue_at_landing = 0;
  bool saw_air = false;
  bool saw_landing_edge = false;
  bool air_replayed = false;
  bool land_projectile = false;
  bool no_land_cue = true;

  printf("reference: old SaberLand record total=%u, visual-only, old "
         "src/mmx_saber.c:382-398; old starts only from AIR landing at "
         "src/mmx_saber.c:2183-2198 and 2288-2306\n",
         OLD_SABER_LAND_TOTAL);
  check(land && land->visual_animation == 7 &&
            land->total_ticks == OLD_SABER_LAND_TOTAL &&
            land->active_ticks == 0 && land->recovery_ticks == 0 &&
            land->bounds_segments == NULL && land->bounds_segment_count == 0 &&
            land->damage == 0 && land->bounds_pointer == 0,
        "SaberLand is anim 7 for the old 18 ticks with no hitbox or damage");

  load_fixture(fixture);
  MmxSaberFrameReset();
  for (unsigned frame_number = 1; frame_number <= 120; ++frame_number) {
    const bool was_grounded = saber_test_grounded();
    const MmxSaberAttackSnapshot before = MmxSaberAttackSnapshotGet();
    unsigned input = frame_number <= 20 ? SNES_PAD_B : 0;
    if (frame_number == 3) input |= SNES_PAD_Y;
    frame(input);
    snapshot = MmxSaberAttackSnapshotGet();
    if (before.kind == SABER_KIND_AIR) saw_air = true;
    if (!was_grounded && saber_test_grounded()) saw_landing_edge = true;
    if (snapshot.kind == SABER_KIND_AIR && snapshot.anim_id == 4 &&
        saw_landing_edge)
      air_replayed = true;
    if (snapshot.kind == SABER_KIND_SABER_LAND && snapshot.tick == 0) {
      ++land_starts;
      if (!landing_frame) landing_frame = frame_number;
      check(saw_air && saw_landing_edge,
            "air slash landing claims SaberLand on the grounded edge");
      cue_at_landing = MmxSaberAttackCueCount();
    }
    if (snapshot.kind == SABER_KIND_SABER_LAND && tagged_projectiles() != 0)
      land_projectile = true;
    if (landing_frame && MmxSaberAttackCueCount() != cue_at_landing)
      no_land_cue = false;
    if (landing_frame && frame_number > landing_frame &&
        snapshot.kind == SABER_KIND_AIR)
      air_replayed = true;
    if (landing_frame && frame_number > landing_frame + OLD_SABER_LAND_TOTAL + 4)
      break;
  }
  check(land_starts == 1 && landing_frame != 0,
        "one and only one SaberLand starts on the air-slash landing edge");
  check(!air_replayed,
        "air animation 4 never replays after the landing owner starts");
  check(!land_projectile && tagged_projectiles() == 0,
        "SaberLand owns no tagged projectile slot");
  check(cue_at_landing == 1 && no_land_cue,
        "SaberLand emits no cue beyond the air slash cue");

  load_fixture(fixture);
  MmxSaberFrameReset();
  bool land_y_claimed = false;
  for (unsigned frame_number = 1; frame_number <= 120; ++frame_number) {
    unsigned input = frame_number <= 20 ? SNES_PAD_B : 0;
    if (frame_number == 3) input |= SNES_PAD_Y;
    frame(input);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind == SABER_KIND_SABER_LAND && snapshot.tick == 0) {
      frame(SNES_PAD_Y);
      snapshot = MmxSaberAttackSnapshotGet();
      land_y_claimed = snapshot.kind == SABER_KIND_GROUND1 &&
          snapshot.tick == 0 && tagged_projectiles() == 0 &&
          MmxSaberAttackCueCount() == 2;
      break;
    }
  }
  check(land_y_claimed,
        "Y during SaberLand cancels it and starts ground slash 1 per the donor");

  /* D2: the donor has no plain-jump landing call. save0's native landing
   * action is also outside the playable context, so this remains zero. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  unsigned plain_landing_starts = 0;
  bool plain_landing_edge = false;
  for (unsigned frame_number = 1; frame_number <= 120; ++frame_number) {
    const bool was_grounded = saber_test_grounded();
    frame(frame_number <= 20 ? SNES_PAD_B : 0);
    snapshot = MmxSaberAttackSnapshotGet();
    if (!was_grounded && saber_test_grounded()) plain_landing_edge = true;
    if (snapshot.kind == SABER_KIND_SABER_LAND && snapshot.tick == 0)
      ++plain_landing_starts;
    if (plain_landing_edge) break;
  }
  check(plain_landing_edge && plain_landing_starts == 0,
        "plain jump follows the old rule and does not start SaberLand");

  /* D4/D5c: keep a held X charge through the air-owner landing and ensure no
   * native buster shot is born while X remains held. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  hold_charge_button(30, SNES_PAD_X);
  const unsigned charge_before_jump = MmxZeroGetState().charge;
  unsigned charge_before_landing = charge_before_jump;
  unsigned charge_at_landing = 0;
  bool charge_never_decreased = true;
  bool shot_fired = false;
  bool charge_landing_seen = false;
  for (unsigned frame_number = 1; frame_number <= 120; ++frame_number) {
    const unsigned previous_charge = MmxZeroGetState().charge;
    unsigned input = frame_number <= 20 ? SNES_PAD_B | SNES_PAD_X : SNES_PAD_X;
    if (frame_number == 3) input |= SNES_PAD_Y;
    frame(input);
    if (MmxZeroGetState().charge < previous_charge)
      charge_never_decreased = false;
    if (native_projectiles() != 0) shot_fired = true;
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind == SABER_KIND_SABER_LAND && snapshot.tick == 0 &&
        !charge_landing_seen) {
      charge_landing_seen = true;
      charge_at_landing = MmxZeroGetState().charge;
      charge_before_landing = previous_charge;
    }
    if (charge_landing_seen && snapshot.phase == SABER_PHASE_IDLE) break;
  }
  check(charge_landing_seen && charge_at_landing >= charge_before_landing &&
            charge_at_landing >= charge_before_jump && charge_never_decreased,
        "held X charge never decreases across SaberLand");
  check(!shot_fired, "held X during SaberLand never fires a native shot");

  /* D5d: repeat the same AIR-owner landing twice; each edge gets one visual. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  unsigned consecutive_land_starts = 0;
  for (unsigned jump = 0; jump < 2; ++jump) {
    bool landed = false;
    for (unsigned frame_number = 1; frame_number <= 120; ++frame_number) {
      unsigned input = frame_number <= 20 ? SNES_PAD_B : 0;
      if (frame_number == 3) input |= SNES_PAD_Y;
      frame(input);
      snapshot = MmxSaberAttackSnapshotGet();
      if (snapshot.kind == SABER_KIND_SABER_LAND && snapshot.tick == 0) {
        ++consecutive_land_starts;
        landed = true;
        break;
      }
    }
    check(landed, "each consecutive air-slash jump reaches its landing visual");
    idle(OLD_SABER_LAND_TOTAL + 8);
  }
  check(consecutive_land_starts == 2,
        "two consecutive air-slash jumps produce one SaberLand per landing");
  puts("ok: saber-land");
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

static bool saber_window_empty(const uint8_t *window) {
  for (unsigned i = 0; i < 40; ++i)
    if (window[i] != 0xff) return false;
  return true;
}

static void saber_lifecycle_load_checks(const char *x1_rom,
                                        const char *x3_rom,
                                        const char *fixture,
                                        const char *assets) {
  MmxSaberAttackSnapshot snapshot;
  const size_t snapshot_capacity = RtlSaveSnapshotToMemory(NULL, 0);
  uint8_t *airborne_snapshot = malloc(snapshot_capacity);

  check(snapshot_capacity != 0 && airborne_snapshot != NULL,
        "lifecycle test allocates an in-memory snapshot buffer");

  /* D1: SetState enters the reset seam once for its reset and once again
   * after the replacement state has been installed. Keep this assertion
   * separate from Saber behavior so either generic call cannot disappear. */
  MmxZeroExtension probe = { .state_reset = zero_state_reset_probe };
  const MmxZeroState zero_state = MmxZeroGetState();
  zero_state_reset_calls = 0;
  MmxZeroSetExtension(&probe);
  MmxZeroSetState(zero_state);
  MmxZeroSetExtension(MmxSaberFrameExtension());
  check(zero_state_reset_calls == 2,
        "state replacement calls the generic reset seam at both boundaries");

  /* D4a: replace a live Saber attack with the standing save in the same
   * process. The callback must retire the guest slot before the next frame. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned i = 0; i < OLD_SABER_GROUND1_ACTIVE; ++i) frame(0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.phase == SABER_PHASE_ACTIVE && tagged_projectiles() == 1,
        "hot-load probe reaches ACTIVE with a live Saber-tagged slot");
  check(RtlLoadSnapshot(fixture),
        "hot-load probe replaces the running state with save0.sav");
  check(saber_lifecycle_idle(),
        "state replacement immediately returns Saber idle and retires its slot");
  const unsigned cues_after_load = MmxSaberAttackCueCount();
  idle(OLD_SABER_GROUND1_TOTAL);
  check(MmxSaberAttackCueCount() == cues_after_load,
        "state replacement emits no delayed Saber cue");
  frame(SNES_PAD_Y);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_GROUND1 &&
            snapshot.phase == SABER_PHASE_STARTUP && snapshot.tick == 0,
        "the next Y after a hot load starts ground slash 1 normally");

  /* D4b: save an actually airborne guest state through the harness API,
   * replace an active Saber state with it, and inspect the first frames. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  bool airborne = false;
  for (unsigned i = 0; i < 60; ++i) {
    frame(SNES_PAD_B);
    if (!(g_ram[0xbd3] & 4)) {
      airborne = true;
      break;
    }
  }
  check(airborne, "lifecycle harness reaches an airborne state");
  const size_t airborne_size =
      RtlSaveSnapshotToMemory(airborne_snapshot, snapshot_capacity);
  check(airborne_size != 0, "airborne state saves through the harness API");

  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(SNES_PAD_Y);
  for (unsigned i = 0; i < OLD_SABER_GROUND1_ACTIVE; ++i) frame(0);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_ACTIVE,
        "airborne-load probe starts from a live Saber attack");
  check(RtlLoadSnapshotFromMemory(airborne_snapshot, airborne_size),
        "airborne load replaces the running Saber state");
  bool fake_landing = false;
  for (unsigned i = 0; i < 3; ++i) {
    frame(0);
    snapshot = MmxSaberAttackSnapshotGet();
    fake_landing |= snapshot.kind == SABER_KIND_SABER_LAND;
  }
  check(!fake_landing,
        "loading airborne RAM does not synthesize SaberLand before a landing edge");

  /* D4c: release X during an attack, then replace the state. A fresh X edge
   * must remain a fresh edge; a stale CR1 release latch would suppress it. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  hold_charge_button(10, SNES_PAD_X);
  frame(SNES_PAD_X | SNES_PAD_Y);
  for (unsigned i = 0; i < OLD_SABER_GROUND1_ACTIVE; ++i)
    frame(SNES_PAD_X);
  frame(0);
  check(MmxSaberAttackSnapshotGet().phase != SABER_PHASE_IDLE,
        "CR1 probe releases X while the Saber attack is still live");
  check(RtlLoadSnapshot(fixture),
        "CR1 probe replaces the running state before attack completion");
  /* The fixture's upstream save is allowed to carry an ordinary charge; clear
   * that upstream state so the shot assertion isolates the stale Saber latch. */
  MmxZeroCancel(g_ram);
  unsigned char native_before[8];
  for (unsigned i = 0; i < 8; ++i) {
    const unsigned d = 0x1228 + i * 64;
    native_before[i] = (unsigned char)(g_ram[d] && !saber_tagged_projectile(d));
  }
  /* Feed only the Saber pre-player seam with a fresh X edge. This exposes the
   * mapped press without allowing native buster code to create an ordinary
   * uncharged shot that would obscure the stale-latch assertion. */
  g_ram[0x00a7] = MMX_SABER_NATIVE_FIRE_BIT;
  g_ram[0x00a9] = 0;
  g_ram[0x00ac] = 0;
  MmxZeroExtPrePlayer(g_ram);
  check((g_ram[0x0be3] & MMX_SABER_NATIVE_FIRE_BIT) != 0,
        "state replacement clears the CR1 latch before the next X edge");
  frame(0);
  bool native_birth = false;
  for (unsigned i = 0; i < 8; ++i) {
    const unsigned d = 0x1228 + i * 64;
    native_birth |= g_ram[d] && !saber_tagged_projectile(d) && !native_before[i];
  }
  check(!native_birth,
        "state replacement fires no stale charged shot");

  /* D4d: use the same provider activation/deactivation route as the asset
   * lifecycle checks, then verify both ownership cleanup and reinstallation. */
  load_fixture(fixture);
  MmxSaberFrameReset();
  frame(0);
  check(!memcmp(g_snes->cart->rom + 0x37fd8, kOldSaberGroundBounds, 40) &&
            !memcmp(g_snes->cart->rom + 0x37f40, kOldSaberAirBounds, 40),
        "disable probe starts with both Saber collision windows installed");
  activate_zero(x1_rom, x3_rom, assets, false, false);
  check(saber_window_empty(g_snes->cart->rom + 0x37fd8) &&
            saber_window_empty(g_snes->cart->rom + 0x37f40),
        "disabling Saber restores only its collision windows to $FF");
  activate_zero(x1_rom, x3_rom, assets, true, true);
  load_fixture(fixture);
  frame(0);
  check(!memcmp(g_snes->cart->rom + 0x37fd8, kOldSaberGroundBounds, 40) &&
            !memcmp(g_snes->cart->rom + 0x37f40, kOldSaberAirBounds, 40),
        "re-enabling Saber reinstalls both collision windows");

  free(airborne_snapshot);
  puts("ok: saber-lifecycle-load");
}

int main(int argc, char **argv) {
  check(argc == 2, "X1 ROM supplied");
  const char *fixture = getenv("MMX_ZERO_TEST_FIXTURE");
  const char *x3_rom = getenv("MMX_COOP_X3_ROM");
  const char *assets = getenv("MMX_ZERO_TEST_ASSETS");
  const char *only = getenv("MMX_SABER_TEST_ONLY");
  const char *fixture_dir = getenv("MMX_SABER_FIXTURE_DIR");
  check(fixture && fixture[0], "MMX_ZERO_TEST_FIXTURE supplied");
  check(x3_rom && x3_rom[0], "MMX_COOP_X3_ROM supplied");
  check(assets && assets[0], "MMX_ZERO_TEST_ASSETS supplied");
  if (only && (!strcmp(only, "fixtures") || !strcmp(only, "saber-wall") ||
      !strcmp(only, "saber-cancel")))
    check(fixture_dir && fixture_dir[0], "MMX_SABER_FIXTURE_DIR supplied");

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
  const bool saber_ground_combo = only && !strcmp(only, "saber-ground-combo");
  const bool saber_air = only && !strcmp(only, "saber-air");
  const bool saber_wall = only && !strcmp(only, "saber-wall");
  const bool saber_dash = only && !strcmp(only, "saber-dash");
  const bool saber_cancel = only && !strcmp(only, "saber-cancel");
  const bool saber_land = only && !strcmp(only, "saber-land");
  const bool saber_ground_lifecycle = only && !strcmp(only, "saber-ground-lifecycle");
  const bool saber_lifecycle_load = only && !strcmp(only, "saber-lifecycle-load");
  const bool saber_ground_hit = only && !strcmp(only, "saber-ground-hit");
  const bool x3_zero_specials = only && !strcmp(only, "x3-zero-specials");
  const bool fixtures = only && !strcmp(only, "fixtures");
  const bool saber_enabled_group = saber_package || zero_extension ||
      saber_input || saber_ground_1 || saber_ground_combo ||
      saber_air || saber_wall || saber_dash || saber_land || saber_ground_lifecycle ||
      saber_ground_hit || saber_cancel || saber_lifecycle_load;
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
  } else if (saber_ground_combo) {
    check(MmxSaberEnabled(),
          "saber-ground-combo runs with the Saber package enabled");
    saber_ground_combo_checks(fixture);
  } else if (saber_air) {
    check(MmxSaberEnabled(), "saber-air runs with the Saber package enabled");
    saber_air_checks(fixture);
  } else if (saber_wall) {
    check(MmxSaberEnabled(), "saber-wall runs with the Saber package enabled");
    saber_wall_checks(fixture_dir);
  } else if (saber_dash) {
    check(MmxSaberEnabled(), "saber-dash runs with the Saber package enabled");
    saber_dash_checks(fixture);
  } else if (saber_cancel) {
    check(MmxSaberEnabled(), "saber-cancel runs with the Saber package enabled");
    saber_cancel_checks(fixture, fixture_dir);
  } else if (saber_land) {
    check(MmxSaberEnabled(), "saber-land runs with the Saber package enabled");
    saber_land_checks(fixture);
  } else if (saber_ground_lifecycle) {
    check(MmxSaberEnabled(),
          "saber-ground-lifecycle runs with the Saber package enabled");
    saber_ground_lifecycle_checks(fixture);
  } else if (saber_lifecycle_load) {
    check(MmxSaberEnabled(),
          "saber-lifecycle-load runs with the Saber package enabled");
    saber_lifecycle_load_checks(argv[1], x3_rom, fixture, assets);
  } else if (saber_ground_hit) {
    check(MmxSaberEnabled(),
          "saber-ground-hit runs with the Saber package enabled");
    saber_ground_hit_checks(fixture);
  } else if (x3_zero_specials) {
    upstream_specials = x3_zero_specials_checks(fixture);
  } else if (fixtures) {
    check(!MmxSaberEnabled(), "fixtures run with Saber disabled");
    fixture_checks(fixture_dir);
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
      saber_ground_lifecycle_checks(fixture);
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
      strcmp(only, "saber-ground-combo") &&
      strcmp(only, "saber-air") &&
      strcmp(only, "saber-wall") &&
      strcmp(only, "saber-dash") &&
      strcmp(only, "saber-cancel") &&
      strcmp(only, "saber-land") &&
      strcmp(only, "saber-ground-lifecycle") &&
      strcmp(only, "saber-lifecycle-load") &&
        strcmp(only, "saber-ground-hit") &&
        strcmp(only, "zero-extension") &&
        strcmp(only, "saber-assets") &&
        strcmp(only, "fixtures")) {
      fprintf(stderr, "FAIL: unknown MMX_SABER_TEST_ONLY group: %s\n", only);
      return 1;
    }
  }
  puts("SABER ROM CHECKS PASSED");
  return 0;
}

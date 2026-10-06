/* Saber-owned reference checks for the unmodified X3 Zero and native X1
 * weapon paths. The runner supplies a copied catalog, an isolated cache, the
 * original ROMs, and the standing Highway save0 fixture. */
#define MMX_DESKTOP_ENTRY MmxDesktopMain
#include "desktop/host_main.c"
#include MMX_GAME_MAIN
#include "mmx_zero.h"
#include "mmx_weapons.h"
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
  SABER_ONE_SHOT_FRAMES = 60,
  SABER_ONE_SHOT_PROJECTILES = 1,
};

static const char *const kMmxRomDigest =
    "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32";

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

static void hold_charge(unsigned frames) {
  while (frames--) frame(SNES_PAD_Y);
}

static void release_charge(unsigned frames) {
  hold_charge(frames);
  frame(0);
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

static void x3_plain_checks(const char *fixture) {
  load_fixture(fixture);
  unsigned char previous[8] = {0};
  unsigned births = 0;
  for (unsigned tap = 0; tap < 3; ++tap) {
    frame(SNES_PAD_Y);
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

static void x3_charge_checks(const char *fixture) {
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
    hold_charge(checkpoints[i].frames);
    MmxZeroState state = MmxZeroGetState();
    char label[96];
    snprintf(label, sizeof(label), "X3 Zero charge checkpoint %u reaches %u (tier %u)",
             checkpoints[i].frames, checkpoints[i].charge, checkpoints[i].tier);
    check(state.charge == checkpoints[i].charge && MmxZeroChargeTier(&state) == checkpoints[i].tier,
          label);
  }

  load_fixture(fixture);
  release_charge(30);
  check(projectiles(SABER_TIER_4_RELEASE_CLASS) == 1 &&
            !projectiles(2) && !projectiles(3),
        "tier-4 release fires exactly one class-1 buster projectile");

  load_fixture(fixture);
  release_charge(90);
  check(projectiles(SABER_TIER_6_RELEASE_CLASS) == 1 &&
            !projectiles(1) && !projectiles(2),
        "tier-6 release fires exactly one class-3 buster projectile");

  load_fixture(fixture);
  release_charge(150);
  check(MmxZeroGetState().combo == 1 && !MmxZeroGetState().saber_ready,
        "tier-8 release stores one X3 charged shot without saber readiness");
  idle(30);
  check(projectiles(SABER_TIER_8_RELEASE_CLASS) == 1,
        "tier-8 release emits one class-3 buster projectile");

  load_fixture(fixture);
  release_charge(SABER_CHARGE_FULL_FRAME);
  check(MmxZeroGetState().combo == 1 && MmxZeroGetState().saber_ready,
        "full release stores the X3 two-shot combo and saber readiness");
  idle(17);
  frame(SNES_PAD_Y);
  idle(9);
  check(projectiles(SABER_FULL_RELEASE_CLASS) == SABER_FULL_RELEASE_SHOTS &&
            MmxZeroGetState().combo == 2,
        "full charge release produces the observed two class-3 shots");
  printf("reference: X3 charge thresholds frames %u/%u/%u/%u; release classes 1/3/3; full shots=%u\n",
         SABER_CHARGE_TIER_1_FRAME, SABER_CHARGE_TIER_2_FRAME,
         SABER_CHARGE_TIER_3_FRAME, SABER_CHARGE_FULL_FRAME,
         projectiles(SABER_FULL_RELEASE_CLASS));
  puts("ok: x3-zero-charge-tiers");
}

static void x3_hurt_checks(const char *fixture) {
  load_fixture(fixture);
  hold_charge(60);
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
    frame(SNES_PAD_Y);
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

static void x3_jump_checks(const char *fixture) {
  load_fixture(fixture);
  hold_charge(10);
  unsigned before = MmxZeroGetState().charge;
  bool was_grounded = (g_ram[0xbd3] & 4) || (g_ram[0xbd4] & 4);
  unsigned airborne_frames = 0;
  unsigned airborne_streak = 0;
  unsigned landing_frame = 0, charge_at_landing = 0;
  bool landed = false;
  bool charge_never_lowered = true;
  for (unsigned i = 1; i <= 20; ++i) {
    frame(SNES_PAD_B | SNES_PAD_Y);
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
    frame(SNES_PAD_Y);
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
    frame(SNES_PAD_Y);
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

static void x3_post_charge_checks(const char *fixture) {
  load_fixture(fixture);
  release_charge(SABER_CHARGE_FULL_FRAME);
  idle(17);
  frame(SNES_PAD_Y);
  idle(9);
  for (unsigned i = 0; i < 240 &&
       (MmxZeroGetState().burst || MmxZeroGetState().shot_mask || g_ram[0xc25]); ++i)
    frame(0);
  check(MmxZeroGetState().combo == 2, "full X3 buster sequence reaches its stored second-shot state");
  frame(SNES_PAD_Y);
  idle(55);
  check(!MmxZeroGetState().combo && !MmxZeroGetState().slash,
        "full X3 buster sequence finishes before the plain-buster probe");
  unsigned char previous[8] = {0};
  frame(SNES_PAD_Y);
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

static void activate_zero(const char *x1_rom, const char *x3_rom,
                          const char *assets) {
  const char *root = getenv("MMX_COOP_LAUNCHER_ROOT");
  check(root && root[0], "Saber runner supplies an isolated mod catalog");
  check(readable_file(x1_rom), "X1 ROM exists");
  check(readable_file(x3_rom), "X3 ROM exists");
  check(readable_file(getenv("MMX_ZERO_TEST_FIXTURE")), "save0.sav exists");
  check(snes_mod_runtime_initialize_c(root, "megaman-x-us", kMmxRomDigest),
        "Saber catalog initializes");
  const RecompLauncherCModProvider *provider = snes_mod_runtime_launcher_provider_c();
  check(provider && provider->feature_enable && provider->feature_set_option &&
            provider->feature_resource_set_path && provider->commit,
        "Saber catalog exposes the feature/resource provider");
  check(provider->feature_enable(provider->ctx, "megaman-x.character.zero", "zero", 1),
        "upstream Zero package enables");
  check(provider->feature_set_option(provider->ctx, "megaman-x.character.zero", "zero",
                                     "start", "zero"),
        "upstream Zero package starts as Zero");
  check(provider->feature_set_option(provider->ctx, "megaman-x.character.zero", "zero",
                                     "behavior", "x3"),
        "upstream Zero package selects behavior=x3");
  check(provider->feature_resource_set_path(provider->ctx, "megaman-x.character.zero", "zero",
                                            "x3-rom", x3_rom),
        "upstream Zero package selects the X3 ROM");
  check(provider->commit(provider->ctx, x1_rom), "upstream Zero package commits for the X1 ROM");
  snes_mod_runtime_activate_plugins_c();
  check(MmxZeroEnabled() && MmxZeroActive() && !MmxZeroModern(),
        "upstream Zero package activates the legacy X3 controller");
  check(readable_file(assets), "isolated X3 Zero asset cache exists");
}

int main(int argc, char **argv) {
  check(argc == 2, "X1 ROM supplied");
  const char *fixture = getenv("MMX_ZERO_TEST_FIXTURE");
  const char *x3_rom = getenv("MMX_COOP_X3_ROM");
  const char *assets = getenv("MMX_ZERO_TEST_ASSETS");
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

  activate_zero(argv[1], x3_rom, assets);
  const char *only = getenv("MMX_SABER_TEST_ONLY");
  if (!only || !strcmp(only, "x3-plain")) x3_plain_checks(fixture);
  if (!only || !strcmp(only, "x3-charge")) x3_charge_checks(fixture);
  if (!only || !strcmp(only, "x3-hurt")) x3_hurt_checks(fixture);
  if (!only || !strcmp(only, "x3-jump")) x3_jump_checks(fixture);
  if (!only || !strcmp(only, "x3-post-charge")) x3_post_charge_checks(fixture);
  if (!only || !strcmp(only, "x1-native")) native_x1_checks(fixture);
  if (only && strcmp(only, "x3-plain") && strcmp(only, "x3-charge") &&
      strcmp(only, "x3-hurt") && strcmp(only, "x3-jump") &&
      strcmp(only, "x3-post-charge") && strcmp(only, "x1-native")) {
    fprintf(stderr, "FAIL: unknown MMX_SABER_TEST_ONLY group: %s\n", only);
    return 1;
  }
  puts("SABER ROM CHECKS PASSED");
  return 0;
}

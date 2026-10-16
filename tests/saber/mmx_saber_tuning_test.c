#include "mmx_saber_tuning.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct FakeOption {
  const char *id;
  const char *value;
} FakeOption;

typedef struct FakeOptions {
  const FakeOption *options;
  unsigned count;
} FakeOptions;

static int failures;

typedef struct ManifestDefaultSpec {
  const char *id;
  int expected;
  bool seen;
} ManifestDefaultSpec;

static void check(int condition, const char *message) {
  if (condition) return;
  fprintf(stderr, "FAIL: %s\n", message);
  ++failures;
}

static bool fake_reader(const char *id, char *value, size_t value_size,
                        void *context) {
  const FakeOptions *source = (const FakeOptions *)context;
  for (unsigned i = 0; i < source->count; ++i) {
    if (strcmp(id, source->options[i].id) != 0) continue;
    if (value_size != 0) {
      strncpy(value, source->options[i].value, value_size - 1);
      value[value_size - 1] = '\0';
    }
    return true;
  }
  return false;
}

static FILE *open_staged_manifest(const char *program_path) {
  static const char manifest_suffix[] =
      "mods/preloaded/packages/megaman-x.character.saber-zero/0.0.1/"
      "manifest.toml";
  static const char *relative_paths[] = {
    "mods/preloaded/packages/megaman-x.character.saber-zero/0.0.1/manifest.toml",
    "../mods/preloaded/packages/megaman-x.character.saber-zero/0.0.1/manifest.toml",
    "build-mingw/mods/preloaded/packages/megaman-x.character.saber-zero/0.0.1/manifest.toml",
  };
  char path[4096];

  if (program_path && program_path[0]) {
    const char *forward = strrchr(program_path, '/');
    const char *back = strrchr(program_path, '\\');
    const char *last = forward;
    if (!last)
      last = back;
    else if (back && back > last)
      last = back;
    if (last) {
      const size_t directory_size = (size_t)(last - program_path);
      if (directory_size < sizeof(path)) {
        const int written = snprintf(path, sizeof(path), "%.*s/%s",
                                     (int)directory_size, program_path,
                                     manifest_suffix);
        if (written >= 0 && written < (int)sizeof(path)) {
          FILE *manifest = fopen(path, "rb");
          if (manifest) return manifest;
        }
      }
    }
  }

  for (unsigned i = 0; i < sizeof(relative_paths) / sizeof(relative_paths[0]);
       ++i) {
    FILE *manifest = fopen(relative_paths[i], "rb");
    if (manifest) return manifest;
  }
  return NULL;
}

static bool parse_manifest_value(const char *text, int *out) {
  char *end = NULL;
  long parsed;

  if (!strcmp(text, "true")) {
    *out = 1;
    return true;
  }
  if (!strcmp(text, "false")) {
    *out = 0;
    return true;
  }
  errno = 0;
  parsed = strtol(text, &end, 10);
  if (end == text || *end != '\0' || errno == ERANGE ||
      parsed < INT_MIN || parsed > INT_MAX)
    return false;
  *out = (int)parsed;
  return true;
}

static ManifestDefaultSpec *find_manifest_spec(ManifestDefaultSpec *specs,
                                                unsigned count,
                                                const char *id) {
  for (unsigned i = 0; i < count; ++i)
    if (!strcmp(specs[i].id, id)) return specs + i;
  return NULL;
}

static void check_manifest_defaults(const char *program_path) {
  const MmxSaberTuning *tuning = MmxSaberTuningGet();
  /* Mutation red: remove special_moves from the manifest or its k_options
   * entry in mods/preloaded/packages/megaman-x.character.saber-zero/0.0.1/
   * manifest.toml or src/saber/mmx_saber_tuning.c. */
  ManifestDefaultSpec specs[] = {
    {"slash1_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_SLASH1], false},
    {"slash2_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_SLASH2], false},
    {"slash3_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_SLASH3], false},
    {"x3_finisher_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_X3_FINISHER], false},
    {"air_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_AIR], false},
    {"wall_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_WALL], false},
    {"dash_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_DASH], false},
    {"wave_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_WAVE], false},
    {"uppercut_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_UPPERCUT], false},
    {"rolling_slash_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_ROLLING], false},
    {"thrust_damage", tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_THRUST], false},
    {"boss_slash1_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_SLASH1], false},
    {"boss_slash2_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_SLASH2], false},
    {"boss_slash3_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_SLASH3], false},
    {"boss_air_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_AIR], false},
    {"boss_wall_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_WALL], false},
    {"boss_dash_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_DASH], false},
    {"boss_x3_finisher_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_X3_FINISHER], false},
    {"boss_wave_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_WAVE], false},
    {"boss_uppercut_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_UPPERCUT], false},
    {"boss_rolling_slash_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_ROLLING], false},
    {"boss_thrust_damage", tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_THRUST], false},
    {"slash1_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_SLASH1], false},
    {"slash2_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_SLASH2], false},
    {"slash3_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_SLASH3], false},
    {"air_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_AIR], false},
    {"wall_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_WALL], false},
    {"dash_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_DASH], false},
    {"charge_small_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_CHARGE_SMALL], false},
    {"charge_full_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_CHARGE_FULL], false},
    {"max_shot1_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_MAX_SHOT1], false},
    {"max_shot2_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_MAX_SHOT2], false},
    {"x3_finisher_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_X3_FINISHER], false},
    {"wave_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_WAVE], false},
    {"uppercut_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_UPPERCUT], false},
    {"rolling_slash_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_ROLLING], false},
    {"thrust_priority", tuning->priority[MMX_SABER_TUNING_PRIORITY_THRUST], false},
    {"priority_window_frames", tuning->priority_window_frames, false},
    {"finisher_window_frames", tuning->finisher_window_frames, false},
    {"uppercut_height", tuning->uppercut_height, false},
    {"thrust_bounce_height", tuning->thrust_bounce_height, false},
    {"saber_swing_volume", tuning->saber_swing_volume, false},
    {"show_hitboxes", tuning->show_hitboxes ? 1 : 0, false},
    {"start_all_upgrades", tuning->start_all_upgrades ? 1 : 0, false},
    {"special_moves", tuning->special_moves ? 1 : 0, false},
    {"unlock_special_moves", tuning->unlock_special_moves ? 1 : 0, false},
    {"crescent_effect", tuning->crescent_effect ? 1 : 0, false},
  };
  const unsigned spec_count = sizeof(specs) / sizeof(specs[0]);
  char line[256];
  char current_id[128] = {0};
  FILE *manifest = open_staged_manifest(program_path);

  check(manifest != NULL, "staged Saber manifest is available");
  if (!manifest) return;

  while (fgets(line, sizeof(line), manifest)) {
    char parsed_id[sizeof(current_id)];
    char raw_value[64];
    ManifestDefaultSpec *spec;
    int parsed_value;
    bool parsed;
    char message[192];

    if (sscanf(line, " id = \"%127[^\"]\"", parsed_id) == 1) {
      strncpy(current_id, parsed_id, sizeof(current_id) - 1);
      current_id[sizeof(current_id) - 1] = '\0';
      continue;
    }
    if (!current_id[0] ||
        sscanf(line, " default = %63s", raw_value) != 1)
      continue;
    spec = find_manifest_spec(specs, spec_count, current_id);
    if (!spec) {
      current_id[0] = '\0';
      continue;
    }
    snprintf(message, sizeof(message),
             "manifest has one default for tuning option %s", spec->id);
    check(!spec->seen, message);
    spec->seen = true;
    snprintf(message, sizeof(message),
             "manifest default for %s is parseable", spec->id);
    parsed = parse_manifest_value(raw_value, &parsed_value);
    check(parsed, message);
    if (parsed) {
      snprintf(message, sizeof(message),
               "manifest default for %s matches the compiled fallback", spec->id);
      check(parsed_value == spec->expected, message);
    }
    current_id[0] = '\0';
  }
  fclose(manifest);

  for (unsigned i = 0; i < spec_count; ++i) {
    snprintf(line, sizeof(line), "manifest contains tuning option %s",
             specs[i].id);
    check(specs[i].seen, line);
  }
}

static void check_defaults(void) {
  const MmxSaberTuning *tuning;
  MmxSaberTuningLoad(NULL, NULL);
  tuning = MmxSaberTuningGet();
  check(tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_SLASH1] == 3 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_SLASH2] == 3 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_SLASH3] == 8 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_X3_FINISHER] == 16 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_AIR] == 3 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_WALL] == 3 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_DASH] == 3 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_WAVE] == 6,
        "normal damage defaults match the manifest");
  check(tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_UPPERCUT] == 8 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_ROLLING] == 8 &&
            tuning->normal_damage[MMX_SABER_TUNING_DAMAGE_THRUST] == 4,
        "special normal damage defaults stay at 8, 8, and 4");
  check(tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_SLASH1] == 1 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_SLASH2] == 1 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_SLASH3] == 2 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_AIR] == 1 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_WALL] == 1 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_DASH] == 1 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_X3_FINISHER] == 6 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_WAVE] == 4,
        "boss damage defaults match operator decision D-OP-60");
  check(tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_UPPERCUT] == 2 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_ROLLING] == 2 &&
            tuning->boss_damage[MMX_SABER_TUNING_DAMAGE_THRUST] == 2,
        "special boss damage defaults stay at 2, 2, and 2");
  check(tuning->priority[MMX_SABER_TUNING_PRIORITY_SLASH1] == 2 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_SLASH2] == 3 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_SLASH3] == 4 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_AIR] == 1 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_WALL] == 1 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_DASH] == 5 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_CHARGE_SMALL] == 1 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_CHARGE_FULL] == 1 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_MAX_SHOT1] == 2 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_MAX_SHOT2] == 3 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_X3_FINISHER] == 4 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_WAVE] == 5,
        "priority defaults match the manifest");
  check(tuning->priority[MMX_SABER_TUNING_PRIORITY_UPPERCUT] == 6 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_ROLLING] == 7 &&
            tuning->priority[MMX_SABER_TUNING_PRIORITY_THRUST] == 8,
        "special priority defaults stay at 6, 7, and 8");
  check(MmxSaberTuningPriorityWindowFrames() == 70 &&
            MmxSaberTuningFinisherWindowFrames() == 27 &&
            MmxSaberTuningSaberSwingVolume() == 50,
        "window and volume defaults match the manifest");
  check(tuning->uppercut_height == 130 &&
            MmxSaberTuningUppercutHeight() == 130,
        "uppercut height defaults to 130 percent");
  check(tuning->thrust_bounce_height == 60 &&
            MmxSaberTuningThrustBounceHeight() == 60,
        "thrust bounce height defaults to 60 percent");
  check(!tuning->show_hitboxes && !MmxSaberTuningShowHitboxes(),
        "show_hitboxes defaults off");
  check(!tuning->start_all_upgrades && !MmxSaberTuningStartAllUpgrades(),
        "start_all_upgrades defaults off");
  /* Mutation red: remove or miswire the special_moves k_options entry in
   * src/saber/mmx_saber_tuning.c. */
  check(tuning->special_moves && MmxSaberTuningSpecialMoves(),
        "special_moves defaults on");
  check(!tuning->unlock_special_moves &&
            !MmxSaberTuningUnlockSpecialMoves(),
        "unlock_special_moves defaults off");
  check(tuning->crescent_effect && MmxSaberTuningCrescentEffect(),
        "crescent_effect defaults on");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_SLASH3) == 2,
        "boss slash 3 default returns the tuned boss value");
}

static void check_each_id_maps_to_field(void) {
  static const FakeOption options[] = {
    {"slash1_damage", "11"}, {"slash2_damage", "12"},
    {"slash3_damage", "13"}, {"x3_finisher_damage", "14"},
    {"air_damage", "15"}, {"wall_damage", "16"},
    {"dash_damage", "17"}, {"wave_damage", "18"},
    {"uppercut_damage", "19"}, {"rolling_slash_damage", "20"},
    {"thrust_damage", "21"},
    {"boss_slash1_damage", "21"}, {"boss_slash2_damage", "22"},
    {"boss_slash3_damage", "23"}, {"boss_air_damage", "24"},
    {"boss_wall_damage", "25"}, {"boss_dash_damage", "26"},
    {"boss_x3_finisher_damage", "27"}, {"boss_wave_damage", "28"},
    {"boss_uppercut_damage", "31"}, {"boss_rolling_slash_damage", "32"},
    {"boss_thrust_damage", "30"},
    {"slash1_priority", "1"}, {"slash2_priority", "2"},
    {"slash3_priority", "3"}, {"air_priority", "4"},
    {"wall_priority", "5"}, {"dash_priority", "6"},
    {"charge_small_priority", "7"}, {"charge_full_priority", "8"},
    {"max_shot1_priority", "9"}, {"max_shot2_priority", "1"},
    {"x3_finisher_priority", "2"}, {"wave_priority", "3"},
    {"uppercut_priority", "4"}, {"rolling_slash_priority", "5"},
    {"thrust_priority", "6"},
    {"priority_window_frames", "71"}, {"finisher_window_frames", "60"},
    {"uppercut_height", "155"},
    {"thrust_bounce_height", "88"},
    {"saber_swing_volume", "90"},
    {"show_hitboxes", "true"},
    {"start_all_upgrades", "true"},
    {"special_moves", "false"},
    {"unlock_special_moves", "true"},
    {"crescent_effect", "false"},
  };
  FakeOptions source = {options, sizeof(options) / sizeof(options[0])};
  static const int expected_boss[] = {21, 22, 23, 27, 24, 25, 26, 28};

  MmxSaberTuningLoad(fake_reader, &source);
  for (unsigned i = 0; i <= MMX_SABER_TUNING_DAMAGE_WAVE; ++i) {
    check(MmxSaberTuningNormalDamage((MmxSaberTuningDamageKind)i) ==
              11 + (int)i,
          "normal damage option id maps to the matching field");
    check(MmxSaberTuningBossDamage((MmxSaberTuningDamageKind)i) ==
              expected_boss[i],
          "boss damage option id maps to the matching field");
  }
  static const int expected_priority[] = {1, 2, 3, 4, 5, 6,
                                          7, 8, 9, 1, 2, 3};
  for (unsigned i = 0; i <= MMX_SABER_TUNING_PRIORITY_WAVE; ++i)
    check(MmxSaberTuningPriority((MmxSaberTuningPriorityKind)i) ==
              expected_priority[i],
          "priority option id maps to the matching field");
  check(MmxSaberTuningPriorityWindowFrames() == 71,
        "priority window option id maps to its field");
  check(MmxSaberTuningFinisherWindowFrames() == 60,
        "finisher window option id maps to its field");
  check(MmxSaberTuningUppercutHeight() == 155,
        "uppercut height option id maps to its field");
  check(MmxSaberTuningThrustBounceHeight() == 88,
        "thrust bounce height option id maps to its field");
  check(MmxSaberTuningSaberSwingVolume() == 90,
        "swing volume option id maps to its field");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_UPPERCUT) == 19,
        "uppercut normal damage option id maps to its field");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_ROLLING) == 20,
        "rolling slash normal damage option id maps to its field");
  /* Mutation red: remove or miswire the thrust_damage k_options entry. */
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_THRUST) == 21,
        "thrust normal damage option id maps to its field");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_UPPERCUT) == 31,
        "uppercut boss damage option id maps to its field");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_ROLLING) == 32,
        "rolling slash boss damage option id maps to its field");
  /* Mutation red: remove or miswire the boss_thrust_damage k_options entry. */
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_THRUST) == 30,
        "thrust boss damage option id maps to its field");
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_UPPERCUT) == 4,
        "uppercut priority option id maps to its field");
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_ROLLING) == 5,
        "rolling slash priority option id maps to its field");
  /* Mutation red: remove or miswire the thrust_priority k_options entry. */
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_THRUST) == 6,
        "thrust priority option id maps to its field");
  check(MmxSaberTuningGet()->show_hitboxes && MmxSaberTuningShowHitboxes(),
        "boolean show_hitboxes option maps to its field");
  check(MmxSaberTuningGet()->start_all_upgrades &&
            MmxSaberTuningStartAllUpgrades(),
        "boolean start_all_upgrades option maps to its field");
  /* Mutation red: remove or miswire the special_moves k_options entry in
   * src/saber/mmx_saber_tuning.c. */
  check(!MmxSaberTuningGet()->special_moves &&
            !MmxSaberTuningSpecialMoves(),
        "boolean special_moves option maps to its field");
  check(MmxSaberTuningGet()->unlock_special_moves &&
            MmxSaberTuningUnlockSpecialMoves(),
        "boolean unlock_special_moves option maps to its field");
  check(!MmxSaberTuningGet()->crescent_effect &&
            !MmxSaberTuningCrescentEffect(),
        "boolean crescent_effect option maps to its field");
}

static void check_clamping_and_invalid_fallback(void) {
  static const FakeOption options[] = {
    {"slash1_damage", "0"},
    {"slash2_damage", "99"},
    {"slash3_damage", "not-an-integer"},
    {"uppercut_damage", "0"},
    {"rolling_slash_damage", "99"},
    {"thrust_damage", "0"},
    {"boss_uppercut_damage", "-1"},
    {"boss_rolling_slash_damage", "99"},
    {"boss_thrust_damage", "-1"},
    {"priority_window_frames", "29"},
    {"finisher_window_frames", "61"},
    {"uppercut_height", "99"},
    {"thrust_bounce_height", "-1"},
    {"saber_swing_volume", "-10"},
    {"wave_priority", "10"},
    {"uppercut_priority", "0"},
    {"rolling_slash_priority", "10"},
    {"thrust_priority", "10"},
    {"start_all_upgrades", "not-a-boolean"},
    {"unlock_special_moves", "not-a-boolean"},
    {"crescent_effect", "not-a-boolean"},
    {"air_damage", "3x"},
    {"show_hitboxes", "not-a-boolean"},
    {"special_moves", "not-a-boolean"},
  };
  FakeOptions source = {options, sizeof(options) / sizeof(options[0])};

  MmxSaberTuningLoad(fake_reader, &source);
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_SLASH1) == 1,
        "normal damage clamps to its manifest minimum");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_SLASH2) == 32,
        "normal damage clamps to its manifest maximum");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_SLASH3) == 8,
        "invalid normal damage falls back to its default");
  check(MmxSaberTuningPriorityWindowFrames() == 30,
        "priority window clamps to its manifest minimum");
  check(MmxSaberTuningFinisherWindowFrames() == 60,
        "finisher window clamps to its manifest maximum");
  check(MmxSaberTuningUppercutHeight() == 100,
        "uppercut height clamps to its manifest minimum");
  check(MmxSaberTuningThrustBounceHeight() == 0,
        "thrust bounce height clamps to zero");
  check(MmxSaberTuningSaberSwingVolume() == 0,
        "swing volume clamps to its manifest minimum");
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_WAVE) == 9,
        "priority clamps to its manifest maximum");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_AIR) == 3,
        "trailing text falls back to the normal damage default");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_UPPERCUT) == 1,
        "uppercut normal damage clamps to its manifest minimum");
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_ROLLING) == 32,
        "rolling slash normal damage clamps to its manifest maximum");
  /* Mutation red: change thrust_damage min/max or remove its clamp path. */
  check(MmxSaberTuningNormalDamage(MMX_SABER_TUNING_DAMAGE_THRUST) == 1,
        "thrust normal damage clamps to its manifest minimum");
  check(MmxSaberTuningGet()->boss_damage[MMX_SABER_TUNING_DAMAGE_UPPERCUT] == 0,
        "uppercut boss damage clamps to its manifest minimum");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_ROLLING) == 32,
        "rolling slash boss damage clamps to its manifest maximum");
  /* Mutation red: change boss_thrust_damage min/max or remove its clamp path. */
  check(MmxSaberTuningGet()->boss_damage[MMX_SABER_TUNING_DAMAGE_THRUST] == 0,
        "thrust boss damage clamps to its manifest minimum");
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_UPPERCUT) == 1,
        "uppercut priority clamps to its manifest minimum");
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_ROLLING) == 9,
        "rolling slash priority clamps to its manifest maximum");
  /* Mutation red: change thrust_priority min/max or remove its clamp path. */
  check(MmxSaberTuningPriority(MMX_SABER_TUNING_PRIORITY_THRUST) == 9,
        "thrust priority clamps to its manifest maximum");
  check(!MmxSaberTuningShowHitboxes(),
        "invalid boolean falls back to show_hitboxes off");
  check(!MmxSaberTuningStartAllUpgrades(),
        "invalid boolean falls back to start_all_upgrades off");
  /* Mutation red: change the special_moves default or invalid-boolean path in
   * src/saber/mmx_saber_tuning.c. */
  check(MmxSaberTuningSpecialMoves(),
        "invalid boolean falls back to special_moves on");
  check(!MmxSaberTuningUnlockSpecialMoves(),
        "invalid boolean falls back to unlock_special_moves off");
  check(MmxSaberTuningCrescentEffect(),
        "invalid boolean falls back to crescent_effect on");

  static const FakeOption high_options[] = {
    {"uppercut_height", "201"},
    {"thrust_bounce_height", "151"},
  };
  FakeOptions high_source = {
    high_options, sizeof(high_options) / sizeof(high_options[0])};
  MmxSaberTuningLoad(fake_reader, &high_source);
  check(MmxSaberTuningUppercutHeight() == 200,
        "uppercut height clamps to its manifest maximum");
  check(MmxSaberTuningThrustBounceHeight() == 150,
        "thrust bounce height clamps to 150 percent");
}

static void check_boss_semantics(void) {
  static const FakeOption options[] = {
    {"uppercut_damage", "32"},
    {"boss_uppercut_damage", "0"},
    {"rolling_slash_damage", "31"},
    {"boss_rolling_slash_damage", "0"},
    {"thrust_damage", "30"},
    {"boss_thrust_damage", "0"},
    {"slash3_damage", "32"},
    {"boss_slash3_damage", "0"},
    {"boss_air_damage", "5"},
  };
  FakeOptions source = {options, sizeof(options) / sizeof(options[0])};

  MmxSaberTuningLoad(fake_reader, &source);
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_UPPERCUT) == 32,
        "uppercut boss zero resolves to the tuned normal value");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_ROLLING) == 31,
        "rolling slash boss zero resolves to the tuned normal value");
  /* Mutation red: remove boss-thrust fallback or miswire its option field. */
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_THRUST) == 30,
        "thrust boss zero resolves to the tuned normal value");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_SLASH3) == 32,
        "boss zero resolves to the tuned normal value");
  check(MmxSaberTuningBossDamage(MMX_SABER_TUNING_DAMAGE_AIR) == 5,
        "explicit boss damage is returned unchanged");
}

int main(int argc, char **argv) {
  const char *program_path = argc > 0 ? argv[0] : NULL;
  check_manifest_defaults(program_path);
  check_defaults();
  check_manifest_defaults(program_path);
  check_each_id_maps_to_field();
  check_clamping_and_invalid_fallback();
  check_boss_semantics();
  if (failures != 0) return 1;
  puts("PASS: Saber tuning cache");
  return 0;
}

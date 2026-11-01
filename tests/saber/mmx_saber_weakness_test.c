#include "mmx_saber_weakness.h"

#include <stdio.h>

typedef struct WeaknessCase {
  MmxSaberWeaknessMove move;
  unsigned class_id;
  unsigned boss_actor;
} WeaknessCase;

static const WeaknessCase kCases[] = {
  {MMX_SABER_WEAKNESS_STORM_UPPERCUT, 0x09, 0x0c},
  {MMX_SABER_WEAKNESS_TORNADO, 0x09, 0x0c},
  {MMX_SABER_WEAKNESS_ROLLING_SLASH, 0x08, 0x07},
  {MMX_SABER_WEAKNESS_DOWNWARD_THRUST, 0x0a, 0x02},
  {MMX_SABER_WEAKNESS_ICE_CLONE, 0x07, 0x31},
  {MMX_SABER_WEAKNESS_THUNDER_ORB, 0x0c, 0x14},
  {MMX_SABER_WEAKNESS_CUTTER, 0x0d, 0x0a},
};

static int failures;

static void check(int condition, const char *message) {
  if (condition) return;
  fprintf(stderr, "FAIL: %s\n", message);
  ++failures;
}

int main(void) {
  const unsigned original = 0x1203;

  for (unsigned i = 0; i < sizeof(kCases) / sizeof(kCases[0]); ++i) {
    const WeaknessCase *test = &kCases[i];
    const unsigned expected = 0x1200 | test->class_id;
    check(MmxSaberWeaknessContactClass(test->move, test->boss_actor,
                                      original) == expected,
          "each Saber move returns its theme class for its exact boss");
    for (unsigned j = 0; j < sizeof(kCases) / sizeof(kCases[0]); ++j) {
      if (kCases[j].boss_actor == test->boss_actor) continue;
      check(MmxSaberWeaknessContactClass(test->move, kCases[j].boss_actor,
                                        original) == original,
            "each Saber move keeps original class on every other boss");
    }
    check(MmxSaberWeaknessContactClass(test->move, 0x22, original) == original,
          "each Saber move keeps original class on an ordinary actor");
  }

  check(MmxSaberWeaknessContactClass(MMX_SABER_WEAKNESS_NONE, 0x0c,
                                    original) == original,
        "non-Saber projectile keeps original class");
  check(MmxSaberWeaknessMoveForAttackKind(SABER_KIND_UPPERCUT) ==
            MMX_SABER_WEAKNESS_STORM_UPPERCUT &&
        MmxSaberWeaknessMoveForAttackKind(SABER_KIND_ROLLING) ==
            MMX_SABER_WEAKNESS_ROLLING_SLASH &&
        MmxSaberWeaknessMoveForAttackKind(SABER_KIND_THRUST) ==
            MMX_SABER_WEAKNESS_DOWNWARD_THRUST,
        "melee specials map to their theme rows");
  return failures != 0;
}

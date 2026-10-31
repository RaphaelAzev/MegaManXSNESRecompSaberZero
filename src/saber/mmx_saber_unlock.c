#include "mmx_saber_unlock.h"

bool MmxSaberSpecialBossDefeated(const uint8_t *ram,
                                 MmxSaberSpecialMove move) {
  static const unsigned boss_clear_offsets[SABER_SPECIAL_COUNT] = {
    [SABER_SPECIAL_UPPERCUT] = 0x1f90,
    [SABER_SPECIAL_ROLLING] = 0x1f8c,
    [SABER_SPECIAL_THRUST] = 0x1f8e,
    [SABER_SPECIAL_ICE_CLONE] = 0x1f96,
    [SABER_SPECIAL_THUNDER_SLAM] = 0x1f92,
    [SABER_SPECIAL_CUTTER_SLASH] = 0x1f94,
  };
  if (!ram || (unsigned)move >= SABER_SPECIAL_COUNT) return false;
  return (ram[boss_clear_offsets[move]] & 0x40) != 0;
}

unsigned MmxSaberDefeatedMavericks(const uint8_t *ram) {
  unsigned defeated = 0;
  if (!ram) return 0;
  /* Native boss-clear ORs $40 into $1F88+(stage-1)*2 for stages 1..8 only
   * (bank84 boss-clear routine), so Sigma stages and Vile never count; other
   * bits hold weapon energy/ownership. */
  for (unsigned i = 0; i < 8; ++i)
    defeated += (ram[0x1f88 + 2 * i] & 0x40) != 0;
  return defeated;
}

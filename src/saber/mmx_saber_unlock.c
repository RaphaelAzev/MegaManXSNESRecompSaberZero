#include "mmx_saber_unlock.h"

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

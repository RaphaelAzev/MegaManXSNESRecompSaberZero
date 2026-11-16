#include "mmx_saber_armor.h"

#include "mmx_saber_frame.h"
#include "mmx_saber_plugin.h"
#include "mmx_saber_tuning.h"
#include "../mmx_zero.h"

MmxSaberArmorFlags MmxSaberArmorDecode(uint8_t upgrades) {
  return (MmxSaberArmorFlags){
      (upgrades & MMX_SABER_ARMOR_HEAD_BIT) != 0,
      (upgrades & MMX_SABER_ARMOR_ARMS_BIT) != 0,
      (upgrades & MMX_SABER_ARMOR_BODY_BIT) != 0,
      (upgrades & MMX_SABER_ARMOR_LEGS_BIT) != 0,
      (upgrades & MMX_SABER_ARMOR_MASK) == MMX_SABER_ARMOR_MASK};
}

MmxSaberArmorFlags MmxSaberArmorCurrent(void) {
  const uint8_t *ram;
  if (!MmxSaberEnabled() || !MmxZeroActive())
    return (MmxSaberArmorFlags){false, false, false, false, false};
  ram = MmxSaberFrameRam();
  return ram ? MmxSaberArmorDecode(ram[MMX_SABER_ARMOR_RAM_OFFSET]) :
               (MmxSaberArmorFlags){false, false, false, false, false};
}

void MmxSaberArmorApplyStageStart(uint8_t *ram) {
  if (!ram || !MmxSaberEnabled() || !MmxZeroActive() ||
      !MmxSaberTuningStartAllUpgrades())
    return;
  ram[MMX_SABER_ARMOR_RAM_OFFSET] |= MMX_SABER_ARMOR_MASK;
}

void MmxSaberWeaponsApplyStageStart(uint8_t *ram) {
  enum {
    WEAPON_FIRST = 1,
    WEAPON_LAST = 8,
    WEAPON_RAM_BASE = 0x1f85,
    WEAPON_ENERGY_MASK = 0x3fff,
    WEAPON_FULL_ENERGY = 0x1c00,
    WEAPON_OWNED_WORD_BIT = 0x4000,
  };

  if (!ram || !MmxSaberEnabled() || !MmxZeroActive() ||
      !MmxSaberTuningStartAllWeapons())
    return;
  for (unsigned weapon = WEAPON_FIRST; weapon <= WEAPON_LAST; ++weapon) {
    const unsigned offset = WEAPON_RAM_BASE + 2 * weapon;
    uint16_t word = (uint16_t)(ram[offset] |
                               (uint16_t)ram[offset + 1] << 8);
    word = (uint16_t)((word & (uint16_t)~WEAPON_ENERGY_MASK) |
                      WEAPON_FULL_ENERGY | WEAPON_OWNED_WORD_BIT);
    ram[offset] = (uint8_t)word;
    ram[offset + 1] = (uint8_t)(word >> 8);
  }
}

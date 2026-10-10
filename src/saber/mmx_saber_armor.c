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

#pragma once

#include <stdbool.h>

#include "../mmx_zero.h"
#include "mmx_saber_armor.h"

/* The Saber-owned per-frame bridge registered at Zero's pre-player seam. */
const MmxZeroExtension *MmxSaberFrameExtension(void);
void MmxSaberFrameReset(void);

enum {
  /* These are native selected IDs, equal to 2*w for weapon RAM words. */
  MMX_SABER_WEAPON_CUTTER = 0x0e,
  MMX_SABER_WEAPON_STORM_TORNADO = 0x0a,
  MMX_SABER_WEAPON_SHOTGUN_ICE = 0x10,
  MMX_SABER_NORMAL_SHOT_COST = 0x0100,
  MMX_SABER_WEAPON_ENERGY_BASE = 0x1f85,
  MMX_SABER_WEAPON_ENERGY_MASK = 0x3fff,
  MMX_SABER_WEAPON_OWNED_BYTE_BIT = 0x40,
};

/* X1 normal shots use fixed-point energy.  Head halves the debit, floors it,
 * and keeps a nonzero cost at one unit. */
static inline unsigned MmxSaberNormalWeaponCost(unsigned weapon_id,
                                                 unsigned cost, bool head) {
  unsigned half;
  if (weapon_id == 0 || !cost || !head) return cost;
  half = cost >> 1;
  return half ? half : 1;
}

static inline unsigned MmxSaberFrameWeaponEnergyOffset(unsigned weapon_id) {
  return MMX_SABER_WEAPON_ENERGY_BASE + weapon_id;
}

static inline uint16_t MmxSaberFrameWeaponEnergyWord(const uint8_t *ram,
                                                     unsigned weapon_id) {
  const unsigned offset = MmxSaberFrameWeaponEnergyOffset(weapon_id);
  return (uint16_t)(ram[offset] | (uint16_t)ram[offset + 1] << 8);
}

static inline bool MmxSaberFrameWeaponHasUsableEnergy(
    const uint8_t *ram, unsigned weapon_id) {
  return ram && (MmxSaberFrameWeaponEnergyWord(ram, weapon_id) &
                 MMX_SABER_WEAPON_ENERGY_MASK) != 0;
}

static inline void MmxSaberFrameDebitNormalWeaponEnergy(
    uint8_t *ram, unsigned weapon_id, unsigned cost) {
  if (!ram) return;
  const unsigned offset = MmxSaberFrameWeaponEnergyOffset(weapon_id);
  const uint16_t original = MmxSaberFrameWeaponEnergyWord(ram, weapon_id);
  const unsigned usable = original & MMX_SABER_WEAPON_ENERGY_MASK;
  const unsigned spend = MmxSaberNormalWeaponCost(
      weapon_id, cost, MmxSaberArmorCurrent().head);
  const unsigned remaining = usable >= spend ? usable - spend : 0;
  const uint16_t stored = (uint16_t)(
      (remaining & MMX_SABER_WEAPON_ENERGY_MASK) |
      (original & (uint16_t)~MMX_SABER_WEAPON_ENERGY_MASK));

  ram[offset] = (uint8_t)stored;
  ram[offset + 1] = (uint8_t)(stored >> 8);
}

/* Whether X1's handler for player action $0BAA can fire the buster. The
 * Saber swings only where it can (hurt is handled separately). */
bool MmxSaberFrameNativeFireAction(uint8_t action);

/* The physical X/Y buttons plus held Up, Down, and Y state of the seat being run.
 * Single player reads the native joypad words ($A7 held, $A9 previous, $AC
 * newly pressed); co-op supplies each seat's own pad, and Saber then tracks
 * X's previous state. */
typedef struct MmxSaberFramePad {
  bool x_held, y_pressed;
  bool up_held, y_held;
  bool down_held;
} MmxSaberFramePad;
typedef void (*MmxSaberFramePadSource)(const uint8_t *ram, MmxSaberFramePad *pad);
void MmxSaberFrameSetPadSource(MmxSaberFramePadSource source);

/* Live RAM retained by the Saber-owned player seams for presentation reads. */
const uint8_t *MmxSaberFrameRam(void);

/* Test-only observability for the deliberate X pass-through path. */
bool MmxSaberFrameLastWroteInput(void);

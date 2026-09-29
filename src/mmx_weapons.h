#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Page 0 retains the native X1 inventory. Pages 1/2 are X2/X3. */
typedef struct MmxWeaponsState {
  uint8_t page, weapon, menu_page, initialized;
  uint8_t energy[16];
  uint16_t charge;
  uint8_t cooldown, reserved;
} MmxWeaponsState;
typedef struct MmxWeaponPose {
  int16_t left, top;
  uint16_t width, height;
  const uint8_t *pixels;
} MmxWeaponPose;
bool MmxWeaponsLoad(const char *path);
void MmxWeaponsDisable(void);
bool MmxWeaponsEnabled(void);
bool MmxWeaponsActive(void);
MmxWeaponsState MmxWeaponsGetState(void);
bool MmxWeaponsValidState(const MmxWeaponsState *state);
void MmxWeaponsSetState(MmxWeaponsState state);
const MmxWeaponPose *MmxWeaponsPose(unsigned page, unsigned weapon, unsigned group, unsigned pose);
const MmxWeaponPose *MmxWeaponsIcon(unsigned page, unsigned weapon);
const uint16_t *MmxWeaponsIconPalette(unsigned page, unsigned weapon);
const uint16_t *MmxWeaponsPalette(unsigned page, unsigned weapon, bool body);
const char *MmxWeaponsLabel(unsigned page, unsigned weapon);
bool MmxWeaponsMenuVisible(const uint8_t ram[0x20000]);
void MmxWeaponsMenuTick(uint8_t ram[0x20000], unsigned direct_page);
unsigned MmxWeaponsMenuRead(uint8_t ram[0x20000], unsigned pc, unsigned direct_page,
                            unsigned index, unsigned original);

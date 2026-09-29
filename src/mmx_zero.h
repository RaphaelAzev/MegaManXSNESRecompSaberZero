#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MMX_ZERO_WIDTH = 128, MMX_ZERO_HEIGHT = 128, MMX_ZERO_POSES = 152,
       MMX_ZERO_ANIMATION_BYTES = 0x474, MMX_ZERO_LEGACY_STATE_SIZE = 12 };
typedef struct MmxZeroState {
  uint16_t charge, slash, projectile;
  uint8_t combo, cooldown, air, facing;
  uint16_t hit_slots;
  uint16_t anim_offset;
  uint8_t anim_timer, anim_pose, anim_flags, anim_valid;
} MmxZeroState;
bool MmxZeroLoad(const char *path);
void MmxZeroDisable(void);
bool MmxZeroEnabled(void);
const uint8_t *MmxZeroPose(const uint8_t ram[0x20000], const MmxZeroState *snapshot);
const uint8_t *MmxZeroBlade(const MmxZeroState *snapshot);
const uint16_t *MmxZeroColors(void);
const uint8_t *MmxZeroMenuPose(void);
int MmxZeroLifeColor(unsigned x, unsigned y);
/* Original X3 BGR555 badge pixel; -2 is transparent, -1 retains native art. */
int MmxZeroHudColor(unsigned x, unsigned y);
void MmxZeroSetCollisionRom(uint8_t *rom, size_t size);
unsigned MmxZeroUpgradeBits(unsigned pc, unsigned original);
void MmxZeroPlayerTick(uint8_t ram[0x20000]);
void MmxZeroAnimationStart(unsigned object, unsigned sequence);
void MmxZeroAnimationAdvance(unsigned object);
unsigned MmxZeroWeaponTick(uint8_t ram[0x20000], unsigned object, unsigned active);
unsigned MmxZeroDamage(uint8_t ram[0x20000], unsigned enemy, unsigned projectile, unsigned original);
unsigned MmxZeroHitbox(const uint8_t ram[0x20000], unsigned enemy, unsigned projectile, unsigned original);
MmxZeroState MmxZeroGetState(void);
void MmxZeroSetState(MmxZeroState state);
void MmxZeroResetState(void);
void MmxZeroCancel(uint8_t ram[0x20000]);
void MmxZeroRegisterHooks(void);

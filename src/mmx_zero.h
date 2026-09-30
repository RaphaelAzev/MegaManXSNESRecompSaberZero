#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { MMX_ZERO_WIDTH = 128, MMX_ZERO_HEIGHT = 128, MMX_ZERO_POSES = 152, MMX_ZERO_CHARGE_POSES = 66,
       MMX_ZERO_ANIMATION_BYTES = 0x474, MMX_ZERO_MUZZLE_BYTES = 196,
       MMX_ZERO_LEGACY_STATE_SIZE = 12, MMX_ZERO_ANIMATION_STATE_SIZE = 18,
       MMX_ZERO_COMBAT_STATE_SIZE = 30, MMX_ZERO_SWAP_STATE_SIZE = 36 };
typedef struct MmxZeroState {
  uint16_t charge, slash, projectile;
  /* Reuses the formerly unused cooldown byte without changing save layout. */
  uint8_t combo, charge_phase, air, facing;
  uint16_t hit_slots;
  uint16_t anim_offset;
  uint8_t anim_timer, anim_pose, anim_flags, anim_valid;
  uint16_t burst_offset;
  uint8_t burst_timer, burst, burst_end, saber_ready, shot_mask, held_gravity;
  uint16_t held_vy;
  uint8_t burst_transition, burst_fired;
  /* Zero remains the default for legacy saves and initial mod activation. */
  uint8_t active_x, swap_phase, swap_tick, swap_fraction;
  int16_t swap_y;
  uint8_t hp[2], hp_valid, hp_max; /* Index 0 = Zero, 1 = X; shared maximum. */
} MmxZeroState;
bool MmxZeroLoad(const char *path);
void MmxZeroDisable(void);
bool MmxZeroEnabled(void);
bool MmxZeroActive(void);
bool MmxZeroSwapping(void);
void MmxZeroHealthSync(const uint8_t ram[0x20000]);
void MmxZeroHealthRespawn(const uint8_t ram[0x20000]);
/* Called after native NMI input polling; true suspends the game scheduler. */
bool MmxZeroSwapTick(uint8_t ram[0x20000]);
unsigned MmxZeroSwapPose(const MmxZeroState *snapshot);
const uint8_t *MmxZeroTeleportPose(unsigned pose);
const uint8_t *MmxZeroPose(const uint8_t ram[0x20000], const MmxZeroState *snapshot);
const uint8_t *MmxZeroBlade(const MmxZeroState *snapshot);
const uint16_t *MmxZeroColors(void);
const uint16_t *MmxZeroBodyColors(const MmxZeroState *snapshot);
const uint8_t *MmxZeroChargePose(const MmxZeroState *snapshot);
bool MmxZeroHasChargeArt(void);
bool MmxZeroNativeChargeObject(unsigned object, unsigned kind);
const uint8_t *MmxZeroMenuPose(void);
/* Original X3 BGR555 badge pixel; -2 is transparent, -1 retains native art. */
int MmxZeroHudColor(unsigned x, unsigned y);
void MmxZeroSetCollisionRom(uint8_t *rom, size_t size);
unsigned MmxZeroUpgradeBits(unsigned pc, unsigned original);
void MmxZeroPlayerTick(uint8_t ram[0x20000]);
void MmxZeroPlayerEnd(uint8_t ram[0x20000]);
unsigned MmxZeroChargeTier(const MmxZeroState *snapshot);
void MmxZeroAnimationStart(unsigned object, unsigned sequence);
void MmxZeroAnimationAdvance(unsigned object);
unsigned MmxZeroMuzzle(const uint8_t ram[0x20000], unsigned object,
                      unsigned native_index, unsigned axis, unsigned original);
unsigned MmxZeroWeaponOrigin(const uint8_t ram[0x20000], unsigned object,
                             unsigned axis, unsigned original);
unsigned MmxZeroWeaponTick(uint8_t ram[0x20000], unsigned object, unsigned active);
unsigned MmxZeroDamage(uint8_t ram[0x20000], unsigned enemy, unsigned projectile, unsigned original);
unsigned MmxZeroHitbox(const uint8_t ram[0x20000], unsigned enemy, unsigned projectile, unsigned original);
MmxZeroState MmxZeroGetState(void);
void MmxZeroSetState(MmxZeroState state);
void MmxZeroResetState(void);
void MmxZeroCancel(uint8_t ram[0x20000]);
void MmxZeroRegisterHooks(void);

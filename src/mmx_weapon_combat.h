#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MmxWeaponShot {
  int32_t x, y; /* 16.8 world coordinates. */
  int16_t vx, vy;
  uint16_t age, animation, hit_slots, born;
  int16_t origin_x, origin_y;
  uint8_t page, weapon, group, pose, timer, flags, facing, variant;
  uint8_t charged, radius, active, reserved;
  uint8_t tether_pose, muzzle_pose;
} MmxWeaponShot;
typedef struct MmxWeaponCombatState {
  MmxWeaponShot shots[8];
  uint16_t tick;
  uint8_t stage, valid, held, pressed, direction, reserved;
} MmxWeaponCombatState;
MmxWeaponCombatState MmxWeaponsGetCombatState(void);
bool MmxWeaponsValidCombatState(const MmxWeaponCombatState *state);
void MmxWeaponsSetCombatState(MmxWeaponCombatState state);
bool MmxWeaponsCombatActive(void);
void MmxWeaponsPlayerTick(uint8_t ram[0x20000]);
void MmxWeaponsMarkShot(uint8_t ram[0x20000], unsigned slot);
unsigned MmxWeaponsProjectileTick(uint8_t ram[0x20000], unsigned slot, unsigned active);
void MmxWeaponsCancelShots(uint8_t ram[0x20000]);
void MmxWeaponsCollisionRom(uint8_t *rom, size_t size);
unsigned MmxWeaponsDamage(uint8_t ram[0x20000], unsigned enemy, unsigned projectile, unsigned original);
unsigned MmxWeaponsHitbox(const uint8_t ram[0x20000], unsigned enemy, unsigned projectile, unsigned original);

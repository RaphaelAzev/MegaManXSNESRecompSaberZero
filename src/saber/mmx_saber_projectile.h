#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
  MMX_SABER_PROJECTILE_SLOT_BYTES = 64,
  MMX_SABER_PROJECTILE_SLOT_FIRST = 0x1228,
  MMX_SABER_PROJECTILE_SLOT_END = 0x1428,
  MMX_SABER_PROJECTILE_SLOT_COUNT = 8,
  MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY = 0x5700,
  MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY_MASK = 0xff00,
  MMX_SABER_PROJECTILE_COLLISION_POINTER = 0xffa0,
  MMX_SABER_PROJECTILE_MAX_VIEW_MARGIN = 272,
  MMX_SABER_PROJECTILE_STAGE = 0x1f7a,
  MMX_SABER_PROJECTILE_CAMERA = 0x1e4d,
  MMX_SABER_PROJECTILE_COUNT = 0x0bdd
};

typedef enum MmxSaberProjectileKind {
  SABER_PROJECTILE_TORNADO,
  SABER_PROJECTILE_ICE_CLONE,
  SABER_PROJECTILE_THUNDER_ORB,
  SABER_PROJECTILE_CUTTER,
  SABER_PROJECTILE_KIND_COUNT
} MmxSaberProjectileKind;

typedef struct MmxSaberProjectileSpawnInfo {
  MmxSaberProjectileKind kind;
  int32_t x, y;
  int16_t vx, vy;
  uint16_t lifetime;
  uint8_t facing;
  uint8_t native_weapon_class;
  uint8_t priority;
  uint8_t damage_normal, damage_boss;
  bool pass_through;
  uint16_t hit_mask;
  uint8_t fade_level;
  uint8_t animation_tick;
} MmxSaberProjectileSpawnInfo;

typedef struct MmxSaberProjectile {
  bool active;
  uint16_t slot;
  MmxSaberProjectileKind kind;
  int32_t x, y;
  int16_t vx, vy;
  uint16_t age, lifetime;
  uint8_t facing;
  uint8_t native_weapon_class;
  uint8_t priority;
  uint8_t damage_normal, damage_boss;
  bool pass_through;
  uint16_t hit_mask;
  bool hit_any;
  uint8_t fade_level;
  uint8_t animation_tick;
} MmxSaberProjectile;

typedef struct MmxSaberProjectileLiveInfo {
  MmxSaberProjectileKind kind;
  int32_t world_x, world_y;
  bool facing_left;
  uint16_t age;
  uint8_t animation_tick;
  uint8_t fade_level;
} MmxSaberProjectileLiveInfo;

void MmxSaberProjectileReset(uint8_t *ram);
bool MmxSaberProjectileSpawn(uint8_t *ram,
                             const MmxSaberProjectileSpawnInfo *info,
                             unsigned *index_out);
bool MmxSaberProjectileOwns(const uint8_t *ram, unsigned slot);
unsigned MmxSaberProjectileFreeCapacity(const uint8_t *ram);
void MmxSaberProjectileTick(uint8_t *ram);
unsigned MmxSaberProjectileLive(MmxSaberProjectileLiveInfo *out,
                                unsigned max);
unsigned MmxSaberProjectileDamage(uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value);
unsigned MmxSaberProjectileWeakDamage(uint8_t *ram, unsigned enemy,
                                      unsigned slot, unsigned value);
unsigned MmxSaberProjectileHitbox(const uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value);
void MmxSaberProjectileRetire(uint8_t *ram, unsigned index);
void MmxSaberProjectileRetireAll(uint8_t *ram);
const MmxSaberProjectile *MmxSaberProjectileGet(unsigned index);

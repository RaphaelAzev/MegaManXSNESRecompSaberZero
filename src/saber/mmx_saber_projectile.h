#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum {
  MMX_SABER_PROJECTILE_SLOT_BYTES = 64,
  MMX_SABER_PROJECTILE_SLOT_FIRST = 0x1228,
  MMX_SABER_PROJECTILE_SLOT_END = 0x1428,
  MMX_SABER_PROJECTILE_SLOT_COUNT = 8,
  MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY = 0x5700,
  MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY_MASK = 0xff00,
  /* $FFB0..$FFD7 hold Zero's Saber bounds, so projectile slots skip that range. */
  MMX_SABER_PROJECTILE_COLLISION_POINTER_BASE = 0xffa4,
  MMX_SABER_PROJECTILE_COLLISION_POINTER_TAIL = 0xffc4,
  MMX_SABER_PROJECTILE_COLLISION_RECORD_BYTES = 4,
  /* Kept as aliases for callers that named this storage after tornadoes. */
  MMX_SABER_PROJECTILE_TORNADO_COLLISION_POINTER =
      MMX_SABER_PROJECTILE_COLLISION_POINTER_BASE,
  MMX_SABER_PROJECTILE_TORNADO_COLLISION_POINTER_TAIL =
      MMX_SABER_PROJECTILE_COLLISION_POINTER_TAIL,
  MMX_SABER_PROJECTILE_TORNADO_COLLISION_ROM_OFFSET = 0x37fa4,
  MMX_SABER_PROJECTILE_TORNADO_COLLISION_RECORD_BYTES =
      MMX_SABER_PROJECTILE_COLLISION_RECORD_BYTES,
  MMX_SABER_PROJECTILE_MAX_VIEW_MARGIN = 272,
  MMX_SABER_PROJECTILE_STAGE = 0x1f7a,
  MMX_SABER_PROJECTILE_CAMERA = 0x1e4d,
  MMX_SABER_PROJECTILE_COUNT = 0x0bdd,
  MMX_SABER_PROJECTILE_PLAYER_Y = 0x0bb0,
  /* F1: RAM $0BB0 is canvas row 64; Zero frames end at row 88, so drawn
   * feet/floor is 24 pixels below Zero y. */
  MMX_SABER_ZERO_FEET_BELOW_Y = 24,
  /* boomer() ground mock centre is Zero x + 30. */
  MMX_SABER_CUTTER_FRONT_X = 30,
  /* boomer() air step-3 blade bbox right edge is 108 on a 128 canvas;
   * Zero origin is x=64, so the front offset is 108 - 64. */
  MMX_SABER_AIR_CUTTER_FRONT_X = 108 - 64,
  /* Slash-2 step-3 blade centre is canvas y=70.5; F1 maps it to Zero y+7. */
  MMX_SABER_CUTTER_GROUND_Y_OFFSET = 7,
  MMX_SABER_CUTTER_LIFETIME = 60,
  /* Storm uppercut step 2 arc front edge is x=51 from Zero; column centre
   * stays six pixels behind that saber tip. */
  MMX_SABER_TORNADO_FRONT_X = 45,
  /* The growing column reaches the take-off arc top: 57 px above drawn feet. */
  MMX_SABER_TORNADO_TOP_ABOVE_FEET = 57,
  MMX_SABER_TORNADO_HEIGHT = 120,
  MMX_SABER_TORNADO_GROWTH_TICKS = 10,
  MMX_SABER_TORNADO_RISE_VY = -7 * 256,
  MMX_SABER_ICE_CLONE_FRONT_X = 58,
  MMX_SABER_ICE_CLONE_FADE_START_AGE = 12,
  MMX_SABER_ICE_CLONE_FADE_STEP_TICKS = 3,
  MMX_SABER_ICE_CLONE_FADE_ACTIVE_LEVEL = 12,
  MMX_SABER_ICE_CLONE_FADE_DONE_LEVEL = 16,
  MMX_SABER_ICE_CLONE_LIFETIME = 21
};

unsigned MmxSaberProjectileCollisionPointer(unsigned index);
size_t MmxSaberProjectileCollisionOffset(unsigned index);
unsigned MmxSaberProjectileTornadoCollisionPointer(unsigned index);
size_t MmxSaberProjectileTornadoCollisionOffset(unsigned index);

typedef enum MmxSaberProjectileKind {
  SABER_PROJECTILE_TORNADO,
  SABER_PROJECTILE_ICE_CLONE,
  SABER_PROJECTILE_THUNDER_ORB,
  SABER_PROJECTILE_CUTTER,
  SABER_PROJECTILE_CUTTER_AIR,
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
  /* True only while Zero is still in the uppercut growth phase. */
  bool tornado_uppercut_live;
  uint16_t hit_mask;
  uint8_t fade_level;
  uint8_t animation_tick;
} MmxSaberProjectileSpawnInfo;

typedef struct MmxSaberProjectile {
  bool active;
  uint16_t slot;
  MmxSaberProjectileKind kind;
  int32_t x, y;
  int32_t ice_clone_origin_x;
  int32_t tornado_floor_y;
  int16_t vx, vy;
  uint16_t age, lifetime;
  uint8_t facing;
  uint8_t native_weapon_class;
  uint8_t priority;
  uint8_t damage_normal, damage_boss;
  bool pass_through;
  bool tornado_uppercut_live;
  /* Keeps age-based rise active after the uppercut owner exits. */
  bool tornado_uppercut_motion;
  uint16_t hit_mask;
  bool hit_any;
  uint8_t fade_level;
  uint8_t fade_start_age;
  uint8_t animation_tick;
} MmxSaberProjectile;

typedef struct MmxSaberProjectileLiveInfo {
  MmxSaberProjectileKind kind;
  int32_t world_x, world_y;
  bool facing_left;
  uint16_t age;
  uint8_t animation_tick;
  uint8_t fade_level;
  uint16_t visible_height;
} MmxSaberProjectileLiveInfo;

void MmxSaberProjectileReset(uint8_t *ram);
void MmxSaberProjectileCollisionRom(uint8_t *rom, size_t size);
bool MmxSaberProjectileSpawn(uint8_t *ram,
                             const MmxSaberProjectileSpawnInfo *info,
                             unsigned *index_out);
bool MmxSaberProjectileOwns(const uint8_t *ram, unsigned slot);
unsigned MmxSaberProjectileFreeCapacity(const uint8_t *ram);
void MmxSaberProjectileTick(uint8_t *ram);
void MmxSaberProjectileRefreshCollision(uint8_t *ram);
unsigned MmxSaberProjectileLive(MmxSaberProjectileLiveInfo *out,
                                unsigned max);
unsigned MmxSaberProjectileVisibleHeight(const uint8_t *ram,
                                         unsigned slot);
void MmxSaberProjectileSetTornadoUppercutLive(uint8_t *ram, unsigned index,
                                              bool live);
unsigned MmxSaberProjectileDamage(uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value);
unsigned MmxSaberProjectileWeakDamage(uint8_t *ram, unsigned enemy,
                                      unsigned slot, unsigned value);
unsigned MmxSaberProjectileHitbox(const uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value);
void MmxSaberProjectileRetire(uint8_t *ram, unsigned index);
void MmxSaberProjectileRetireAll(uint8_t *ram);
const MmxSaberProjectile *MmxSaberProjectileGet(unsigned index);
/* True while any live Saber projectile of this kind owns its native slot. */
bool MmxSaberProjectileKindLive(const uint8_t *ram, MmxSaberProjectileKind kind);

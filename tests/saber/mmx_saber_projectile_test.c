#include "mmx_saber_projectile.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { TEST_RAM_BYTES = 0x20000 };

static uint8_t ram[TEST_RAM_BYTES];

static unsigned read_word(const uint8_t *p) {
  return p[0] | ((unsigned)p[1] << 8);
}

static bool check(bool condition, const char *message) {
  if (condition) return true;
  fprintf(stderr, "FAIL: %s\n", message);
  return false;
}

static MmxSaberProjectileSpawnInfo default_info(void) {
  MmxSaberProjectileSpawnInfo info = {0};
  info.kind = SABER_PROJECTILE_TORNADO;
  info.lifetime = 20;
  info.fade_level = 4;
  return info;
}

static void prepare_ram(void) {
  memset(ram, 0, sizeof(ram));
  MmxSaberProjectileReset(ram);
  ram[MMX_SABER_PROJECTILE_STAGE] = 4;
  ram[MMX_SABER_PROJECTILE_CAMERA] = 0;
  ram[MMX_SABER_PROJECTILE_CAMERA + 1] = 0;
}

static bool test_spawn_publishes_slot(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  unsigned index = 99;
  const MmxSaberProjectile *projectile;
  unsigned slot;
  unsigned tag;

  prepare_ram();
  info.kind = SABER_PROJECTILE_ICE_CLONE;
  info.x = 100 * 256;
  info.y = 50 * 256;
  info.vx = 0x0180;
  info.vy = -0x0080;
  info.facing = 0x40;
  info.native_weapon_class = 7;
  info.priority = 6;
  info.damage_normal = 5;
  info.damage_boss = 9;
  info.pass_through = true;
  info.hit_mask = 0x1234;
  info.fade_level = 16;
  info.animation_tick = 2;

  if (!check(MmxSaberProjectileFreeCapacity(ram) == 7,
             "empty pool leaves one native slot for slash"))
    return false;
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
             "valid projectile spawn succeeds"))
    return false;
  if (!check(index == 0, "first projectile uses first table entry"))
    return false;

  projectile = MmxSaberProjectileGet(index);
  slot = MMX_SABER_PROJECTILE_SLOT_FIRST;
  tag = read_word(ram + slot + 0x3e);
  if (!check(projectile && projectile->active,
             "spawn marks host entry active"))
    return false;
  if (!check(projectile->slot == slot && projectile->kind == info.kind &&
                 projectile->x == info.x && projectile->y == info.y &&
                 projectile->vx == info.vx && projectile->vy == info.vy &&
                 projectile->age == 0 && projectile->lifetime == info.lifetime,
             "spawn copies kind, Q8.8 motion and lifetime"))
    return false;
  if (!check(projectile->facing == info.facing &&
                 projectile->native_weapon_class == info.native_weapon_class &&
                 projectile->priority == info.priority &&
                 projectile->damage_normal == info.damage_normal &&
                 projectile->damage_boss == info.damage_boss &&
                 projectile->pass_through == info.pass_through &&
                 projectile->hit_mask == info.hit_mask &&
                 !projectile->hit_any &&
                 projectile->fade_level == info.fade_level &&
                 projectile->animation_tick == info.animation_tick,
             "spawn copies later collision and presentation fields"))
    return false;
  if (!check(MmxSaberProjectileOwns(ram, slot),
             "active entry with matching $5700 tag owns its native slot"))
    return false;
  if (!check(ram[slot] == 1 && ram[slot + 0x0a] == 7 &&
                 ram[slot + 0x11] == 0x40 &&
                 read_word(ram + slot + 5) == 100 &&
                 read_word(ram + slot + 8) == 50 &&
                 read_word(ram + slot + 0x20) ==
                     MMX_SABER_PROJECTILE_COLLISION_POINTER &&
                 ram[slot + 0x32] == 4,
             "spawn publishes native active, class, facing, position, collision and stage"))
    return false;
  return check((tag & MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY_MASK) ==
                   MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY &&
                   (tag & 0xff) != 0,
               "spawn publishes nonzero $5700 family tag");
}

static bool test_motion(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  unsigned index;
  const MmxSaberProjectile *projectile;
  unsigned slot;

  prepare_ram();
  info.x = 100 * 256;
  info.y = 50 * 256;
  info.vx = 0x0100;
  info.vy = 0;
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
             "straight projectile spawns"))
    return false;
  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  projectile = MmxSaberProjectileGet(index);
  slot = projectile->slot;
  if (!check(projectile->x == 103 * 256 && projectile->y == 50 * 256 &&
                 projectile->age == 3,
             "straight motion advances exact Q8.8 position and age"))
    return false;
  if (!check(read_word(ram + slot + 5) == 103 &&
                 read_word(ram + slot + 8) == 50,
             "straight motion writes native world position"))
    return false;

  prepare_ram();
  info.x = 100 * 256;
  info.y = 50 * 256;
  info.vx = 0x0180;
  info.vy = -0x00c0;
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
             "fractional diagonal projectile spawns"))
    return false;
  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  projectile = MmxSaberProjectileGet(index);
  slot = projectile->slot;
  if (!check(projectile->x == 100 * 256 + 3 * 0x0180 &&
                 projectile->y == 50 * 256 - 3 * 0x00c0 &&
                 projectile->age == 3,
             "diagonal motion adds both fractional velocities exactly"))
    return false;
  return check(ram[slot + 4] == 0x80 && ram[slot + 7] == 0xc0 &&
                   read_word(ram + slot + 5) == 104 &&
                   read_word(ram + slot + 8) == 47,
               "diagonal motion writes converted native world position");
}

static bool test_lifetime_retire(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  unsigned index;
  unsigned slot;

  prepare_ram();
  info.lifetime = 3;
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
             "short-lived projectile spawns"))
    return false;
  slot = MmxSaberProjectileGet(index)->slot;
  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  if (!check(MmxSaberProjectileGet(index)->active && ram[slot] == 1,
             "projectile stays live before lifetime"))
    return false;
  MmxSaberProjectileTick(ram);
  if (!check(!MmxSaberProjectileGet(index)->active && ram[slot] == 0,
             "lifetime retires host entry and native slot"))
    return false;
  return check(MmxSaberProjectileFreeCapacity(ram) == 7,
               "lifetime retirement restores spawn capacity");
}

static bool test_independent_projectiles(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  int32_t expected_x[7];
  int32_t expected_y[7];
  unsigned indices[7];

  prepare_ram();
  for (unsigned i = 0; i < 7; ++i) {
    info.x = (80 + (int)i) * 256;
    info.y = 40 * 256;
    info.vx = (int16_t)(0x0080 + i * 0x0040);
    info.vy = (int16_t)(i & 1 ? -0x0080 : 0x0100);
    if (!check(MmxSaberProjectileSpawn(ram, &info, &indices[i]),
               "each of seven projectiles spawns independently"))
      return false;
    expected_x[i] = info.x + 4 * info.vx;
    expected_y[i] = info.y + 4 * info.vy;
  }
  if (!check(MmxSaberProjectileFreeCapacity(ram) == 0,
             "seven live projectiles consume capacity but preserve slash slot"))
    return false;
  for (unsigned i = 0; i < 4; ++i) MmxSaberProjectileTick(ram);
  for (unsigned i = 0; i < 7; ++i) {
    const MmxSaberProjectile *projectile =
        MmxSaberProjectileGet(indices[i]);
    if (!check(projectile->active && projectile->x == expected_x[i] &&
                   projectile->y == expected_y[i] && projectile->age == 4,
               "seven projectiles retain independent positions and ages"))
      return false;
  }
  return true;
}

static bool test_spawn_failure_is_clean(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  MmxSaberProjectile before[MMX_SABER_PROJECTILE_SLOT_COUNT];
  uint8_t before_ram[TEST_RAM_BYTES];
  unsigned index;

  prepare_ram();
  for (unsigned i = 0; i < 7; ++i) {
    if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
               "pool setup spawn succeeds"))
      return false;
  }
  memcpy(before_ram, ram, sizeof(ram));
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    memcpy(&before[i], MmxSaberProjectileGet(i), sizeof(before[i]));
  if (!check(!MmxSaberProjectileSpawn(ram, &info, &index),
             "spawn fails when only slash-reserved slot remains"))
    return false;
  if (!check(memcmp(ram, before_ram, sizeof(ram)) == 0,
             "failed spawn leaves native RAM unchanged"))
    return false;
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    if (!check(memcmp(&before[i], MmxSaberProjectileGet(i), sizeof(before[i])) == 0,
               "failed spawn leaves host table unchanged"))
      return false;
  return true;
}

static bool test_retire_all_isolated(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  uint8_t wave_before[MMX_SABER_PROJECTILE_SLOT_BYTES];
  uint8_t native_before[MMX_SABER_PROJECTILE_SLOT_BYTES];
  unsigned index;
  const unsigned wave_slot = MMX_SABER_PROJECTILE_SLOT_FIRST + 2 * 64;
  const unsigned native_slot = MMX_SABER_PROJECTILE_SLOT_FIRST + 3 * 64;

  prepare_ram();
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index) &&
                 MmxSaberProjectileSpawn(ram, &info, &index),
             "owned slots spawn before isolated retire-all"))
    return false;
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_BYTES; ++i) {
    ram[wave_slot + i] = (uint8_t)(0xa0 + i);
    ram[native_slot + i] = (uint8_t)(0xb0 + i);
  }
  ram[wave_slot] = 1;
  ram[native_slot] = 1;
  ram[wave_slot + 0x3e] = 1;
  ram[wave_slot + 0x3f] = 0x56;
  ram[native_slot + 0x3e] = 0x53;
  ram[native_slot + 0x3f] = 0x5a;
  memcpy(wave_before, ram + wave_slot, sizeof(wave_before));
  memcpy(native_before, ram + native_slot, sizeof(native_before));

  MmxSaberProjectileRetireAll(ram);
  if (!check(ram[MMX_SABER_PROJECTILE_SLOT_FIRST] == 0 &&
                 ram[MMX_SABER_PROJECTILE_SLOT_FIRST + 64] == 0,
             "retire-all clears every owned slot"))
    return false;
  if (!check(memcmp(ram + wave_slot, wave_before, sizeof(wave_before)) == 0,
             "retire-all preserves wave-tagged slot"))
    return false;
  if (!check(memcmp(ram + native_slot, native_before, sizeof(native_before)) == 0,
             "retire-all preserves native slot"))
    return false;
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    if (!check(!MmxSaberProjectileGet(i)->active,
               "retire-all clears every host entry"))
      return false;
  return check(ram[MMX_SABER_PROJECTILE_COUNT] == 0,
               "retire-all decrements native count only for owned slots");
}

static bool test_stage_and_view_retire(void) {
  MmxSaberProjectileSpawnInfo info = default_info();
  unsigned index;
  unsigned slot;

  prepare_ram();
  info.x = 0;
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
             "stage test projectile spawns"))
    return false;
  slot = MmxSaberProjectileGet(index)->slot;
  ram[MMX_SABER_PROJECTILE_STAGE] = 5;
  MmxSaberProjectileTick(ram);
  if (!check(!MmxSaberProjectileGet(index)->active && ram[slot] == 0,
             "stage change retires projectile"))
    return false;

  prepare_ram();
  info.x = 600 * 256;
  if (!check(MmxSaberProjectileSpawn(ram, &info, &index),
             "offscreen test projectile spawns"))
    return false;
  slot = MmxSaberProjectileGet(index)->slot;
  MmxSaberProjectileTick(ram);
  return check(!MmxSaberProjectileGet(index)->active && ram[slot] == 0,
               "wave view margin retires offscreen projectile");
}

int main(void) {
  if (!test_spawn_publishes_slot()) return 1;
  if (!test_motion()) return 1;
  if (!test_lifetime_retire()) return 1;
  if (!test_independent_projectiles()) return 1;
  if (!test_spawn_failure_is_clean()) return 1;
  if (!test_retire_all_isolated()) return 1;
  if (!test_stage_and_view_retire()) return 1;
  return 0;
}

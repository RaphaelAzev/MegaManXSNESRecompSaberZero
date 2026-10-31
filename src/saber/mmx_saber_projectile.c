#include "mmx_saber_projectile.h"

#include <stddef.h>
#include <string.h>

static MmxSaberProjectile projectiles[MMX_SABER_PROJECTILE_SLOT_COUNT];
static uint8_t *runtime_ram;
static bool table_initialized;

static unsigned word(const uint8_t *p) {
  return p[0] | ((unsigned)p[1] << 8);
}

static void putword(uint8_t *p, unsigned value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
}

static unsigned slot_for_index(unsigned index) {
  return MMX_SABER_PROJECTILE_SLOT_FIRST +
      index * MMX_SABER_PROJECTILE_SLOT_BYTES;
}

static bool slot_valid(unsigned slot) {
  return slot >= MMX_SABER_PROJECTILE_SLOT_FIRST &&
      slot < MMX_SABER_PROJECTILE_SLOT_END &&
      (slot & (MMX_SABER_PROJECTILE_SLOT_BYTES - 1)) == 0x28;
}

static bool tag_family(unsigned tag) {
  return (tag & MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY_MASK) ==
      MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY;
}

static bool owns_slot(const uint8_t *ram, unsigned slot) {
  return ram && slot_valid(slot) && tag_family(word(ram + slot + 0x3e));
}

static bool free_slot(const uint8_t *ram, unsigned slot) {
  return ram && slot_valid(slot) && word(ram + slot) == 0;
}

static uint8_t *target_ram(uint8_t *ram) {
  if (ram) runtime_ram = ram;
  return ram ? ram : runtime_ram;
}

static void initialize_table(void) {
  if (table_initialized) return;
  memset(projectiles, 0, sizeof(projectiles));
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    projectiles[i].slot = (uint16_t)slot_for_index(i);
  table_initialized = true;
}

static void clear_entry(unsigned index) {
  memset(&projectiles[index], 0, sizeof(projectiles[index]));
  projectiles[index].slot = (uint16_t)slot_for_index(index);
}

static void retire_slot(uint8_t *ram, unsigned slot) {
  bool counted;
  if (!owns_slot(ram, slot)) return;
  counted = ram[slot] != 0;
  memset(ram + slot, 0, MMX_SABER_PROJECTILE_SLOT_BYTES);
  if (counted && ram[MMX_SABER_PROJECTILE_COUNT])
    --ram[MMX_SABER_PROJECTILE_COUNT];
}

static int16_t native_coordinate(int32_t fixed) {
  const int64_t value = fixed;
  if (value < 0) return (int16_t)-((-value + 255) / 256);
  return (int16_t)(value / 256);
}

static void write_position(uint8_t *ram, const MmxSaberProjectile *projectile) {
  const unsigned slot = projectile->slot;
  ram[slot + 4] = (uint8_t)projectile->x;
  putword(ram + slot + 5, (uint16_t)native_coordinate(projectile->x));
  ram[slot + 7] = (uint8_t)projectile->y;
  putword(ram + slot + 8, (uint16_t)native_coordinate(projectile->y));
  putword(ram + slot + 0x20, MMX_SABER_PROJECTILE_COLLISION_POINTER);
}

static bool offscreen(const uint8_t *ram, unsigned slot) {
  const int x = (int16_t)word(ram + slot + 5);
  const int camera = (int16_t)word(ram + MMX_SABER_PROJECTILE_CAMERA);
  return x < camera - MMX_SABER_PROJECTILE_MAX_VIEW_MARGIN ||
      x > camera + 256 + MMX_SABER_PROJECTILE_MAX_VIEW_MARGIN;
}

void MmxSaberProjectileReset(uint8_t *ram) {
  MmxSaberProjectileRetireAll(ram);
}

bool MmxSaberProjectileSpawn(uint8_t *ram,
                             const MmxSaberProjectileSpawnInfo *info,
                             unsigned *index_out) {
  unsigned first = MMX_SABER_PROJECTILE_SLOT_COUNT;
  unsigned free_count = 0;

  if (!ram || !info || info->kind < SABER_PROJECTILE_TORNADO ||
      info->kind >= SABER_PROJECTILE_KIND_COUNT ||
      !info->lifetime || info->fade_level > 16)
    return false;
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i) {
    if (projectiles[i].active || !free_slot(ram, slot_for_index(i))) continue;
    if (first == MMX_SABER_PROJECTILE_SLOT_COUNT) first = i;
    ++free_count;
  }
  /* Keep one free native slot for a slash, matching wave reservation. */
  if (free_count < 2) return false;

  initialize_table();
  runtime_ram = ram;
  MmxSaberProjectile *projectile = &projectiles[first];
  const unsigned slot = slot_for_index(first);
  *projectile = (MmxSaberProjectile){
      true,
      (uint16_t)slot,
      info->kind,
      info->x,
      info->y,
      info->vx,
      info->vy,
      0,
      info->lifetime,
      info->facing,
      info->native_weapon_class,
      info->damage_normal,
      info->damage_boss,
      info->pass_through,
      info->hit_mask,
      info->fade_level,
      info->animation_tick};

  memset(ram + slot, 0, MMX_SABER_PROJECTILE_SLOT_BYTES);
  ram[slot] = 1;
  ram[slot + 1] = 2;
  ram[slot + 0x0a] = projectile->native_weapon_class;
  ram[slot + 0x11] = projectile->facing & 0x40;
  write_position(ram, projectile);
  ram[slot + 0x32] = ram[MMX_SABER_PROJECTILE_STAGE];
  putword(ram + slot + 0x3e,
          MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY | (first + 1));
  ++ram[MMX_SABER_PROJECTILE_COUNT];
  if (index_out) *index_out = first;
  return true;
}

unsigned MmxSaberProjectileFreeCapacity(const uint8_t *ram) {
  unsigned free_count = 0;
  if (!ram) ram = runtime_ram;
  if (!ram) return 0;
  initialize_table();
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    if (!projectiles[i].active && free_slot(ram, slot_for_index(i)))
      ++free_count;
  return free_count >= 2 ? free_count - 1 : 0;
}

void MmxSaberProjectileTick(uint8_t *ram) {
  uint8_t *target = target_ram(ram);
  if (!target) return;
  initialize_table();
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i) {
    MmxSaberProjectile *projectile = &projectiles[i];
    const unsigned slot = projectile->slot;
    if (!projectile->active) continue;
    if (!owns_slot(target, slot) || !target[slot]) {
      clear_entry(i);
      continue;
    }
    if (target[slot + 0x32] != target[MMX_SABER_PROJECTILE_STAGE]) {
      retire_slot(target, slot);
      clear_entry(i);
      continue;
    }
    projectile->x = (int32_t)((int64_t)projectile->x + projectile->vx);
    projectile->y = (int32_t)((int64_t)projectile->y + projectile->vy);
    ++projectile->age;
    ++projectile->animation_tick;
    write_position(target, projectile);
    if (projectile->age >= projectile->lifetime || offscreen(target, slot)) {
      retire_slot(target, slot);
      clear_entry(i);
    }
  }
}

void MmxSaberProjectileRetire(uint8_t *ram, unsigned index) {
  uint8_t *target = target_ram(ram);
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT) return;
  initialize_table();
  if (target && projectiles[index].active)
    retire_slot(target, projectiles[index].slot);
  clear_entry(index);
}

void MmxSaberProjectileRetireAll(uint8_t *ram) {
  uint8_t *target = target_ram(ram);
  initialize_table();
  if (target)
    for (unsigned slot = MMX_SABER_PROJECTILE_SLOT_FIRST;
         slot < MMX_SABER_PROJECTILE_SLOT_END;
         slot += MMX_SABER_PROJECTILE_SLOT_BYTES)
      retire_slot(target, slot);
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    clear_entry(i);
}

const MmxSaberProjectile *MmxSaberProjectileGet(unsigned index) {
  initialize_table();
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT) return NULL;
  return &projectiles[index];
}

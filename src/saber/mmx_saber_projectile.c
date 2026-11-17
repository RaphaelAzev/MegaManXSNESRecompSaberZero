#include "mmx_saber_projectile.h"

#include <stddef.h>
#include <string.h>

#include "../mmx_wide_policy.h"

static MmxSaberProjectile projectiles[MMX_SABER_PROJECTILE_SLOT_COUNT];
static uint8_t *runtime_ram;
static bool table_initialized;
static uint8_t *collision_rom;
static size_t collision_rom_size;

typedef struct MmxSaberTornadoCollision {
  bool installed;
  uint8_t bytes[MMX_SABER_PROJECTILE_TORNADO_COLLISION_RECORD_BYTES];
} MmxSaberTornadoCollision;

static MmxSaberTornadoCollision tornado_collision[
    MMX_SABER_PROJECTILE_SLOT_COUNT];

typedef struct MmxSaberProjectileBounds {
  uint8_t half_width;
  uint8_t half_height;
} MmxSaberProjectileBounds;

static const MmxSaberProjectileBounds kProjectileHitboxes[
    SABER_PROJECTILE_KIND_COUNT] = {
  [SABER_PROJECTILE_TORNADO] = {10, 60},
  [SABER_PROJECTILE_ICE_CLONE] = {16, 16},
  [SABER_PROJECTILE_THUNDER_ORB] = {8, 8},
  [SABER_PROJECTILE_CUTTER] = {16, 6},
  [SABER_PROJECTILE_CUTTER_AIR] = {13, 12},
};

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

static unsigned index_for_slot(unsigned slot) {
  if (!slot_valid(slot)) return MMX_SABER_PROJECTILE_SLOT_COUNT;
  return (slot - MMX_SABER_PROJECTILE_SLOT_FIRST) /
      MMX_SABER_PROJECTILE_SLOT_BYTES;
}

static void clear_tornado_collision(unsigned index);

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

static MmxSaberProjectile *owned_entry(const uint8_t *ram, unsigned slot) {
  const unsigned index = index_for_slot(slot);
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT) return NULL;
  initialize_table();
  if (!projectiles[index].active || projectiles[index].slot != slot ||
      !owns_slot(ram, slot))
    return NULL;
  return &projectiles[index];
}

static bool enemy_slot_valid(unsigned enemy) {
  return enemy >= 0xe68 && enemy < 0x1228 &&
      (enemy & (MMX_SABER_PROJECTILE_SLOT_BYTES - 1)) == 0x28;
}

static unsigned enemy_bit(unsigned enemy) {
  if (!enemy_slot_valid(enemy)) return 0;
  return 1u << ((enemy - 0xe68) / MMX_SABER_PROJECTILE_SLOT_BYTES);
}

static const MmxSaberProjectileBounds *hitbox_for_kind(
    MmxSaberProjectileKind kind) {
  if (kind < SABER_PROJECTILE_TORNADO ||
      kind >= SABER_PROJECTILE_KIND_COUNT)
    return NULL;
  return &kProjectileHitboxes[kind];
}

static void clear_entry(unsigned index) {
  clear_tornado_collision(index);
  memset(&projectiles[index], 0, sizeof(projectiles[index]));
  projectiles[index].slot = (uint16_t)slot_for_index(index);
}

static void retire_slot(uint8_t *ram, unsigned slot) {
  bool counted;
  const unsigned index = index_for_slot(slot);
  if (!owns_slot(ram, slot)) return;
  if (index < MMX_SABER_PROJECTILE_SLOT_COUNT)
    clear_tornado_collision(index);
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

unsigned MmxSaberProjectileTornadoCollisionPointer(unsigned index) {
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT) return 0;
  return index < 3 ?
      MMX_SABER_PROJECTILE_TORNADO_COLLISION_POINTER + index * 4 :
      MMX_SABER_PROJECTILE_TORNADO_COLLISION_POINTER_TAIL +
          (index - 3) * 4;
}

size_t MmxSaberProjectileTornadoCollisionOffset(unsigned index) {
  return 0x30000u +
      (MmxSaberProjectileTornadoCollisionPointer(index) & 0x7fffu);
}

static uint8_t *tornado_collision_record(unsigned index) {
  size_t offset;
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT || !collision_rom ||
      !MmxSaberProjectileTornadoCollisionPointer(index) ||
      collision_rom_size < 0x30000)
    return NULL;
  offset = MmxSaberProjectileTornadoCollisionOffset(index);
  if (offset > collision_rom_size ||
      collision_rom_size - offset <
          MMX_SABER_PROJECTILE_TORNADO_COLLISION_RECORD_BYTES)
    return NULL;
  return collision_rom + offset;
}

static bool collision_record_empty(const uint8_t *record) {
  if (!record) return false;
  for (unsigned i = 0;
       i < MMX_SABER_PROJECTILE_TORNADO_COLLISION_RECORD_BYTES; ++i)
    if (record[i] != 0xff) return false;
  return true;
}

static void clear_tornado_collision(unsigned index) {
  MmxSaberTornadoCollision *owner;
  uint8_t *record;
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT) return;
  owner = &tornado_collision[index];
  if (!owner->installed) return;
  record = tornado_collision_record(index);
  if (record && !memcmp(record, owner->bytes, sizeof(owner->bytes)))
    memset(record, 0xff, sizeof(owner->bytes));
  owner->installed = false;
  memset(owner->bytes, 0, sizeof(owner->bytes));
}

static unsigned tornado_visible_height(const uint8_t *ram,
                                       const MmxSaberProjectile *projectile) {
  int height;
  if (!projectile || projectile->kind != SABER_PROJECTILE_TORNADO)
    return 0;
  if (projectile->age >= MMX_SABER_TORNADO_GROWTH_TICKS ||
      !projectile->tornado_uppercut_live || !ram)
    return MMX_SABER_TORNADO_HEIGHT;
  height = (int)native_coordinate(projectile->tornado_floor_y) -
      ((int16_t)word(ram + MMX_SABER_PROJECTILE_PLAYER_Y) -
       MMX_SABER_TORNADO_TOP_ABOVE_ZERO);
  if (height < 16) height = 16;
  if (height > MMX_SABER_TORNADO_HEIGHT)
    height = MMX_SABER_TORNADO_HEIGHT;
  return (unsigned)height;
}

static void update_tornado_collision(const uint8_t *ram,
                                     const MmxSaberProjectile *projectile) {
  const unsigned index = projectile ? index_for_slot(projectile->slot) :
      MMX_SABER_PROJECTILE_SLOT_COUNT;
  const MmxSaberProjectileBounds *hitbox = projectile ?
      hitbox_for_kind(projectile->kind) : NULL;
  MmxSaberTornadoCollision *owner;
  uint8_t *record;
  uint8_t next[MMX_SABER_PROJECTILE_TORNADO_COLLISION_RECORD_BYTES];
  unsigned half_height;

  if (!projectile || projectile->kind != SABER_PROJECTILE_TORNADO || !hitbox ||
      index >= MMX_SABER_PROJECTILE_SLOT_COUNT)
    return;
  record = tornado_collision_record(index);
  if (!record) return;
  owner = &tornado_collision[index];
  if (owner->installed) {
    if (memcmp(record, owner->bytes, sizeof(owner->bytes)) != 0) {
      owner->installed = false;
      memset(owner->bytes, 0, sizeof(owner->bytes));
      return;
    }
  } else if (!collision_record_empty(record)) {
    return;
  }

  half_height = (tornado_visible_height(ram, projectile) + 1) / 2;
  next[0] = 0;
  next[1] = (uint8_t)(-(int)half_height);
  next[2] = hitbox->half_width;
  next[3] = (uint8_t)half_height;
  memcpy(record, next, sizeof(next));
  memcpy(owner->bytes, next, sizeof(owner->bytes));
  owner->installed = true;
}

static void write_position(uint8_t *ram, const MmxSaberProjectile *projectile) {
  const unsigned slot = projectile->slot;
  const unsigned index = index_for_slot(slot);
  const unsigned collision_pointer = projectile->kind == SABER_PROJECTILE_TORNADO ?
      MmxSaberProjectileTornadoCollisionPointer(index) :
      MMX_SABER_PROJECTILE_COLLISION_POINTER;
  ram[slot + 4] = (uint8_t)projectile->x;
  putword(ram + slot + 5, (uint16_t)native_coordinate(projectile->x));
  ram[slot + 7] = (uint8_t)projectile->y;
  putword(ram + slot + 8, (uint16_t)native_coordinate(projectile->y));
  putword(ram + slot + 0x20, collision_pointer);
}

static bool offscreen(const uint8_t *ram, unsigned slot) {
  const int x = (int16_t)word(ram + slot + 5);
  const int camera = (int16_t)word(ram + MMX_SABER_PROJECTILE_CAMERA);
  return x < camera - MMX_SABER_PROJECTILE_MAX_VIEW_MARGIN ||
      x > camera + 256 + MMX_SABER_PROJECTILE_MAX_VIEW_MARGIN;
}

void MmxSaberProjectileCollisionRom(uint8_t *rom, size_t size) {
  if (collision_rom != rom || collision_rom_size != size) {
    for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
      clear_tornado_collision(i);
  }
  collision_rom = rom;
  collision_rom_size = size;
  initialize_table();
  if (!rom) return;
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i)
    if (projectiles[i].active)
      update_tornado_collision(runtime_ram, &projectiles[i]);
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
      .active = true,
      .slot = (uint16_t)slot,
      .kind = info->kind,
      .x = info->x,
      .y = info->y,
      .tornado_floor_y = info->y,
      .vx = info->vx,
      .vy = info->vy,
      .age = 0,
      .lifetime = info->lifetime,
      .facing = info->facing,
      .native_weapon_class = info->native_weapon_class,
      .priority = info->priority,
      .damage_normal = info->damage_normal,
      .damage_boss = info->damage_boss,
      .pass_through = info->pass_through,
      .tornado_uppercut_live = info->kind == SABER_PROJECTILE_TORNADO &&
          info->tornado_uppercut_live,
      .tornado_uppercut_motion = info->kind == SABER_PROJECTILE_TORNADO &&
          info->tornado_uppercut_live,
      .hit_mask = info->hit_mask,
      .hit_any = false,
      .fade_level = info->fade_level,
      .animation_tick = info->animation_tick};

  memset(ram + slot, 0, MMX_SABER_PROJECTILE_SLOT_BYTES);
  ram[slot] = 1;
  ram[slot + 1] = 2;
  ram[slot + 0x0a] = projectile->kind == SABER_PROJECTILE_TORNADO ?
      projectile->native_weapon_class : 3;
  ram[slot + 0x11] = projectile->facing & 0x40;
  write_position(ram, projectile);
  ram[slot + 0x32] = ram[MMX_SABER_PROJECTILE_STAGE];
  putword(ram + slot + 0x3e,
          MMX_SABER_PROJECTILE_RUNTIME_TAG_FAMILY | (first + 1));
  ++ram[MMX_SABER_PROJECTILE_COUNT];
  update_tornado_collision(ram, projectile);
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

bool MmxSaberProjectileOwns(const uint8_t *ram, unsigned slot) {
  return owned_entry(ram, slot) != NULL;
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
    if (projectile->kind == SABER_PROJECTILE_TORNADO &&
        projectile->tornado_uppercut_motion) {
      projectile->vy = projectile->age >= MMX_SABER_TORNADO_GROWTH_TICKS ?
          MMX_SABER_TORNADO_RISE_VY : 0;
    }
    projectile->x = (int32_t)((int64_t)projectile->x + projectile->vx);
    projectile->y = (int32_t)((int64_t)projectile->y + projectile->vy);
    ++projectile->age;
    if (projectile->kind == SABER_PROJECTILE_TORNADO &&
        projectile->tornado_uppercut_motion &&
        projectile->age == MMX_SABER_TORNADO_GROWTH_TICKS)
      projectile->vy = MMX_SABER_TORNADO_RISE_VY;
    ++projectile->animation_tick;
    write_position(target, projectile);
    update_tornado_collision(target, projectile);
    if (projectile->age >= projectile->lifetime || offscreen(target, slot)) {
      retire_slot(target, slot);
      clear_entry(i);
    }
  }
}

void MmxSaberProjectileRefreshCollision(uint8_t *ram) {
  uint8_t *target = target_ram(ram);
  if (!target) return;
  initialize_table();
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT; ++i) {
    MmxSaberProjectile *projectile = &projectiles[i];
    if (!projectile->active) continue;
    if (!target[projectile->slot] || !owns_slot(target, projectile->slot)) {
      clear_entry(i);
      continue;
    }
    update_tornado_collision(target, projectile);
  }
}

unsigned MmxSaberProjectileLive(MmxSaberProjectileLiveInfo *out,
                                unsigned max) {
  unsigned count = 0;
  if (!runtime_ram || !max) return 0;
  initialize_table();
  for (unsigned i = 0; i < MMX_SABER_PROJECTILE_SLOT_COUNT && count < max;
       ++i) {
    const MmxSaberProjectile *projectile = &projectiles[i];
    if (!projectile->active || !owns_slot(runtime_ram, projectile->slot) ||
        !runtime_ram[projectile->slot])
      continue;
    if (out) {
      out[count].kind = projectile->kind;
      out[count].world_x = native_coordinate(projectile->x);
      out[count].world_y = native_coordinate(projectile->y);
      out[count].facing_left = (projectile->facing & 0x40) != 0;
      out[count].age = projectile->age;
      out[count].animation_tick = projectile->animation_tick;
      out[count].fade_level = projectile->fade_level;
      out[count].visible_height = (uint16_t)tornado_visible_height(
          runtime_ram, projectile);
    }
    ++count;
  }
  return count;
}

unsigned MmxSaberProjectileVisibleHeight(const uint8_t *ram,
                                         unsigned slot) {
  const MmxSaberProjectile *projectile = owned_entry(ram, slot);
  return tornado_visible_height(ram, projectile);
}

void MmxSaberProjectileSetTornadoUppercutLive(uint8_t *ram, unsigned index,
                                              bool live) {
  uint8_t *target = target_ram(ram);
  MmxSaberProjectile *projectile;
  if (index >= MMX_SABER_PROJECTILE_SLOT_COUNT || !target) return;
  initialize_table();
  projectile = &projectiles[index];
  if (!projectile->active || projectile->kind != SABER_PROJECTILE_TORNADO ||
      !owns_slot(target, projectile->slot))
    return;
  projectile->tornado_uppercut_live = live;
  update_tornado_collision(target, projectile);
}

static unsigned projectile_damage(uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value,
                                  bool weak_contact) {
  MmxSaberProjectile *projectile = owned_entry(ram, slot);
  const unsigned index = index_for_slot(slot);
  unsigned bit;
  unsigned damage;

  if (!projectile) return value;
  if (value & 128) {
    /* Native reflection stays authoritative. Only a non-pass-through owner
     * is retired by the reflected response, matching the wave owner. */
    if (!projectile->pass_through)
      MmxSaberProjectileRetire(ram, index);
    return value;
  }
  if (!enemy_slot_valid(enemy) || !value) return value;
  bit = enemy_bit(enemy);
  if (projectile->hit_mask & bit) return 0;
  projectile->hit_mask |= (uint16_t)bit;
  projectile->hit_any = true;
  damage = MmxWidePolicy_IsBossEncounter(ram[enemy + 0x0a]) ?
      projectile->damage_boss : projectile->damage_normal;
  /* Theme weakness: native weakness damage, never below the tuned value. */
  if (weak_contact && value > damage) damage = value;
  if (!projectile->pass_through) MmxSaberProjectileRetire(ram, index);
  return damage;
}

unsigned MmxSaberProjectileDamage(uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value) {
  return projectile_damage(ram, enemy, slot, value, false);
}

unsigned MmxSaberProjectileWeakDamage(uint8_t *ram, unsigned enemy,
                                      unsigned slot, unsigned value) {
  return projectile_damage(ram, enemy, slot, value, true);
}

unsigned MmxSaberProjectileHitbox(const uint8_t *ram, unsigned enemy,
                                  unsigned slot, unsigned value) {
  const MmxSaberProjectile *projectile = owned_entry(ram, slot);
  const MmxSaberProjectileBounds *hitbox;
  const unsigned bit = enemy_bit(enemy);

  if (!projectile || !enemy_slot_valid(enemy) || !value) return value;
  hitbox = hitbox_for_kind(projectile->kind);
  /* Tornado records are refreshed from its bottom-anchored visible height;
   * native collision supplies the broad-phase candidate and this callback
   * supplies the per-enemy hit-mask gate. */
  if (!hitbox || !hitbox->half_width || !hitbox->half_height) return 0;
  if (projectile->kind == SABER_PROJECTILE_TORNADO &&
      !tornado_visible_height(ram, projectile)) return 0;
  if (projectile->hit_mask & bit) return 0;
  return value;
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

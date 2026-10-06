#include "mmx_saber_wave_runtime.h"

#include <string.h>

#include "../mmx_zero.h"

enum {
  MMX_SABER_WAVE_SLOT_BYTES = 64,
  MMX_SABER_WAVE_SLOT_FIRST = 0x1228,
  MMX_SABER_WAVE_SLOT_END = 0x1428,
  MMX_SABER_WAVE_STAGE = 0x1f7a,
  MMX_SABER_WAVE_CAMERA = 0x1e4d,
  MMX_SABER_WAVE_COUNT = 0x0bdd
};

typedef struct MmxSaberWaveCollision {
  uint8_t *rom;
  uint8_t bytes[MMX_SABER_WAVE_COLLISION_RECORD_BYTES];
  bool installed;
  bool ready;
} MmxSaberWaveCollision;

static uint8_t *runtime_ram;
static uint8_t next_generation;
static uint8_t collision_record[MMX_SABER_WAVE_COLLISION_RECORD_BYTES];
static bool collision_record_valid;
static MmxSaberWaveCollision collision;

static unsigned word(const uint8_t *p) {
  return p[0] | ((unsigned)p[1] << 8);
}

static void putword(uint8_t *p, unsigned value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
}

static bool slot_valid(unsigned slot) {
  return slot >= MMX_SABER_WAVE_SLOT_FIRST &&
      slot < MMX_SABER_WAVE_SLOT_END &&
      (slot & (MMX_SABER_WAVE_SLOT_BYTES - 1)) == 0x28;
}

static bool tag_family(unsigned tag) {
  return (tag & MMX_SABER_WAVE_TAG_FAMILY_MASK) ==
      MMX_SABER_WAVE_TAG_FAMILY;
}

static bool tagged(const uint8_t *ram, unsigned slot) {
  return ram && slot_valid(slot) && tag_family(word(ram + slot + 0x3e));
}

static bool live(const uint8_t *ram, unsigned slot) {
  return tagged(ram, slot) && ram[slot] != 0;
}

static bool free_slot(const uint8_t *ram, unsigned slot) {
  return slot_valid(slot) && word(ram + slot) == 0;
}

static uint8_t generation_for_ram(const uint8_t *ram, uint8_t candidate) {
  for (unsigned slot = MMX_SABER_WAVE_SLOT_FIRST;
       slot < MMX_SABER_WAVE_SLOT_END; slot += MMX_SABER_WAVE_SLOT_BYTES) {
    if (word(ram + slot + 0x3e) ==
        (MMX_SABER_WAVE_TAG_FAMILY | candidate))
      return 0;
  }
  return candidate;
}

static uint8_t allocate_generation(const uint8_t *ram) {
  uint8_t candidate = next_generation;
  for (unsigned attempt = 0; attempt < 255; ++attempt) {
    if (++candidate == 0) candidate = 1;
    if (!ram || generation_for_ram(ram, candidate)) {
      next_generation = candidate;
      return candidate;
    }
  }
  return 0;
}

static uint8_t *target_ram(uint8_t *ram) {
  if (ram) runtime_ram = ram;
  return ram ? ram : runtime_ram;
}

static void retire_slot(uint8_t *ram, unsigned slot) {
  bool counted;
  if (!tagged(ram, slot)) return;
  counted = ram[slot] != 0;
  memset(ram + slot, 0, MMX_SABER_WAVE_SLOT_BYTES);
  if (counted && ram[MMX_SABER_WAVE_COUNT]) --ram[MMX_SABER_WAVE_COUNT];
}

static bool offscreen(const uint8_t *ram, unsigned slot) {
  const int x = (int16_t)word(ram + slot + 5);
  const int camera = (int16_t)word(ram + MMX_SABER_WAVE_CAMERA);
  return x < camera - MMX_SABER_WAVE_MAX_VIEW_MARGIN ||
      x > camera + 256 + MMX_SABER_WAVE_MAX_VIEW_MARGIN;
}

static void collision_reset(void) {
  static const uint8_t empty[MMX_SABER_WAVE_COLLISION_RECORD_BYTES] =
      {0xff, 0xff, 0xff, 0xff};
  if (collision.rom &&
      !memcmp(collision.rom + MMX_SABER_WAVE_COLLISION_ROM_OFFSET,
              collision.bytes, sizeof(collision.bytes)))
    memcpy(collision.rom + MMX_SABER_WAVE_COLLISION_ROM_OFFSET,
           empty, sizeof(empty));
  collision.rom = NULL;
  collision.installed = false;
  collision.ready = false;
}

bool MmxSaberWaveRuntimeReserve(uint8_t *ram, unsigned *slot) {
  unsigned first = 0;
  unsigned free_count = 0;
  uint8_t generation;
  if (slot) *slot = 0;
  if (!ram) return false;
  runtime_ram = ram;
  for (unsigned d = MMX_SABER_WAVE_SLOT_FIRST;
       d < MMX_SABER_WAVE_SLOT_END; d += MMX_SABER_WAVE_SLOT_BYTES) {
    if (!free_slot(ram, d)) continue;
    if (!first) first = d;
    ++free_count;
  }
  /* The finisher's slash and its future wave must be atomic. */
  if (free_count < 2) return false;
  generation = allocate_generation(ram);
  if (!generation) return false;
  memset(ram + first, 0, MMX_SABER_WAVE_SLOT_BYTES);
  ram[first + 1] = 0x80;
  putword(ram + first + 0x3e, MMX_SABER_WAVE_TAG_FAMILY | generation);
  ram[first + MMX_SABER_WAVE_SLOT_STAGE] = ram[MMX_SABER_WAVE_STAGE];
  if (slot) *slot = first;
  return true;
}

bool MmxSaberWaveRuntimeReleaseReservation(uint8_t *ram, unsigned slot) {
  uint8_t *target = target_ram(ram);
  if (!target || !tagged(target, slot) || target[slot]) return false;
  memset(target + slot, 0, MMX_SABER_WAVE_SLOT_BYTES);
  return true;
}

bool MmxSaberWaveRuntimePublish(uint8_t *ram, unsigned slot) {
  MmxZeroState zero;
  unsigned tag;
  uint8_t facing;
  uint8_t *target = target_ram(ram);
  if (!target || !slot_valid(slot) || !tagged(target, slot)) return false;
  if (!collision.ready) {
    MmxSaberWaveRuntimeReleaseReservation(target, slot);
    return false;
  }
  if (target[slot] || target[slot + 1] != 0x80) {
    /* A native allocator may have claimed the promised address. Preserve it
     * and make its former wave tag non-owning, exactly as the old branch did. */
    if (target[slot]) putword(target + slot + 0x3e, 0x5758);
    return false;
  }
  tag = word(target + slot + 0x3e);
  zero = MmxZeroGetState();
  facing = zero.facing & 0x40;
  memset(target + slot, 0, MMX_SABER_WAVE_SLOT_BYTES);
  target[slot] = 1;
  target[slot + 1] = 2;
  target[slot + 10] = 3;
  target[slot + 0x11] = facing;
  putword(target + slot + 5,
          (uint16_t)((int16_t)word(target + 0x0bad) +
              (facing ? MMX_SABER_WAVE_SPAWN_OFFSET_X :
                        -MMX_SABER_WAVE_SPAWN_OFFSET_X)));
  putword(target + slot + 8,
          (uint16_t)((int16_t)word(target + 0x0bb0) +
              MMX_SABER_WAVE_SPAWN_OFFSET_Y));
  putword(target + slot + 0x20, MMX_SABER_WAVE_COLLISION_POINTER);
  target[slot + MMX_SABER_WAVE_SLOT_AGE] = 0;
  target[slot + MMX_SABER_WAVE_SLOT_STATE] =
      MMX_SABER_WAVE_SLOT_STATE_TRAVEL;
  target[slot + MMX_SABER_WAVE_SLOT_PULSES] = 0;
  target[slot + MMX_SABER_WAVE_SLOT_COUNTDOWN] = 0;
  target[slot + MMX_SABER_WAVE_SLOT_STAGE] = target[MMX_SABER_WAVE_STAGE];
  /* Old src/mmx_saber.c:1701-1704 keeps the publication frame stationary. */
  target[slot + MMX_SABER_WAVE_SLOT_BIRTH] = 1;
  putword(target + slot + 0x3e, tag);
  ++target[MMX_SABER_WAVE_COUNT];
  return true;
}

void MmxSaberWaveRuntimeObserveStage(uint8_t *ram) {
  uint8_t *target = target_ram(ram);
  if (!target) return;
  for (unsigned slot = MMX_SABER_WAVE_SLOT_FIRST;
       slot < MMX_SABER_WAVE_SLOT_END; slot += MMX_SABER_WAVE_SLOT_BYTES)
    if (tagged(target, slot) &&
        target[slot + MMX_SABER_WAVE_SLOT_STAGE] !=
            target[MMX_SABER_WAVE_STAGE])
      retire_slot(target, slot);
}

void MmxSaberWaveRuntimeRetireAll(uint8_t *ram) {
  uint8_t *target = target_ram(ram);
  if (!target) return;
  for (unsigned slot = MMX_SABER_WAVE_SLOT_FIRST;
       slot < MMX_SABER_WAVE_SLOT_END; slot += MMX_SABER_WAVE_SLOT_BYTES)
    retire_slot(target, slot);
}

void MmxSaberWaveRuntimeReset(uint8_t *ram) {
  MmxSaberWaveRuntimeRetireAll(ram);
  collision_reset();
}

bool MmxSaberWaveRuntimeOwns(const uint8_t *ram, unsigned slot) {
  return live(ram, slot);
}

unsigned MmxSaberWaveRuntimeWeaponTick(uint8_t *ram, unsigned slot,
                                       unsigned value) {
  uint8_t age;
  uint8_t *target = target_ram(ram);
  (void)value;
  if (!target || !live(target, slot)) return value;
  if (!collision.ready ||
      target[slot + MMX_SABER_WAVE_SLOT_STAGE] !=
          target[MMX_SABER_WAVE_STAGE] ||
      target[slot + MMX_SABER_WAVE_SLOT_STATE] !=
          MMX_SABER_WAVE_SLOT_STATE_TRAVEL) {
    retire_slot(target, slot);
    return 0;
  }
  age = target[slot + MMX_SABER_WAVE_SLOT_AGE];
  if (age >= MMX_SABER_WAVE_LIFETIME) {
    retire_slot(target, slot);
    return 0;
  }
  if (target[slot + MMX_SABER_WAVE_SLOT_BIRTH]) {
    target[slot + MMX_SABER_WAVE_SLOT_BIRTH] = 0;
    if (offscreen(target, slot)) retire_slot(target, slot);
    return 0;
  }
  {
    int x = (int16_t)word(target + slot + 5);
    x += (target[slot + 0x11] & 0x40) ? MMX_SABER_WAVE_SPEED :
        -MMX_SABER_WAVE_SPEED;
    putword(target + slot + 5, (uint16_t)x);
  }
  ++age;
  target[slot + MMX_SABER_WAVE_SLOT_AGE] = age;
  if (age >= MMX_SABER_WAVE_LIFETIME || offscreen(target, slot)) {
    retire_slot(target, slot);
    return 0;
  }
  putword(target + slot + 0x20, MMX_SABER_WAVE_COLLISION_POINTER);
  return 0;
}

unsigned MmxSaberWaveRuntimeDamage(uint8_t *ram, unsigned enemy,
                                   unsigned slot, unsigned value) {
  (void)enemy;
  if (!MmxSaberWaveRuntimeOwns(ram, slot)) return value;
  /* No damage yet: the next wave unit replaces this harmless pass-through. */
  return 0;
}

unsigned MmxSaberWaveRuntimeHitbox(const uint8_t *ram, unsigned enemy,
                                   unsigned slot, unsigned value) {
  (void)enemy;
  if (MmxSaberWaveRuntimeOwns(ram, slot)) return 0;
  return value;
}

void MmxSaberWaveRuntimeSetCollisionRecord(const uint8_t *record,
                                            size_t size) {
  if (!record || size != MMX_SABER_WAVE_COLLISION_RECORD_BYTES) {
    collision_record_valid = false;
    return;
  }
  memcpy(collision_record, record, sizeof(collision_record));
  collision_record_valid = true;
}

void MmxSaberWaveRuntimeCollisionRom(uint8_t *rom, size_t size) {
  static const uint8_t empty[MMX_SABER_WAVE_COLLISION_RECORD_BYTES] =
      {0xff, 0xff, 0xff, 0xff};
  collision.ready = false;
  if (collision.installed &&
      (collision.rom != rom || !rom ||
       size < MMX_SABER_WAVE_COLLISION_ROM_OFFSET +
           MMX_SABER_WAVE_COLLISION_RECORD_BYTES)) {
    collision_reset();
  }
  if (!rom || size < MMX_SABER_WAVE_COLLISION_ROM_OFFSET +
          MMX_SABER_WAVE_COLLISION_RECORD_BYTES ||
      !collision_record_valid) return;
  if (collision.installed) {
    if (memcmp(rom + MMX_SABER_WAVE_COLLISION_ROM_OFFSET,
               collision.bytes, sizeof(collision.bytes)) == 0) {
      if (MmxZeroActive()) {
        collision.ready = true;
      } else {
        collision_reset();
      }
      return;
    }
    /* A third party changed the record. Never overwrite unknown bytes. */
    collision.installed = false;
    collision.rom = NULL;
    return;
  }
  if (!MmxZeroActive() ||
      memcmp(rom + MMX_SABER_WAVE_COLLISION_ROM_OFFSET, empty,
             sizeof(empty)) != 0)
    return;
  memcpy(rom + MMX_SABER_WAVE_COLLISION_ROM_OFFSET,
         collision_record, sizeof(collision_record));
  memcpy(collision.bytes, collision_record, sizeof(collision.bytes));
  collision.rom = rom;
  collision.installed = true;
  collision.ready = true;
}

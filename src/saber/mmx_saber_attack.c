#include "mmx_saber_attack.h"

#include <stdio.h>
#include <string.h>

#include "mmx_saber_sfx.h"

/* The native records are center offset, center offset, and half-extents.
 * Ground pointers use $FFD8 + 4*segment; air starts at $FF40, wall at
 * $FF50, and dash at $FF5C.  Values are copied from the donor table without
 * retuning; later units will install/use these records. */
static const MmxSaberBoundsSegment kGroundSlash1Bounds[] = {
    {4, 5, 7, -24, 11, 14},
    {6, 7, 29, -15, 18, 23},
    {8, 9, 37, -3, 16, 11},
    {10, 11, 37, 0, 16, 8}};

static const MmxSaberBoundsSegment kGroundSlash2Bounds[] = {
    {0, 3, 24, -5, 14, 13},
    {4, 7, 13, -5, 42, 13},
    {8, 11, -21, -5, 18, 13}};

static const MmxSaberBoundsSegment kGroundFinisherBounds[] = {
    {0, 5, -8, -16, 18, 19},
    {6, 9, 30, -16, 36, 24},
    {10, 13, 39, -11, 29, 19}};

static const MmxSaberBoundsSegment kAirSlashBounds[] = {
    {4, 5, 17, -16, 16, 13},
    {6, 7, 15, -7, 30, 20},
    {8, 9, 14, -3, 42, 24},
    {10, 11, -12, -6, 18, 12}};

static const MmxSaberBoundsSegment kWallSlashBounds[] = {
    {0, 3, 31, -10, 33, 19},
    {4, 7, 22, 0, 23, 15},
    {8, 11, 14, 1, 16, 12}};

static const MmxSaberBoundsSegment kDashSlashBounds[] = {
    {2, 5, 25, -1, 36, 9},
    {6, 9, 46, -2, 29, 10},
    {10, 11, 14, -11, 24, 4}};

/* Ported from old src/mmx_saber.c:260-400.  In particular, keep the numeric
 * timing values here independent of the ROM test's old-table oracle. */
static const MmxSaberAttack kSaberAttacks[] = {
    {
        .kind = SABER_KIND_GROUND1,
        .index = 0,
        .visual_animation = 1,
        .facing_xor = 1,
        .startup_ticks = 4,
        .active_ticks = 8,
        .recovery_ticks = 18,
        .total_ticks = 30,
        .chain_open_tick = 12,
        .chain_close_tick = 29,
        .buffer_open_tick = 4,
        .buffer_close_tick = 11,
        .next_index = 1,
        .bounds_segments = kGroundSlash1Bounds,
        .bounds_segment_count = sizeof(kGroundSlash1Bounds) /
            sizeof(kGroundSlash1Bounds[0]),
        .damage = 3,
        .bounds_pointer = MMX_SABER_ATTACK_BOUNDS_POINTER},
    {
        .kind = SABER_KIND_GROUND2,
        .index = 1,
        .visual_animation = 2,
        .facing_xor = 1,
        .startup_ticks = 0,
        .active_ticks = 12,
        .recovery_ticks = 18,
        .total_ticks = 30,
        .chain_open_tick = 12,
        .chain_close_tick = 29,
        .buffer_open_tick = 0,
        .buffer_close_tick = 11,
        .next_index = 2,
        .bounds_segments = kGroundSlash2Bounds,
        .bounds_segment_count = sizeof(kGroundSlash2Bounds) /
            sizeof(kGroundSlash2Bounds[0]),
        .damage = 3,
        .bounds_pointer = MMX_SABER_SLASH2_BOUNDS_POINTER},
    {
        .kind = SABER_KIND_GROUND3,
        .index = 2,
        .visual_animation = 3,
        .facing_xor = 1,
        .startup_ticks = 0,
        .active_ticks = 14,
        .recovery_ticks = 25,
        .total_ticks = 39,
        .chain_open_tick = MMX_SABER_NO_WINDOW,
        .chain_close_tick = MMX_SABER_NO_WINDOW,
        .buffer_open_tick = MMX_SABER_NO_WINDOW,
        .buffer_close_tick = MMX_SABER_NO_WINDOW,
        .next_index = MMX_SABER_NO_WINDOW,
        .bounds_segments = kGroundFinisherBounds,
        .bounds_segment_count = sizeof(kGroundFinisherBounds) /
            sizeof(kGroundFinisherBounds[0]),
        .damage = 8,
        .bounds_pointer = MMX_SABER_FINISHER_BOUNDS_POINTER},
    {
        .kind = SABER_KIND_AIR,
        .index = 0,
        .visual_animation = 4,
        .facing_xor = 1,
        .startup_ticks = 4,
        .active_ticks = 8,
        .recovery_ticks = 6,
        .total_ticks = 18,
        .chain_open_tick = MMX_SABER_NO_WINDOW,
        .chain_close_tick = MMX_SABER_NO_WINDOW,
        .buffer_open_tick = MMX_SABER_NO_WINDOW,
        .buffer_close_tick = MMX_SABER_NO_WINDOW,
        .next_index = MMX_SABER_NO_WINDOW,
        .bounds_segments = kAirSlashBounds,
        .bounds_segment_count = sizeof(kAirSlashBounds) /
            sizeof(kAirSlashBounds[0]),
        .damage = 3,
        .bounds_pointer = MMX_SABER_AIR_BOUNDS_POINTER},
    {
        .kind = SABER_KIND_WALL,
        .index = 0,
        .visual_animation = 5,
        .facing_xor = 1,
        .startup_ticks = 0,
        .active_ticks = 12,
        .recovery_ticks = 8,
        .chain_open_tick = MMX_SABER_NO_WINDOW,
        .chain_close_tick = MMX_SABER_NO_WINDOW,
        .buffer_open_tick = MMX_SABER_NO_WINDOW,
        .buffer_close_tick = MMX_SABER_NO_WINDOW,
        .next_index = MMX_SABER_NO_WINDOW,
        .bounds_segments = kWallSlashBounds,
        .bounds_segment_count = sizeof(kWallSlashBounds) /
            sizeof(kWallSlashBounds[0]),
        .damage = 3,
        .bounds_pointer = MMX_SABER_WALL_BOUNDS_POINTER},
    {
        .kind = SABER_KIND_DASH,
        .index = 0,
        .visual_animation = 6,
        .facing_xor = 1,
        .startup_ticks = 2,
        .active_ticks = 10,
        .recovery_ticks = 18,
        .total_ticks = 30,
        .chain_open_tick = MMX_SABER_NO_WINDOW,
        .chain_close_tick = MMX_SABER_NO_WINDOW,
        .buffer_open_tick = MMX_SABER_NO_WINDOW,
        .buffer_close_tick = MMX_SABER_NO_WINDOW,
        .next_index = MMX_SABER_NO_WINDOW,
        .bounds_segments = kDashSlashBounds,
        .bounds_segment_count = sizeof(kDashSlashBounds) /
            sizeof(kDashSlashBounds[0]),
        .damage = 3,
        .bounds_pointer = MMX_SABER_DASH_BOUNDS_POINTER},
    {
        .kind = SABER_KIND_SABER_LAND,
        .index = 0,
        .visual_animation = 7,
        .facing_xor = 1,
        .startup_ticks = 18,
        .active_ticks = 0,
        .recovery_ticks = 0,
        .total_ticks = 18,
        .chain_open_tick = MMX_SABER_NO_WINDOW,
        .chain_close_tick = MMX_SABER_NO_WINDOW,
        .buffer_open_tick = MMX_SABER_NO_WINDOW,
        .buffer_close_tick = MMX_SABER_NO_WINDOW,
        .next_index = MMX_SABER_NO_WINDOW,
        .bounds_segments = NULL,
        .bounds_segment_count = 0,
        .damage = 0,
        .bounds_pointer = 0}};

typedef struct MmxSaberAttackState {
  MmxSaberPadKind kind;
  uint8_t index;
  MmxSaberPadPhase phase;
  uint8_t tick;
  uint8_t buffer_slot;
  uint8_t facing;
  uint8_t swing_id;
  uint8_t anim_id;
  uint8_t anim_step;
  uint16_t projectile;
  uint16_t hit_slots;
  uint8_t projectile_tag_generation;
  uint8_t projectile_swing_id;
  uint8_t cue;
  bool cue_pending;
  bool previous_grounded;
  bool previous_grounded_valid;
  bool air_landing_eligible;
} MmxSaberAttackState;

static MmxSaberAttackState state;
static uint8_t projectile_generation;
static uint8_t *runtime_ram;
static unsigned cue_count;

typedef struct MmxSaberCollisionWindow {
  uint8_t *rom;
  uint8_t bytes[40];
  size_t offset;
  const char *name;
  bool installed;
  bool ready;
  bool warned;
} MmxSaberCollisionWindow;

static MmxSaberCollisionWindow ground_collision = {
    NULL, {0}, 0x37fd8, "$37FD8", false, false, false};
static MmxSaberCollisionWindow air_collision = {
    NULL, {0}, 0x37f40, "$37F40", false, false, false};
static unsigned collision_warnings;

static const MmxSaberAttack *state_attack(void) {
  return MmxSaberAttackRecord(state.kind, state.index);
}

static unsigned word(const uint8_t *p) {
  return p[0] | ((unsigned)p[1] << 8);
}

static void putword(uint8_t *p, unsigned value) {
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
}

static bool projectile_slot_valid(unsigned d) {
  return d >= 0x1228 && d < 0x1428 && (d & 63) == 0x28;
}

static bool enemy_slot_valid(unsigned d) {
  return d >= 0xe68 && d < 0x1228 && (d & 63) == 0x28;
}

static bool saber_tag_family(unsigned tag) {
  return (tag & MMX_SABER_PROJECTILE_TAG_FAMILY_MASK) ==
      MMX_SABER_PROJECTILE_TAG_FAMILY;
}

static bool current_projectile_tagged(const uint8_t *ram, unsigned d) {
  return ram && projectile_slot_valid(d) && ram[d] &&
      d == state.projectile && state.projectile_tag_generation != 0 &&
      word(ram + d + 0x3e) ==
          (MMX_SABER_PROJECTILE_TAG_FAMILY |
           state.projectile_tag_generation);
}

static bool saber_projectile_owned(const uint8_t *ram, unsigned d) {
  return current_projectile_tagged(ram, d) && state.phase == SABER_PHASE_ACTIVE;
}

static void retire_projectile_slot(uint8_t *ram, unsigned d) {
  bool live;
  if (!ram || !projectile_slot_valid(d)) return;
  live = ram[d] != 0;
  memset(ram + d, 0, 64);
  if (live && ram[0xbdd]) --ram[0xbdd];
}

static void retire_all_tagged(uint8_t *ram) {
  if (!ram) return;
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    if (saber_tag_family(word(ram + d + 0x3e))) retire_projectile_slot(ram, d);
}

static void release_current_projectile(uint8_t *ram) {
  if (ram && current_projectile_tagged(ram, state.projectile))
    retire_projectile_slot(ram, state.projectile);
  state.projectile = 0;
  state.projectile_tag_generation = 0;
  state.projectile_swing_id = 0;
  state.hit_slots = 0;
}

static bool tick_in_window(uint8_t tick, uint8_t open, uint8_t close) {
  return open != MMX_SABER_NO_WINDOW && close != MMX_SABER_NO_WINDOW &&
      tick >= open && tick <= close;
}

static uint8_t facing_for_direction(uint8_t direction, uint8_t fallback) {
  direction &= MMX_SABER_NATIVE_HORIZONTAL_BITS;
  /* Native input maps right to action bit 0 and left to action bit 1.  The
   * renderer/native facing convention is $40 for right, so a single held
   * direction can replace the lock while both/neither preserve it. */
  if (direction == 1) return 0x40;
  if (direction == 2) return 0;
  return fallback & 0x40;
}

static const MmxSaberAttack *next_ground_attack(
    const MmxSaberAttack *attack) {
  if (!attack || attack->next_index == MMX_SABER_NO_WINDOW)
    return NULL;
  return MmxSaberAttackRecord(
      (MmxSaberPadKind)(SABER_KIND_GROUND1 + attack->next_index),
      attack->next_index);
}

static uint8_t animation_step_for_tick(uint8_t animation, uint8_t tick) {
  /* The donor sidecar uses two ticks per step for animations 1, 2, 4, 5, 6,
   * and 7. Animation 3's final three steps are three ticks each. Keeping the
   * resolved step here makes the state query useful before rendering exists. */
  if (animation == 3 && tick >= 30)
    return (uint8_t)(15 + (tick - 30) / 3);
  return (uint8_t)(tick / 2);
}

static void update_animation(void) {
  state.anim_step = state.anim_id ?
      animation_step_for_tick(state.anim_id, state.tick) : 0;
}

static MmxSaberSfxAttackCue attack_cue(const MmxSaberAttack *attack) {
  if (!attack) return MMX_SABER_SFX_ATTACK_COUNT;
  switch (attack->kind) {
    case SABER_KIND_GROUND1:
      return MMX_SABER_SFX_ATTACK_GROUND_SLASH_1;
    case SABER_KIND_GROUND2:
      return MMX_SABER_SFX_ATTACK_GROUND_SLASH_2;
    case SABER_KIND_GROUND3:
      return MMX_SABER_SFX_ATTACK_GROUND_SLASH_3;
    case SABER_KIND_AIR:
      return MMX_SABER_SFX_ATTACK_AIR;
    case SABER_KIND_WALL:
      return MMX_SABER_SFX_ATTACK_WALL;
    case SABER_KIND_DASH:
      return MMX_SABER_SFX_ATTACK_DASH;
    default:
      return MMX_SABER_SFX_ATTACK_COUNT;
  }
}

static void clear_attack(void) {
  state.kind = SABER_KIND_NONE;
  state.index = 0;
  state.phase = SABER_PHASE_IDLE;
  state.tick = 0;
  state.buffer_slot = 0;
  state.facing = 0;
  state.anim_id = 0;
  state.anim_step = 0;
  state.projectile = 0;
  state.projectile_tag_generation = 0;
  state.projectile_swing_id = 0;
  state.hit_slots = 0;
  state.cue = MMX_SABER_SFX_ATTACK_COUNT;
  state.cue_pending = false;
}

static const MmxSaberBoundsSegment *bounds_segment(
    const MmxSaberAttack *attack, uint8_t tick, unsigned *index) {
  if (!attack || !attack->bounds_segments) return NULL;
  for (unsigned i = 0; i < attack->bounds_segment_count &&
                       i < MMX_SABER_MAX_ACTIVE_SEGMENTS; ++i) {
    const MmxSaberBoundsSegment *segment = attack->bounds_segments + i;
    if (tick >= segment->first_tick && tick <= segment->last_tick) {
      if (index) *index = i;
      return segment;
    }
  }
  return NULL;
}

static uint16_t bounds_pointer(const MmxSaberAttack *attack, uint8_t tick) {
  unsigned index = 0;
  if (bounds_segment(attack, tick, &index))
    return (uint16_t)(attack->bounds_pointer + index * 4);
  return attack ? attack->bounds_pointer : MMX_SABER_ATTACK_BOUNDS_POINTER;
}

static void bounds_records_for_window(uint8_t out[40], uint16_t window_pointer) {
  memset(out, 255, 40);
  for (size_t attack_index = 0;
       attack_index < sizeof(kSaberAttacks) / sizeof(kSaberAttacks[0]);
       ++attack_index) {
    const MmxSaberAttack *attack = kSaberAttacks + attack_index;
    if (attack->bounds_pointer < window_pointer ||
        attack->bounds_pointer >= window_pointer + 40) continue;
    unsigned record = (attack->bounds_pointer - window_pointer) / 4;
    for (unsigned segment_index = 0;
         segment_index < attack->bounds_segment_count &&
             segment_index < MMX_SABER_MAX_ACTIVE_SEGMENTS; ++segment_index) {
      const MmxSaberBoundsSegment *segment =
          attack->bounds_segments + segment_index;
      if (record + segment_index >= 10) continue;
      unsigned offset = (record + segment_index) * 4;
      out[offset] = (uint8_t)segment->bounds_x;
      out[offset + 1] = (uint8_t)segment->bounds_y;
      out[offset + 2] = segment->bounds_half_width;
      out[offset + 3] = segment->bounds_half_height;
    }
  }
}

static bool air_collision_attack(const MmxSaberAttack *attack) {
  return attack && (attack->kind == SABER_KIND_AIR ||
                    attack->kind == SABER_KIND_WALL ||
                    attack->kind == SABER_KIND_DASH);
}

static bool collision_ready(const MmxSaberAttack *attack) {
  return air_collision_attack(attack) ? air_collision.ready :
      (attack && attack->bounds_segments && ground_collision.ready);
}

static void collision_window_update(MmxSaberCollisionWindow *window,
                                    uint8_t *rom, size_t size,
                                    const uint8_t expected[40]) {
  uint8_t empty[40];
  if (!window || !expected) return;
  memset(empty, 255, sizeof(empty));
  window->ready = false;

  if (window->installed &&
      (window->rom != rom || !rom || size < window->offset + 40)) {
    if (window->rom &&
        !memcmp(window->rom + window->offset, window->bytes, sizeof(window->bytes)))
      memcpy(window->rom + window->offset, empty, sizeof(empty));
    else if (!window->warned) {
      fprintf(stderr,
          "MMX Saber: collision window %s was modified; attacks disabled\n",
          window->name);
      window->warned = true;
      ++collision_warnings;
    }
    window->installed = false;
    window->rom = NULL;
  }
  if (!rom || size < window->offset + 40) return;

  if (window->installed) {
    if (!memcmp(rom + window->offset, window->bytes, sizeof(window->bytes))) {
      if (MmxZeroActive()) {
        window->ready = true;
      } else {
        memcpy(rom + window->offset, empty, sizeof(empty));
        window->installed = false;
        window->rom = NULL;
        window->warned = false;
      }
      return;
    }
    window->installed = false;
    window->rom = NULL;
    if (!memcmp(rom + window->offset, empty, sizeof(empty))) {
      window->warned = false;
    } else {
      if (!window->warned) {
        fprintf(stderr,
            "MMX Saber: collision window %s was modified; attacks disabled\n",
            window->name);
        window->warned = true;
        ++collision_warnings;
      }
      return;
    }
  }

  if (!MmxZeroActive()) return;
  if (!memcmp(rom + window->offset, expected, sizeof(window->bytes))) {
    memcpy(window->bytes, expected, sizeof(window->bytes));
  } else if (!memcmp(rom + window->offset, empty, sizeof(empty))) {
    memcpy(rom + window->offset, expected, sizeof(window->bytes));
    memcpy(window->bytes, expected, sizeof(window->bytes));
  } else {
    if (!window->warned) {
      fprintf(stderr,
          "MMX Saber: collision window %s is not ours; attacks disabled\n",
          window->name);
      window->warned = true;
      ++collision_warnings;
    }
    return;
  }
  window->rom = rom;
  window->installed = true;
  window->ready = true;
}

void MmxSaberAttackCollisionRom(uint8_t *rom, size_t size) {
  uint8_t ground[40], air[40];
  bounds_records_for_window(ground, MMX_SABER_ATTACK_BOUNDS_POINTER);
  bounds_records_for_window(air, MMX_SABER_AIR_BOUNDS_POINTER);
  collision_window_update(&ground_collision, rom, size, ground);
  collision_window_update(&air_collision, rom, size, air);
}

const MmxSaberAttack *MmxSaberAttackRecord(MmxSaberPadKind kind,
                                            uint8_t index) {
  size_t i;
  for (i = 0; i < sizeof(kSaberAttacks) / sizeof(kSaberAttacks[0]); ++i)
    if (kSaberAttacks[i].kind == kind && kSaberAttacks[i].index == index)
      return &kSaberAttacks[i];
  return NULL;
}

const MmxSaberAttack *MmxSaberAttackTableAt(size_t id) {
  if (!id || id > sizeof(kSaberAttacks) / sizeof(kSaberAttacks[0]))
    return NULL;
  return &kSaberAttacks[id - 1];
}

size_t MmxSaberAttackTableCount(void) {
  return sizeof(kSaberAttacks) / sizeof(kSaberAttacks[0]);
}

MmxSaberPadPhase MmxSaberAttackPhaseForTick(const MmxSaberAttack *attack,
                                            uint8_t tick) {
  if (!attack) return SABER_PHASE_IDLE;
  if (tick < attack->startup_ticks) return SABER_PHASE_STARTUP;
  if (tick < (uint8_t)(attack->startup_ticks + attack->active_ticks))
    return SABER_PHASE_ACTIVE;
  return SABER_PHASE_RECOVERY;
}

void MmxSaberAttackReset(void) {
  MmxSaberAttackExit(runtime_ram, MMX_SABER_ATTACK_EXIT_CONTEXT);
  memset(&state, 0, sizeof(state));
  state.phase = SABER_PHASE_IDLE;
  state.cue = MMX_SABER_SFX_ATTACK_COUNT;
}

void MmxSaberAttackResetRam(uint8_t *ram) {
  runtime_ram = ram;
  MmxSaberAttackReset();
}

void MmxSaberAttackResetCueCount(void) {
  cue_count = 0;
}

void MmxSaberAttackExit(uint8_t *ram, MmxSaberAttackExitReason reason) {
  uint8_t *target = ram ? ram : runtime_ram;
  const bool preserve_air_landing =
      state.kind == SABER_KIND_AIR && reason == MMX_SABER_ATTACK_EXIT_NATURAL;
  if (target) runtime_ram = target;
  retire_all_tagged(target);
  clear_attack();
  if (!preserve_air_landing) state.air_landing_eligible = false;
}

static void start_attack(const MmxSaberAttack *attack, uint8_t facing) {
  uint8_t swing_id;
  if (!attack) return;
  swing_id = (uint8_t)(state.swing_id + 1);
  if (!swing_id) swing_id = 1;
  state.kind = attack->kind;
  state.index = attack->index;
  state.phase = MmxSaberAttackPhaseForTick(attack, 0);
  state.tick = 0;
  state.buffer_slot = 0;
  state.facing = facing & 0x40;
  state.swing_id = swing_id;
  state.anim_id = attack->visual_animation;
  state.hit_slots = 0;
  state.cue = attack_cue(attack);
  state.cue_pending = state.cue < MMX_SABER_SFX_ATTACK_COUNT;
  if (attack->kind == SABER_KIND_AIR)
    state.air_landing_eligible = true;
  else
    state.air_landing_eligible = false;
  update_animation();
}

static bool start_land_visual(uint8_t facing) {
  const MmxSaberAttack *attack =
      MmxSaberAttackRecord(SABER_KIND_SABER_LAND, 0);
  if (!attack || state.phase != SABER_PHASE_IDLE) return false;
  start_attack(attack, facing);
  return true;
}

void MmxSaberAttackStep(bool saber_pressed, bool grounded, bool playable,
                        uint8_t native_facing,
                        uint8_t horizontal_direction) {
  const MmxSaberAttack *attack;

  if (!playable) {
    if (state.phase != SABER_PHASE_IDLE || state.projectile || state.hit_slots)
      MmxSaberAttackExit(runtime_ram, MMX_SABER_ATTACK_EXIT_CONTEXT);
    return;
  }

  if (state.phase == SABER_PHASE_IDLE) {
    if (saber_pressed)
      start_attack(MmxSaberAttackRecord(
                       grounded ? SABER_KIND_GROUND1 : SABER_KIND_AIR, 0),
                   facing_for_direction(horizontal_direction, native_facing));
    return;
  }

  attack = state_attack();
  if (!attack || !attack->total_ticks) {
    clear_attack();
    return;
  }

  /* The old landing visual has no combo priority: a Y edge cancels it and,
   * while still grounded, immediately claims ordinary ground slash 1. */
  if (attack->kind == SABER_KIND_SABER_LAND && saber_pressed) {
    MmxSaberAttackExit(runtime_ram, MMX_SABER_ATTACK_EXIT_CONTEXT);
    if (grounded)
      start_attack(MmxSaberAttackRecord(SABER_KIND_GROUND1, 0),
                   facing_for_direction(horizontal_direction, native_facing));
    return;
  }

  ++state.tick;
  if (state.tick >= attack->total_ticks) {
    MmxSaberAttackExit(runtime_ram, MMX_SABER_ATTACK_EXIT_NATURAL);
    return;
  }

  if (attack->kind == SABER_KIND_GROUND1 ||
      attack->kind == SABER_KIND_GROUND2) {
    if (saber_pressed && tick_in_window(state.tick,
                                        attack->chain_open_tick,
                                        attack->chain_close_tick)) {
      const MmxSaberAttack *next = next_ground_attack(attack);
      if (next) {
        start_attack(next,
                     facing_for_direction(horizontal_direction, state.facing));
        return;
      }
    }

    if (saber_pressed && !state.buffer_slot &&
        tick_in_window(state.tick, attack->buffer_open_tick,
                       attack->buffer_close_tick))
      /* The old state has exactly one pending entry. */
      state.buffer_slot = 1;

    if (state.buffer_slot && state.tick == attack->chain_open_tick) {
      const MmxSaberAttack *next = next_ground_attack(attack);
      /* Facing is sampled at acceptance, not when the early edge was queued. */
      state.buffer_slot = 0;
      if (next) {
        start_attack(next,
                     facing_for_direction(horizontal_direction, state.facing));
        return;
      }
    }
  }

  state.phase = MmxSaberAttackPhaseForTick(attack, state.tick);
  update_animation();
}

static uint8_t next_projectile_generation(void) {
  if (++projectile_generation == 0) ++projectile_generation;
  return projectile_generation;
}

static uint8_t projectile_facing(const MmxSaberAttack *attack) {
  bool mirror = (state.facing != 0) ^ (attack && attack->facing_xor != 0);
  return mirror ? 0x40 : 0;
}

static void anchor_projectile(uint8_t *ram, unsigned d,
                              const MmxSaberAttack *attack) {
  putword(ram + d + 5, word(ram + 0x0bad));
  putword(ram + d + 8, word(ram + 0x0bb0));
  ram[d + 0x11] = projectile_facing(attack);
  putword(ram + d + 0x20,
          collision_ready(attack) ? bounds_pointer(attack, state.tick) : 0);
  ram[d + 0x30] = 0;
}

static unsigned free_projectile(const uint8_t *ram) {
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    if (!word(ram + d)) return d;
  return 0;
}

static bool allocate_projectile(uint8_t *ram, const MmxSaberAttack *attack) {
  unsigned d = free_projectile(ram);
  uint8_t generation;
  if (!d) return false;
  generation = next_projectile_generation();
  memset(ram + d, 0, 64);
  ram[d] = 1;
  ram[d + 1] = 2;
  ram[d + 10] = 3;
  ram[d + 0x11] = projectile_facing(attack);
  putword(ram + d + 0x3e,
          MMX_SABER_PROJECTILE_TAG_FAMILY | generation);
  state.projectile = (uint16_t)d;
  state.projectile_tag_generation = generation;
  state.projectile_swing_id = state.swing_id;
  state.hit_slots = 0;
  ++ram[0x0bdd];
  anchor_projectile(ram, d, attack);
  return true;
}

void MmxSaberAttackRuntimeTick(uint8_t *ram) {
  const MmxSaberAttack *attack;
  runtime_ram = ram;
  if (!ram) return;
  attack = state_attack();
  if (state.phase != SABER_PHASE_ACTIVE || !attack ||
      !attack->bounds_segments) {
    if (state.projectile) release_current_projectile(ram);
    return;
  }
  if (state.projectile && state.projectile_swing_id != state.swing_id)
    release_current_projectile(ram);
  if (!state.projectile && !allocate_projectile(ram, attack)) {
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_PROJECTILE);
    return;
  }
  if (!saber_projectile_owned(ram, state.projectile)) {
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_PROJECTILE);
    return;
  }
  anchor_projectile(ram, state.projectile, attack);
}

static bool player_grounded(const uint8_t *ram) {
  return ram && ((ram[0x0bd3] & 4) || (ram[0x0bd4] & 4));
}

static void emit_pending_cue(void) {
  const MmxSaberAttack *attack;
  if (!state.cue_pending) return;
  attack = state_attack();
  state.cue_pending = false;
  if (!attack || attack->kind == SABER_KIND_SABER_LAND ||
      state.cue >= MMX_SABER_SFX_ATTACK_COUNT)
    return;
  MmxSaberSfxPlayForAttack((MmxSaberSfxAttackCue)state.cue);
  ++cue_count;
}

void MmxSaberAttackPlayerEnd(uint8_t *ram) {
  bool grounded;
  bool landed;
  bool start_land = false;
  uint8_t landing_facing = 0;

  if (ram) runtime_ram = ram;
  if (!ram) return;

  grounded = player_grounded(ram);
  landed = state.previous_grounded_valid && !state.previous_grounded && grounded;
  if (state.previous_grounded && !grounded && state.kind != SABER_KIND_AIR)
    state.air_landing_eligible = false;
  state.previous_grounded = grounded;
  state.previous_grounded_valid = true;

  /* The grounded edge is observed here, after native movement, so this is
   * the sole landing owner.  An idle Saber can still claim the visual after
   * an AIR owner naturally completed during the same airtime; ground/dash/
   * wall owners remain untouched below. */
  if (landed && state.phase == SABER_PHASE_IDLE && state.air_landing_eligible) {
    landing_facing = (uint8_t)(ram[0x0c11] & 0x40);
    start_land = true;
  } else if (landed && state.kind == SABER_KIND_AIR) {
    landing_facing = state.facing;
    emit_pending_cue();
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_LANDING);
    start_land = true;
  }
  if (start_land) {
    (void)start_land_visual(landing_facing);
    return;
  }

  emit_pending_cue();
}

uint8_t MmxSaberAttackFacing(void) {
  return state.facing & 0x40;
}

MmxSaberPadSaber MmxSaberAttackPadState(bool release_pending) {
  return (MmxSaberPadSaber){state.phase, state.kind, false, false,
                            release_pending};
}

MmxSaberAttackSnapshot MmxSaberAttackSnapshotGet(void) {
  return (MmxSaberAttackSnapshot){state.kind, state.index, state.phase,
                                  state.tick, state.anim_id, state.anim_step};
}

MmxSaberAttackSnapshot MmxSaberAttackGetSnapshot(void) {
  return MmxSaberAttackSnapshotGet();
}

unsigned MmxSaberAttackWeaponTick(uint8_t *ram, unsigned projectile,
                                  unsigned value) {
  if (!ram || !projectile_slot_valid(projectile) || !ram[projectile] ||
      !saber_tag_family(word(ram + projectile + 0x3e)))
    return value;
  if (saber_projectile_owned(ram, projectile)) return 0;
  retire_projectile_slot(ram, projectile);
  if (projectile == state.projectile) {
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_PROJECTILE);
  }
  return 0;
}

unsigned MmxSaberAttackDamage(uint8_t *ram, unsigned enemy,
                              unsigned projectile, unsigned value) {
  unsigned bit;
  if (!saber_projectile_owned(ram, projectile) || !enemy_slot_valid(enemy) ||
      !value || (value & 128))
    return value;
  bit = 1u << ((enemy - 0xe68) / 64);
  if (state.hit_slots & bit) return 0;
  state.hit_slots |= (uint16_t)bit;
  return 3;
}

unsigned MmxSaberAttackHitbox(const uint8_t *ram, unsigned enemy,
                              unsigned projectile, unsigned value) {
  if (saber_projectile_owned(ram, projectile) && enemy_slot_valid(enemy) &&
      (state.hit_slots & (1u << ((enemy - 0xe68) / 64))))
    return 0;
  return value;
}

uint16_t MmxSaberAttackHitSlots(void) {
  return state.hit_slots;
}

unsigned MmxSaberAttackCollisionWarningCount(void) {
  return collision_warnings;
}

unsigned MmxSaberAttackCueCount(void) {
  return cue_count;
}

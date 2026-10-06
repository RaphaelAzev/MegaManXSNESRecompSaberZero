#include "mmx_saber_attack.h"

#include <string.h>

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
} MmxSaberAttackState;

static MmxSaberAttackState state;

static const MmxSaberAttack *state_attack(void) {
  return MmxSaberAttackRecord(state.kind, state.index);
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

static void clear_attack(void) {
  state.kind = SABER_KIND_NONE;
  state.index = 0;
  state.phase = SABER_PHASE_IDLE;
  state.tick = 0;
  state.buffer_slot = 0;
  state.facing = 0;
  state.anim_id = 0;
  state.anim_step = 0;
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
  memset(&state, 0, sizeof(state));
  state.phase = SABER_PHASE_IDLE;
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
  update_animation();
}

void MmxSaberAttackStep(bool saber_pressed, bool grounded, bool playable,
                        uint8_t native_facing,
                        uint8_t horizontal_direction) {
  const MmxSaberAttack *attack;

  if (!playable) {
    MmxSaberAttackReset();
    return;
  }

  if (state.phase == SABER_PHASE_IDLE) {
    if (saber_pressed && grounded)
      start_attack(MmxSaberAttackRecord(SABER_KIND_GROUND1, 0),
                   facing_for_direction(horizontal_direction, native_facing));
    return;
  }

  attack = state_attack();
  if (!attack || !attack->total_ticks) {
    clear_attack();
    return;
  }
  ++state.tick;
  if (state.tick >= attack->total_ticks) {
    clear_attack();
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

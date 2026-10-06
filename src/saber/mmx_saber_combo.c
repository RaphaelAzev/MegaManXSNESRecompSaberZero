#include "mmx_saber_combo.h"

#include <string.h>

#include "../mmx_zero.h"
#include "mmx_saber_sfx.h"

typedef struct MmxSaberComboState {
  uint8_t window_ticks;
  uint8_t window_frames;
  uint8_t pre_live_mask;
  uint8_t pre_burst;
  uint16_t reserved_slot;
  unsigned finisher_cue_count;
  bool pre_valid;
  bool slash_request;
  bool slash_request_sent;
} MmxSaberComboState;

static MmxSaberComboState state = {
  0, MMX_SABER_DEFAULT_FINISHER_WINDOW, 0, 0, 0, 0, false, false, false};
static uint8_t wave_generation;
static uint8_t *runtime_ram;

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

static bool free_projectile(const uint8_t *ram, unsigned d) {
  return projectile_slot_valid(d) && word(ram + d) == 0;
}

static uint8_t next_wave_generation(void) {
  if (++wave_generation == 0) ++wave_generation;
  return wave_generation;
}

static uint8_t live_burst_mask(const uint8_t *ram, MmxZeroState snapshot) {
  uint8_t live = 0;
  if (!ram) return 0;
  for (unsigned i = 0; i < 8; ++i) {
    const unsigned d = 0x1228 + i * 64;
    if ((snapshot.shot_mask & (uint8_t)(1u << i)) && ram[d] &&
        ram[d + 10] == 3 && word(ram + d + 0x3e) != 0x5a53)
      live |= (uint8_t)(1u << i);
  }
  return live;
}

static void release_reservation(uint8_t *ram) {
  uint8_t *target = ram ? ram : runtime_ram;
  const unsigned d = state.reserved_slot;
  if (target && d && projectile_slot_valid(d)) {
    const bool counted = target[d] != 0;
    memset(target + d, 0, 64);
    if (counted && target[0x0bdd]) --target[0x0bdd];
  }
  state.reserved_slot = 0;
}

static bool reserve_wave_slot(uint8_t *ram) {
  unsigned first = 0;
  unsigned free_count = 0;
  if (!ram) return false;
  for (unsigned d = 0x1228; d < 0x1428; d += 64) {
    if (!free_projectile(ram, d)) continue;
    if (!first) first = d;
    ++free_count;
  }
  /* Reserve only after the complete two-slot precondition is known. */
  if (free_count < 2) return false;
  memset(ram + first, 0, 64);
  ram[first + 1] = 0x80; /* inactive reservation, not a native projectile */
  putword(ram + first + 0x3e,
          MMX_SABER_WAVE_TAG_FAMILY | next_wave_generation());
  state.reserved_slot = (uint16_t)first;
  return true;
}

void MmxSaberComboSetWindowFrames(unsigned frames) {
  if (frames > MMX_SABER_MAX_FINISHER_WINDOW)
    frames = MMX_SABER_MAX_FINISHER_WINDOW;
  state.window_frames = (uint8_t)frames;
}

void MmxSaberComboReset(uint8_t *ram) {
  if (ram) runtime_ram = ram;
  release_reservation(ram);
  state.window_ticks = 0;
  state.pre_live_mask = 0;
  state.pre_burst = 0;
  state.pre_valid = false;
  state.slash_request = false;
  state.slash_request_sent = false;
  state.finisher_cue_count = 0;
}

void MmxSaberComboCancel(uint8_t *ram) {
  MmxSaberComboReset(ram);
}

bool MmxSaberComboPrePlayer(uint8_t *ram, bool saber_pressed) {
  const MmxZeroState snapshot = MmxZeroGetState();
  bool claimed = false;
  if (ram) runtime_ram = ram;

  state.pre_live_mask = live_burst_mask(ram, snapshot);
  state.pre_burst = snapshot.burst;
  state.pre_valid = true;

  if (state.window_ticks && saber_pressed && !snapshot.slash) {
    /* The edge belongs to this window even when allocation cannot succeed. */
    claimed = true;
    state.window_ticks = 0;
    if (!state.slash_request && !state.slash_request_sent)
      state.slash_request = reserve_wave_slot(ram);
  }
  if (state.window_ticks) --state.window_ticks;
  return claimed;
}

bool MmxSaberComboLegacySlashRequest(const uint8_t *ram) {
  if (!ram || !state.slash_request) return false;
  runtime_ram = (uint8_t *)(uintptr_t)ram;
  state.slash_request = false;
  state.slash_request_sent = true;
  return true;
}

void MmxSaberComboPlayerEnd(uint8_t *ram) {
  const MmxZeroState snapshot = MmxZeroGetState();
  const uint8_t post_live_mask = live_burst_mask(ram, snapshot);
  if (ram) runtime_ram = ram;

  /* A burst-fired flag is not enough: only a newly live class-3 slot opens
   * the window, and it must be the second X3 burst. */
  if (state.pre_valid && state.pre_burst == 2 &&
      (post_live_mask & (uint8_t)~state.pre_live_mask) &&
      !snapshot.slash && !state.window_ticks && state.window_frames)
    state.window_ticks = state.window_frames;
  state.pre_valid = false;

  if (state.slash_request_sent) {
    if (snapshot.slash == 1) {
      MmxSaberSfxPlayForAttack(MMX_SABER_SFX_ATTACK_X3_FINISHER);
      ++state.finisher_cue_count;
      state.slash_request_sent = false;
    } else if (!snapshot.slash) {
      /* The request was consumed, but upstream did not publish slash == 1. */
      state.slash_request_sent = false;
      release_reservation(ram);
    }
  } else if (state.slash_request) {
    /* The player tick was skipped or could not poll the seam this frame. */
    state.slash_request = false;
    release_reservation(ram);
  }
}

unsigned MmxSaberComboWindowTicks(void) {
  return state.window_ticks;
}

unsigned MmxSaberComboReservedSlot(void) {
  return state.reserved_slot;
}

unsigned MmxSaberComboFinisherCueCount(void) {
  return state.finisher_cue_count;
}

#include "mmx_saber_frame.h"

#include "mmx_saber_attack.h"
#include "mmx_saber_armor.h"
#include "mmx_saber_black_zero.h"
#include "mmx_saber_combo.h"
#include "mmx_saber_frame_pad.h"
#include "mmx_saber_hitbox_debug.h"
#include "mmx_saber_input.h"
#include "mmx_saber_priority.h"
#include "mmx_saber_projectile.h"
#include "mmx_saber_tuning.h"
#include "mmx_saber_wave_runtime.h"
#include "mmx_saber_state.h"

#include <string.h>

static bool release_pending;
static bool release_pending_from_special;
static bool previous_y;
static bool previous_x;
/* Host-only suppression for a claimed X hold; reset on X release or frame reset. */
static bool weapon_technique_hold;
static MmxSaberFramePadSource pad_source;
static MmxZeroLegacyIntent frame_intent;
static bool frame_computed;
static bool frame_override;
static bool last_wrote_input;
static const uint8_t *frame_ram;

static void clear_frame_state(void);

static unsigned charge_cap(void) {
  return 200;
}

static unsigned charge_rate(void) {
  return MmxSaberArmorCurrent().head ? 0x180 : 0x100;
}

static unsigned weapon_cost(unsigned weapon_id, unsigned cost, bool charged) {
  unsigned half;
  (void)charged;
  if (weapon_id == 0 || !cost || !MmxSaberArmorCurrent().head) return cost;
  half = cost >> 1;
  return half ? half : 1;
}

enum {
  SABER_WEAPON_SPARK = 0x0c,
  SABER_WEAPON_CUTTER = 0x0e,
  SABER_WEAPON_ENERGY_BASE = 0x1f85,
  SABER_WEAPON_ENERGY_MASK = 0x3fff,
  SABER_WEAPON_OWNED_BIT = 0x40,
  SABER_WEAPON_STATUS_BITS = 0xc000,
};

static bool weapon_technique_selected(unsigned weapon_id) {
  return weapon_id == SABER_WEAPON_SPARK ||
      weapon_id == SABER_WEAPON_CUTTER;
}

static unsigned weapon_energy_offset(unsigned weapon_id) {
  return SABER_WEAPON_ENERGY_BASE + weapon_id;
}

static uint16_t weapon_energy_word(const uint8_t *ram, unsigned weapon_id) {
  const unsigned offset = weapon_energy_offset(weapon_id);
  return (uint16_t)(ram[offset] | (uint16_t)ram[offset + 1] << 8);
}

static bool weapon_owned(const uint8_t *ram, unsigned weapon_id) {
  return (ram[weapon_energy_offset(weapon_id) + 1] &
          SABER_WEAPON_OWNED_BIT) != 0;
}

static bool weapon_owned_with_energy(const uint8_t *ram, unsigned weapon_id) {
  const unsigned offset = weapon_energy_offset(weapon_id);
  const uint16_t word = weapon_energy_word(ram, weapon_id);
  return weapon_owned(ram, weapon_id) &&
      (word & SABER_WEAPON_ENERGY_MASK) != 0;
}

static void clear_special_weapon_latches(uint8_t *ram) {
  ram[0x0bff] = 0;
  ram[0x0c00] = 0;
  ram[0x0c01] = 0;
  ram[0x0c03] = 0;
}

static void debit_weapon_energy(uint8_t *ram, unsigned weapon_id,
                                unsigned cost) {
  const unsigned offset = weapon_energy_offset(weapon_id);
  const unsigned usable = weapon_energy_word(ram, weapon_id) &
      SABER_WEAPON_ENERGY_MASK;
  const unsigned spend = weapon_cost(weapon_id, cost, false);
  const unsigned remaining = usable >= spend ? usable - spend : 0;
  const uint16_t stored = (uint16_t)(remaining | SABER_WEAPON_STATUS_BITS);
  ram[offset] = (uint8_t)stored;
  ram[offset + 1] = (uint8_t)(stored >> 8);
}

static bool modern_movement(void) {
  return MmxSaberArmorCurrent().legs;
}

static void reset_frame_state(uint8_t *ram, bool retire_attack_ram) {
  if (ram) frame_ram = ram;
  MmxSaberArmorApplyStageStart(ram);
  MmxSaberHitboxDebugSetRam(ram);
  if (ram)
    MmxSaberAttackResetRam(ram);
  else if (retire_attack_ram)
    MmxSaberAttackReset();
  else
    MmxSaberAttackResetHostState();
  MmxSaberWaveRuntimeReset(ram);
  MmxSaberProjectileReset(ram);
  MmxSaberPriorityReset();
  MmxSaberComboReset(ram);
  MmxSaberAttackResetCueCount();
  MmxSaberBlackZeroNativeColorsUpdate(false, false, NULL);
  release_pending = false;
  release_pending_from_special = false;
  previous_y = false;
  previous_x = false;
  weapon_technique_hold = false;
  clear_frame_state();
}

static void state_reset(uint8_t *ram) {
  reset_frame_state(ram, true);
}

static void clear_frame_state(void) {
  frame_intent = (MmxZeroLegacyIntent){false, false, false};
  frame_computed = false;
  frame_override = false;
  last_wrote_input = false;
}

static bool zero_frame_context(const uint8_t *ram) {
  /* These are the same live-player fields used by the old branch's
   * zero_player_base_context/saber_translation_context checks. */
  return ram && MmxZeroActive() && !MmxZeroSwapping() &&
      ram[0xd1] == 2 && ram[0xd2] == 4 && ram[0xd3] == 4 &&
      ram[0xba9] == 2 && (ram[0xbcf] & 127) && !ram[0x1f0c] &&
      ram[0x1f10] < 6 && ram[0xbbe] != 0x6b && ram[0xbaa] != 0x2c &&
      /* Native restraint/freeze action ($84:9FEB / Chill Penguin ice). */
      ram[0xbaa] != 0x1e;
}

void MmxSaberFrameSetPadSource(MmxSaberFramePadSource source) {
  pad_source = source;
}

static MmxSaberFramePad sample_pad(const uint8_t *ram) {
  MmxSaberFramePad pad = {false, false, false, false, false};
  if (!ram) return pad;
  if (pad_source) {
    pad_source(ram, &pad);
    return pad;
  }
  return MmxSaberFrameSampleNativePad(ram);
}

/* X1 fires only from the player actions whose handler in the $81:82A6 table
 * (indexed by $0BAA / 2) reaches the buster code at $81:94AF / $81:9D47:
 * stand/walk/jump/fall/land $00-$0A, $10, wall $12, dash $14, $20, $28, $2A
 * and $40. Everywhere else - Vile's electric restraint $32, Flame Mammoth's
 * ground shock $36, item refills, scripted poses - X cannot shoot, so the
 * Saber cannot swing. Hurt $0E keeps its own handling below. */
bool MmxSaberFrameNativeFireAction(uint8_t action) {
  return action <= 0x0a || action == 0x10 || action == 0x12 ||
      action == 0x14 || action == 0x20 || action == 0x28 || action == 0x2a ||
      action == 0x40;
}

static bool native_fire_locked(const uint8_t *ram) {
  return ram && ram[0x0baa] != 0x0e && ram[0x0baa] != 0x0c &&
      !MmxSaberFrameNativeFireAction(ram[0x0baa]);
}

static MmxSaberPhysicalPad read_physical_pad(const uint8_t *ram,
                                             MmxSaberFramePad pad) {
  const bool x_held = pad.x_held;
  const bool x_previous = pad_source ? previous_x :
      (ram[0x00a9] & 0x40) != 0;
  const bool y_held = pad.y_pressed;
  uint16_t buttons = 0;
  uint16_t previous = 0;
  if (x_held) buttons |= MMX_SABER_PAD_X;
  if (y_held) buttons |= MMX_SABER_PAD_Y;
  if (x_previous) previous |= MMX_SABER_PAD_X;
  if (previous_y) previous |= MMX_SABER_PAD_Y;
  return (MmxSaberPhysicalPad){buttons, previous};
}

static MmxSaberNativePad read_native_pad(const uint8_t *ram) {
  return (MmxSaberNativePad){
      ram[0x0bde], ram[0x0bdf], ram[0x0be1], ram[0x0be2], ram[0x0be3]};
}

static void write_native_pad(uint8_t *ram, MmxSaberNativePad native) {
  ram[0x0bde] = native.dash_held;
  ram[0x0bdf] = native.action_held;
  ram[0x0be1] = native.fire_previous;
  ram[0x0be2] = native.dash_pressed;
  ram[0x0be3] = native.action_pressed;
}

/* Mirrors native $81:9FE1-$81:A003.  Native runs this before the Saber pad
 * is written, so apply the same repeat cadence to the mapped pad afterward. */
static void mirror_native_fire_wave_repeat(uint8_t *ram, bool fire_blocked) {
  if (fire_blocked || ram[0x0bdb] != 0x08 || !(ram[0x0bdf] & 0x40))
    return;
  if (--ram[0x0c20] == 0) {
    ram[0x0c20] = 0x02;
    ram[0x0be3] |= 0x40;
  }
}

static uint8_t native_horizontal_direction(const uint8_t *ram) {
  return ram ? (uint8_t)(ram[0x0bdf] & MMX_SABER_NATIVE_HORIZONTAL_BITS) : 0;
}

static bool native_wall_clinging(const uint8_t *ram) {
  /* Old src/mmx_saber.c:saber_wall_attack_context gates both wall cling and
   * wall slide on native action $12, before the air/ground priority split. */
  return ram && ram[0x0baa] == 0x12;
}

static bool native_dash_active(const uint8_t *ram) {
  /* Old src/mmx_saber.c:saber_dash_attack_context uses native action $14 as
   * the grounded dash-start gate. */
  return ram && ram[0x0baa] == 0x14;
}

static bool ground_swing(MmxSaberPadSaber saber) {
  return saber.phase != SABER_PHASE_IDLE &&
      (saber.kind == SABER_KIND_GROUND1 ||
       saber.kind == SABER_KIND_GROUND2 ||
       saber.kind == SABER_KIND_GROUND3);
}

static void publish_ground_swing(uint8_t *ram, MmxSaberPadSaber saber) {
  uint8_t facing;
  if (!ram || !ground_swing(saber)) return;

  /* The old pre-player ground suppression wrote the native 16-bit VX word;
   * horizontal pad bits remain owned by the input computation. */
  ram[0x0bc2] = 0;
  ram[0x0bc3] = 0;

  /* $0C11/$0BB9 are the locked facing convention.  The attack module only
   * changes this lock when a swing is accepted, so held direction cannot turn
   * Zero in the middle of a swing. */
  facing = MmxSaberAttackFacing();
  ram[0x0c11] = facing;
  ram[0x0bb9] = (ram[0x0bb9] & (uint8_t)~0x40) | facing;
}

static bool zero_dead_or_reset(const uint8_t *ram) {
  /* The old branch treated native death/reset actions and an empty HP byte as
   * lifecycle cancellation, rather than as a fire/charge input frame. */
  return !(ram[0xbcf] & 127) || ram[0xbaa] == 0x0c || ram[0xbaa] == 0x2c;
}

static void player_end(uint8_t *ram) {
  if (ram) frame_ram = ram;
  MmxSaberHitboxDebugSetRam(ram);
  if (!ram) {
    MmxSaberComboCancel(NULL);
    MmxSaberAttackExit(NULL, MMX_SABER_ATTACK_EXIT_CONTEXT);
    return;
  }
  if (zero_dead_or_reset(ram)) {
    MmxSaberComboCancel(ram);
    MmxSaberWaveRuntimeRetireAll(ram);
    MmxSaberProjectileRetireAll(ram);
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_DEATH);
    return;
  }
  if (ram[0xbaa] == 0x0e || native_fire_locked(ram)) {
    MmxSaberComboCancel(ram);
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_HURT);
    return;
  }
  if (!zero_frame_context(ram)) {
    MmxSaberComboCancel(ram);
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_CONTEXT);
    return;
  }
  MmxSaberPriorityObservePlayerEnd(
      ram, MmxZeroGetState().shot_mask, MmxZeroGetState().burst);
  MmxSaberComboPlayerEnd(ram);
  MmxSaberAttackPlayerEnd(ram);
}

static void pre_player(uint8_t *ram) {
  MmxSaberPadOut out;
  MmxSaberPadSaber saber;
  MmxSaberPadSaber pre_native_saber;
  MmxSaberPadZero zero;
  MmxSaberPhysicalPad physical;
  MmxSaberArmorFlags armor;
  MmxSaberFramePad pad;
  MmxSaberPadKind requested_kind = SABER_KIND_NONE;

  MmxSaberHitboxDebugSetRam(ram);
  frame_ram = ram;
  MmxSaberPriorityObservePrePlayer(
      ram, MmxZeroGetState().shot_mask, MmxZeroGetState().burst);
  if (MmxSaberWaveRuntimeObserveStage(ram))
    MmxSaberComboCancel(ram);
  /* Advance host-owned $5700 objects before native weapon dispatch; the
   * Saber weapon callback claims them so native projectile movement cannot run. */
  MmxSaberProjectileTick(ram);
  clear_frame_state();
  armor = MmxSaberArmorCurrent();
  MmxSaberBlackZeroNativeColorsUpdate(
      armor.black, armor.arms,
      (armor.black || armor.arms) ? MmxZeroColors() : NULL);
  if (!zero_frame_context(ram)) {
    /* This also handles an exchange to X, title/menu frames, and an upstream
     * Zero lifecycle transition. Do not touch any native input byte. */
    MmxSaberComboCancel(ram);
    MmxSaberAttackResetRam(ram);
    release_pending = false;
    pad = sample_pad(ram);
    previous_y = pad.y_pressed;
    previous_x = pad.x_held;
    return;
  }

  /* A special release latch belongs to its special charge.  Do not let a
   * later buster selection turn it into a legacy buster release. */
  if (release_pending_from_special && ram[0x0bdb] == 0) {
    release_pending = false;
    release_pending_from_special = false;
  }

  pad = sample_pad(ram);
  physical = read_physical_pad(ram, pad);
  const bool release_pending_before = release_pending;
  saber = MmxSaberAttackPadState(release_pending);
  pre_native_saber = saber;
  MmxSaberAttackObservePreNative(ram);
  /* A stun locks the Saber like a hit: no swing, no finisher, and the
   * held buster charge is kept for the native state to resolve. */
  const bool locked = native_fire_locked(ram);
  const bool finisher_claimed = MmxSaberComboPrePlayer(
      ram, !locked && (physical.buttons & MMX_SABER_PAD_Y) != 0 &&
          !(physical.prev_buttons & MMX_SABER_PAD_Y));
  const unsigned selected_weapon = ram[0x0bdb];
  const bool physical_x_held =
      (physical.buttons & MMX_SABER_PAD_X) != 0;
  const bool physical_x_pressed = physical_x_held &&
      !(physical.prev_buttons & MMX_SABER_PAD_X);
  const bool grounded = (ram[0x0bd3] & 4) || (ram[0x0bd4] & 4);
  const bool hurt = ram[0x0baa] == 0x0e || locked;
  const bool dead_or_reset = zero_dead_or_reset(ram);
  bool suppress_technique_hold = false;

  if (MmxSaberTuningWeaponTechniques() &&
      weapon_technique_selected(selected_weapon)) {
    suppress_technique_hold = weapon_technique_hold;
    if (!physical_x_held) weapon_technique_hold = false;
  } else {
    weapon_technique_hold = false;
  }

  if (MmxSaberTuningWeaponTechniques() &&
      weapon_technique_selected(selected_weapon) &&
      !hurt && !dead_or_reset && !native_wall_clinging(ram) &&
      physical_x_pressed) {
    if (selected_weapon == SABER_WEAPON_CUTTER &&
        weapon_owned_with_energy(ram, selected_weapon)) {
      suppress_technique_hold = true;
      weapon_technique_hold = true;
      requested_kind = grounded ? SABER_KIND_GROUND2 : SABER_KIND_AIR;
      if (!finisher_claimed && saber.phase == SABER_PHASE_IDLE)
        debit_weapon_energy(ram, selected_weapon, 0x0100);
      if (finisher_claimed) requested_kind = SABER_KIND_NONE;
    } else if (selected_weapon == SABER_WEAPON_SPARK &&
               !grounded && weapon_owned(ram, selected_weapon)) {
      suppress_technique_hold = true;
      weapon_technique_hold = true;
    }
  }

  if (suppress_technique_hold) {
    physical.buttons &= (uint16_t)~MMX_SABER_PAD_X;
    physical.prev_buttons &= (uint16_t)~MMX_SABER_PAD_X;
    clear_special_weapon_latches(ram);
    release_pending = false;
    release_pending_from_special = false;
  }

  /* A Saber-only frame must not create a buster charge. Once X is held, its
   * release edge, or an existing latch, the buster path remains available so
   * charge can continue through the slash and CR1 can fire after recovery. */
  const bool x_charge_owned =
      (physical.buttons & MMX_SABER_PAD_X) != 0 ||
      (physical.prev_buttons & MMX_SABER_PAD_X) != 0 || release_pending;
  zero = (MmxSaberPadZero){
      (ram[0x0bdb] == 0) && x_charge_owned,
      hurt,
      dead_or_reset,
      grounded,
      ram[0x0bdb] != 0 && ram[0x0c00] >= 2};

  /* Classify the physical edge first, advance the Saber owner second, and
   * only then compute/write the native pad.  saber_pressed is phase-independent,
   * so the pad written for this frame already reflects a newly started slash. */
  out = MmxSaberComputePad(physical, read_native_pad(ram), saber, zero);
  MmxSaberApplyUppercutJump(&out.native, saber, pad.y_held);
  if (finisher_claimed) out.saber_pressed = false;
  if (zero.hurt)
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_HURT);
  else if (zero.dead_or_reset)
    MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_DEATH);
  MmxSaberAttackStepWithWallAndDashRequested(
      out.saber_pressed, requested_kind, zero.grounded,
      native_wall_clinging(ram),
      native_dash_active(ram),
      pad.up_held,
      pad.down_held,
      pad.y_held,
      (out.native.action_pressed & MMX_SABER_NATIVE_JUMP_BIT) != 0,
      !zero.hurt && !zero.dead_or_reset, ram, ram[0x0c11],
      native_horizontal_direction(ram));
  MmxSaberAttackRuntimeTick(ram);
  saber = MmxSaberAttackPadState(release_pending);
  /* The attack tick advances the published donor phase before native runs.
   * Preserve the phase that was visible on entry for movement-edge masking,
   * so the last STARTUP frame cannot accept a jump/dash merely because its
   * post-step snapshot is ACTIVE. */
  if (pre_native_saber.phase == SABER_PHASE_STARTUP &&
      saber.phase != SABER_PHASE_IDLE)
    saber.phase = SABER_PHASE_STARTUP;
  out = MmxSaberComputePad(physical, read_native_pad(ram), saber, zero);
  MmxSaberApplyUppercutJump(&out.native, saber, pad.y_held);
  publish_ground_swing(ram, saber);

  /* The computed view is the sole input write for this frame. There is no
   * restore step: the native player and the legacy callback consume it. */
  write_native_pad(ram, out.native);
  mirror_native_fire_wave_repeat(ram, out.fire_blocked || zero.hurt);
  frame_intent = out.legacy;
  frame_override = out.legacy_override;
  frame_computed = true;
  if (!out.release_pending)
    release_pending_from_special = false;
  else if (!release_pending_before && !zero.buster_selected &&
           zero.special_charging)
    release_pending_from_special = true;
  release_pending = out.release_pending;
  previous_y = (physical.buttons & MMX_SABER_PAD_Y) != 0;
  previous_x = pad.x_held;
  last_wrote_input = true;
}

static bool legacy_intent(const uint8_t *ram, MmxZeroLegacyIntent *intent) {
  (void)ram;
  if (!frame_computed || !frame_override || !intent) return false;
  *intent = frame_intent;
  return true;
}

static int burst_origin_y(const uint8_t *ram, unsigned shot_index,
                          int native_y, int paired_y) {
  (void)ram;
  return shot_index == 0 ? paired_y : native_y;
}

static unsigned player_damage(const uint8_t *ram, unsigned incoming,
                              unsigned native) {
  /* Body armor reduces Saber Zero's damage by 25%. */
  if (!ram || !MmxSaberEnabled() ||
      !MmxSaberArmorDecode(ram[MMX_SABER_ARMOR_RAM_OFFSET]).body)
    return native;
  return incoming - incoming / 4;
}

/* "SABR" + layout version, then the frame's own fields and each module's
 * snapshot in a fixed order. A blob without this header leaves the reset
 * state that MmxZeroSetState already established. */
enum { SABER_STATE_MAGIC = 0x52424153u, SABER_STATE_VERSION = 2u };
typedef struct SaberFrameSavedState {
  uint32_t magic, version;
  bool release_pending, release_pending_from_special, previous_y, previous_x;
} SaberFrameSavedState;

static size_t saved_state_size(void) {
  return sizeof(SaberFrameSavedState) + MmxSaberAttackStateSize() +
      MmxSaberComboStateSize() + MmxSaberPriorityStateSize() +
      MmxSaberWaveRuntimeStateSize();
}

static void state_save(uint8_t out[]) {
  SaberFrameSavedState frame;
  uint8_t *cursor = out;
  if (saved_state_size() > MMX_ZERO_EXTENSION_STATE_BYTES) return;
  memset(&frame, 0, sizeof(frame));
  frame.magic = SABER_STATE_MAGIC;
  frame.version = SABER_STATE_VERSION;
  frame.release_pending = release_pending;
  frame.release_pending_from_special = release_pending_from_special;
  frame.previous_y = previous_y;
  frame.previous_x = previous_x;
  memcpy(cursor, &frame, sizeof(frame));
  cursor += sizeof(frame);
  MmxSaberAttackStateSave(cursor);
  cursor += MmxSaberAttackStateSize();
  MmxSaberComboStateSave(cursor);
  cursor += MmxSaberComboStateSize();
  MmxSaberPriorityStateSave(cursor);
  cursor += MmxSaberPriorityStateSize();
  MmxSaberWaveRuntimeStateSave(cursor);
}

static bool state_load(const uint8_t in[]) {
  SaberFrameSavedState frame;
  const uint8_t *cursor = in;
  memcpy(&frame, cursor, sizeof(frame));
  if (frame.magic != SABER_STATE_MAGIC ||
      frame.version != SABER_STATE_VERSION ||
      saved_state_size() > MMX_ZERO_EXTENSION_STATE_BYTES)
    return false;
  reset_frame_state(NULL, false);
  cursor += sizeof(frame);
  release_pending = frame.release_pending;
  release_pending_from_special = frame.release_pending_from_special;
  previous_y = frame.previous_y;
  previous_x = frame.previous_x;
  clear_frame_state();
  MmxSaberAttackStateLoad(cursor);
  cursor += MmxSaberAttackStateSize();
  MmxSaberComboStateLoad(cursor);
  cursor += MmxSaberComboStateSize();
  MmxSaberPriorityStateLoad(cursor);
  cursor += MmxSaberPriorityStateSize();
  MmxSaberWaveRuntimeStateLoad(cursor);
  return true;
}

static const MmxZeroExtension extension = {
    .pre_player = pre_player,
    .player_end = player_end,
    .modern_movement = modern_movement,
    .colors = MmxSaberBlackZeroNativeColorsHook,
    .death_orb_color = MmxSaberBlackZeroDeathOrbColor,
    .legacy_intent = legacy_intent,
    .legacy_slash_request = MmxSaberComboLegacySlashRequest,
    .charge_cap = charge_cap,
    .burst_origin_y = burst_origin_y,
    .response = MmxSaberPriorityResponse,
    .weapon_tick = MmxSaberAttackWeaponTick,
    .damage = MmxSaberAttackDamage,
    .player_damage = player_damage,
    .hitbox = MmxSaberAttackHitbox,
    .collision_rom = MmxSaberAttackCollisionRom,
    .state_reset = state_reset,
    .state_save = state_save,
    .state_load = state_load,
    .charge_rate = charge_rate,
    .weapon_cost = weapon_cost,
};

const MmxZeroExtension *MmxSaberFrameExtension(void) {
  return &extension;
}

const uint8_t *MmxSaberFrameRam(void) {
  return frame_ram;
}

void MmxSaberFrameReset(void) {
  state_reset(NULL);
  MmxSaberHitboxDebugReset();
}

bool MmxSaberFrameLastWroteInput(void) {
  return last_wrote_input;
}

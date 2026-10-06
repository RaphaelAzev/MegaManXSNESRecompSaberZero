#include "mmx_saber_frame.h"

#include "mmx_saber_input.h"

static bool release_pending;
static bool previous_y;
static MmxZeroLegacyIntent frame_intent;
static bool frame_computed;
static bool frame_override;
static bool last_wrote_input;

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
      ram[0x1f10] < 6 && ram[0xbbe] != 0x6b && ram[0xbaa] != 0x2c;
}

static MmxSaberPhysicalPad read_physical_pad(const uint8_t *ram) {
  const bool x_held = (ram[0x00a7] & 0x40) != 0;
  const bool x_previous = (ram[0x00a9] & 0x40) != 0;
  const bool y_held = (ram[0x00ac] & 0x40) != 0;
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

static bool zero_dead_or_reset(const uint8_t *ram) {
  /* The old branch treated native death/reset actions and an empty HP byte as
   * lifecycle cancellation, rather than as a fire/charge input frame. */
  return !(ram[0xbcf] & 127) || ram[0xbaa] == 0x0c || ram[0xbaa] == 0x2c;
}

static void pre_player(uint8_t *ram) {
  MmxSaberPadOut out;
  MmxSaberPadSaber saber;
  MmxSaberPadZero zero;
  MmxSaberPhysicalPad physical;

  clear_frame_state();
  if (!zero_frame_context(ram)) {
    /* This also handles an exchange to X, title/menu frames, and an upstream
     * Zero lifecycle transition. Do not touch any native input byte. */
    release_pending = false;
    previous_y = false;
    return;
  }

  physical = read_physical_pad(ram);
  saber = (MmxSaberPadSaber){SABER_PHASE_IDLE, SABER_KIND_NONE, false, false,
                             release_pending};
  zero = (MmxSaberPadZero){
      ram[0x0bdb] == 0,
      ram[0x0baa] == 0x0e,
      zero_dead_or_reset(ram),
      (ram[0x0bd3] & 4) || (ram[0x0bd4] & 4)};
  out = MmxSaberComputePad(physical, read_native_pad(ram), saber, zero);

  /* The computed view is the sole input write for this frame. There is no
   * restore step: the native player and the legacy callback consume it. */
  write_native_pad(ram, out.native);
  frame_intent = out.legacy;
  frame_override = out.legacy_override;
  frame_computed = true;
  release_pending = out.release_pending;
  previous_y = (physical.buttons & MMX_SABER_PAD_Y) != 0;
  last_wrote_input = true;
}

static bool legacy_intent(const uint8_t *ram, MmxZeroLegacyIntent *intent) {
  (void)ram;
  if (!frame_computed || !frame_override || !intent) return false;
  *intent = frame_intent;
  return true;
}

static const MmxZeroExtension extension = {
    .pre_player = pre_player,
    .legacy_intent = legacy_intent,
};

const MmxZeroExtension *MmxSaberFrameExtension(void) {
  return &extension;
}

void MmxSaberFrameReset(void) {
  release_pending = false;
  previous_y = false;
  clear_frame_state();
}

bool MmxSaberFrameLastWroteInput(void) {
  return last_wrote_input;
}

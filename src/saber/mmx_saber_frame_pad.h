#pragma once

#include <stdint.h>

#include "mmx_saber_frame.h"

/* Co-op stores seat input bit n at joypad bit 15-n: Up is bit 4, Y is bit 1,
 * and Down (joypad bit 10) is bit 5. `live` matches the plugin's initialized
 * co-op state gate; X/Y physical buttons are supplied by MmxCoopSeatButtons. */
static inline void MmxSaberFrameApplyCoopHeldPad(MmxSaberFramePad *pad,
                                                  bool live,
                                                  uint16_t seat_input) {
  if (!pad) return;
  pad->up_held = live && (seat_input & (1u << 4)) != 0;
  pad->y_held = live && (seat_input & (1u << 1)) != 0;
  pad->down_held = live && (seat_input & (1u << 5)) != 0;
}

static inline MmxSaberFramePad MmxSaberFrameSampleNativePad(const uint8_t *ram) {
  MmxSaberFramePad pad = {false, false, false, false, false};
  if (!ram) return pad;
  /* Held word $A7 (low: A X L R) / $A8 (high: B Y Select Start Up Down Left
   * Right), measured on the ROM: X = $A7 $40, Y = $A8 $40, Up = $A8 $08,
   * Down = $A8 $04. */
  pad.x_held = (ram[0x00a7] & 0x40) != 0;
  pad.y_pressed = (ram[0x00ac] & 0x40) != 0;
  pad.up_held = (ram[0x00a8] & 0x08) != 0;
  pad.y_held = (ram[0x00a8] & 0x40) != 0;
  pad.down_held = (ram[0x00a8] & 0x04) != 0;
  return pad;
}

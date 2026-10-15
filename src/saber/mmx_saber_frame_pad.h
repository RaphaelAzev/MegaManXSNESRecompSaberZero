#pragma once

#include "mmx_saber_frame.h"

static inline MmxSaberFramePad MmxSaberFrameSampleNativePad(const uint8_t *ram) {
  MmxSaberFramePad pad = {false, false, false, false};
  if (!ram) return pad;
  /* Held word $A7 (low: A X L R) / $A8 (high: B Y Select Start Up Down Left
   * Right), measured on the ROM: X = $A7 $40, Y = $A8 $40, Up = $A8 $08. */
  pad.x_held = (ram[0x00a7] & 0x40) != 0;
  pad.y_pressed = (ram[0x00ac] & 0x40) != 0;
  pad.up_held = (ram[0x00a8] & 0x08) != 0;
  pad.y_held = (ram[0x00a8] & 0x40) != 0;
  return pad;
}

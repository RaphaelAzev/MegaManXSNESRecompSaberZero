#pragma once

#include "mmx_saber_frame.h"

static inline MmxSaberFramePad MmxSaberFrameSampleNativePad(const uint8_t *ram) {
  MmxSaberFramePad pad = {false, false, false, false};
  if (!ram) return pad;
  /* $A6/$A7 is SNES high/low: Y=$40 and Up=$08 in high $A6; X=$40 in low $A7. */
  pad.x_held = (ram[0x00a7] & 0x40) != 0;
  pad.y_pressed = (ram[0x00ac] & 0x40) != 0;
  pad.up_held = (ram[0x00a6] & 0x08) != 0;
  pad.y_held = (ram[0x00a6] & 0x40) != 0;
  return pad;
}

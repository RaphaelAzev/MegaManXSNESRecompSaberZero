#pragma once

#include <stdbool.h>

#include "../mmx_zero.h"

/* The Saber-owned per-frame bridge registered at Zero's pre-player seam. */
const MmxZeroExtension *MmxSaberFrameExtension(void);
void MmxSaberFrameReset(void);

/* Live RAM retained by the Saber-owned player seams for presentation reads. */
const uint8_t *MmxSaberFrameRam(void);

/* Test-only observability for the deliberate X pass-through path. */
bool MmxSaberFrameLastWroteInput(void);

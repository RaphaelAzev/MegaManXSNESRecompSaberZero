#pragma once

#include <stdbool.h>

#include "../mmx_zero.h"

/* The Saber-owned per-frame bridge registered at Zero's pre-player seam. */
const MmxZeroExtension *MmxSaberFrameExtension(void);
void MmxSaberFrameReset(void);

/* Test-only observability for the deliberate X pass-through path. */
bool MmxSaberFrameLastWroteInput(void);

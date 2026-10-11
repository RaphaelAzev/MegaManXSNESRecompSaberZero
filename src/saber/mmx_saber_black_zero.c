#include "mmx_saber_black_zero.h"

#include <stddef.h>
#include <string.h>

typedef struct MmxSaberBlackZeroColor {
  uint16_t source;
  uint16_t target;
} MmxSaberBlackZeroColor;

/* Colours come from the MMX3 Zero Project romhack's Black Zero palette
 * (X3 bank $8C, $A840). No ROM data is embedded beyond these constants. */
static const MmxSaberBlackZeroColor k_black_zero_colors[] = {
  {0x009C, 0x2D6B},
  {0x0094, 0x1CE7},
  {0x008C, 0x0CA5},
  {0x031C, 0x4EF9},
  {0x0194, 0x2DD1},
  {0x1084, 0x0C63},
  {0x0092, 0x18C6},
  {0x008D, 0x10A5},
  {0x008A, 0x0C84},
  {0x006A, 0x0863},
  {0x010D, 0x214C},
};

static const uint16_t *native_source;
static uint16_t native_colors[128];
static bool native_black;

void MmxSaberBlackZeroPalette(const uint16_t *src, unsigned count,
                              uint16_t *dst) {
  if (!src || !dst) return;
  for (unsigned i = 0; i < count; ++i) {
    uint16_t color = src[i];
    for (unsigned j = 0;
         j < sizeof(k_black_zero_colors) / sizeof(k_black_zero_colors[0]);
         ++j) {
      if (color == k_black_zero_colors[j].source) {
        color = k_black_zero_colors[j].target;
        break;
      }
    }
    dst[i] = color;
  }
}

void MmxSaberBlackZeroNativeColors(const uint16_t src[128],
                                   uint16_t dst[128]) {
  if (!src || !dst) return;
  memcpy(dst, src, 128 * sizeof(*dst));
  dst[23] = 0x2D6B;
  dst[24] = 0x1CE7;
  dst[25] = 0x0CA5;
  dst[29] = 0x4EF9;
  dst[30] = 0x2DD1;
  dst[31] = 0x0C63;
}

void MmxSaberBlackZeroNativeColorsUpdate(bool black,
                                         const uint16_t *native) {
  if (!black || !native) {
    native_source = NULL;
    native_black = false;
    return;
  }
  if (native_black && native == native_colors) return;
  if (!native_black || native_source != native) {
    MmxSaberBlackZeroNativeColors(native, native_colors);
    native_source = native;
  }
  native_black = true;
}

const uint16_t *MmxSaberBlackZeroNativeColorsHook(const uint16_t *native) {
  return native_black && native == native_source ? native_colors : NULL;
}

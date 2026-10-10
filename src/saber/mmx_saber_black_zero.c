#include "mmx_saber_black_zero.h"

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

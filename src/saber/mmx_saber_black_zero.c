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
static bool native_arms;

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

static uint16_t black_zero_death_orb_grey(uint16_t color) {
  unsigned red = color & 31u;
  unsigned green = (color >> 5) & 31u;
  unsigned blue = (color >> 10) & 31u;
  if (red <= green || red <= blue) return color;
  /* The X asset's red-tinted ramp is 0x3DFE, 0x295E, 0x001F.
   * Match its non-red channel brightness to Black Zero's light, mid and dark
   * armour greys. Exact table values are handled before this fallback. */
  unsigned brightness = green + blue;
  return brightness >= 26 ? 0x2D6B : brightness >= 16 ? 0x1CE7 : 0x0CA5;
}

uint16_t MmxSaberBlackZeroDeathOrbColor(uint16_t native) {
  uint16_t mapped;
  if (!native_black) return native;
  MmxSaberBlackZeroPalette(&native, 1, &mapped);
  return mapped != native ? mapped : black_zero_death_orb_grey(native);
}

uint16_t MmxSaberPurpleBlade(uint16_t bgr555) {
  unsigned red = bgr555 & 31u;
  unsigned green = (bgr555 >> 5) & 31u;
  unsigned blue = (bgr555 >> 10) & 31u;
  unsigned new_red = (green * 3u) / 4u;
  unsigned new_green = red < blue ? red : blue;
  return (uint16_t)(new_red | (new_green << 5) | (green << 10));
}

unsigned MmxSaberPurpleBladePalette(const uint16_t *src, unsigned count,
                                    const uint8_t *blade_indices,
                                    const uint8_t *body_indices,
                                    uint16_t *dst) {
  unsigned shared = 0;
  if (!src || !dst) return 0;
  if (count > 256u) count = 256u;
  for (unsigned i = 0; i < count; ++i) {
    dst[i] = src[i];
    if (i && blade_indices && blade_indices[i]) {
      if (body_indices && body_indices[i])
        ++shared;
      else
        dst[i] = MmxSaberPurpleBlade(src[i]);
    }
  }
  return shared;
}

static bool green_hued(uint16_t bgr555) {
  unsigned red = bgr555 & 31u;
  unsigned green = (bgr555 >> 5) & 31u;
  unsigned blue = (bgr555 >> 10) & 31u;
  return green > red && green > blue;
}

static void purple_native_range(uint16_t dst[128], unsigned first) {
  for (unsigned i = first; i < first + 16; ++i)
    if (green_hued(dst[i])) dst[i] = MmxSaberPurpleBlade(dst[i]);
}

void MmxSaberBlackZeroNativeColors(const uint16_t src[128], bool black,
                                   bool arms, uint16_t dst[128]) {
  if (!src || !dst) return;
  memcpy(dst, src, 128 * sizeof(*dst));
  if (black) {
    dst[23] = 0x2D6B;
    dst[24] = 0x1CE7;
    dst[25] = 0x0CA5;
    dst[29] = 0x4EF9;
    dst[30] = 0x2DD1;
    dst[31] = 0x0C63;
  }
  if (arms) {
    purple_native_range(dst, 16);
    purple_native_range(dst, 48);
  }
}

void MmxSaberBlackZeroNativeColorsUpdate(bool black, bool arms,
                                         const uint16_t *native) {
  const uint16_t *source = native;
  if ((!black && !arms) || !native) {
    native_source = NULL;
    native_black = false;
    native_arms = false;
    return;
  }
  /* MmxZeroColors() returns this cache after the first replacement. Keep the
   * raw table pointer as the cache key when that happens. */
  if (native == native_colors) source = native_source;
  if (!source) {
    native_source = NULL;
    native_black = false;
    native_arms = false;
    return;
  }
  if (native_source == source && native_black == black &&
      native_arms == arms) return;
  MmxSaberBlackZeroNativeColors(source, black, arms, native_colors);
  native_source = source;
  native_black = black;
  native_arms = arms;
}

const uint16_t *MmxSaberBlackZeroNativeColorsHook(const uint16_t *native) {
  return (native_black || native_arms) && native == native_source ?
      native_colors : NULL;
}

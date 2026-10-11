#include "mmx_saber_black_zero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int ok, const char *message) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
  }
  printf("ok: %s\n", message);
}


static void purple_blade_values(void) {
  check(MmxSaberPurpleBlade(0x2388) == 0x7115,
        "purple blade maps 0x2388 to 0x7115");
  check(MmxSaberPurpleBlade(0x42A5) == 0x54AF,
        "purple blade maps 0x42A5 to 0x54AF");
  check(MmxSaberPurpleBlade(0x1CE7) == 0x1CE5,
        "purple blade maps 0x1CE7 to 0x1CE5");
  check(MmxSaberPurpleBlade(0x0000) == 0x0000,
        "purple blade leaves 0x0000 transparent");
}

static void purple_blade_palette(void) {
  static const uint16_t source[] = {
    0x0000, 0x2388, 0x1CE7, 0x42A5, 0x7FFF,
  };
  static const uint8_t blade_indices[] = {0, 1, 0, 1, 1};
  static const uint8_t body_indices[] = {0, 0, 1, 0, 1};
  static const uint16_t expected[] = {
    0x0000, 0x7115, 0x1CE7, 0x54AF, 0x7FFF,
  };
  uint16_t actual[sizeof(source) / sizeof(source[0])];
  unsigned shared = MmxSaberPurpleBladePalette(
      source, sizeof(source) / sizeof(source[0]), blade_indices,
      body_indices, actual);
  check(shared == 1,
        "purple blade palette reports one shared blade index");
  check(!memcmp(actual, expected, sizeof(expected)),
        "purple blade palette recolours only listed non-shared indices");
}
static void native_color_indices(void) {
  uint16_t source[128], actual[128];
  for (unsigned i = 0; i < 128; ++i) {
    source[i] = (uint16_t)(0x4000 + i);
    actual[i] = 0x7FFF;
  }
  MmxSaberBlackZeroNativeColors(source, true, false, actual);
  for (unsigned i = 0; i < 128; ++i) {
    uint16_t expected = source[i];
    if (i == 23) expected = 0x2D6B;
    if (i == 24) expected = 0x1CE7;
    if (i == 25) expected = 0x0CA5;
    if (i == 29) expected = 0x4EF9;
    if (i == 30) expected = 0x2DD1;
    if (i == 31) expected = 0x0C63;
    check(actual[i] == expected,
          "native Black Zero changes only body indices 23, 24, 25, 29, 30, and 31");
  }
}

static void native_color_modes(void) {
  uint16_t source[128];
  const uint16_t green_body = 0x1282;
  const uint16_t green_blade = 0x0F06;
  const uint16_t *actual;

  for (unsigned i = 0; i < 128; ++i) source[i] = (uint16_t)(0x4000 + i);
  for (unsigned i = 16; i < 32; ++i) source[i] = green_body;
  for (unsigned i = 48; i < 64; ++i) source[i] = green_blade;
  source[15] = green_blade;
  source[64] = green_body;
  source[80] = green_body;
  source[23] = 0x009C;
  source[24] = 0x0094;
  source[25] = 0x008C;
  source[29] = 0x031C;
  source[30] = 0x0194;
  source[31] = 0x1084;

  MmxSaberBlackZeroNativeColorsUpdate(false, true, source);
  actual = MmxSaberBlackZeroNativeColorsHook(source);
  check(actual != NULL, "native arms-only hook returns cached colours");
  for (unsigned i = 16; i < 32; ++i) {
    if (i == 23 || i == 24 || i == 25 || i == 29 || i == 30 || i == 31)
      continue;
    check(actual[i] == MmxSaberPurpleBlade(source[i]),
          "arms recolour every green body-range entry");
  }
  for (unsigned i = 48; i < 64; ++i)
    check(actual[i] == MmxSaberPurpleBlade(source[i]),
          "arms recolour every green wave-saber entry");
  check(actual[15] == source[15] && actual[64] == source[64] &&
            actual[80] == source[80],
        "arms leave out-of-range green entries unchanged");
  check(actual[23] == source[23] && actual[24] == source[24] &&
            actual[25] == source[25] && actual[29] == source[29] &&
            actual[30] == source[30] && actual[31] == source[31],
        "arms leave Black Zero body entries unchanged");

  MmxSaberBlackZeroNativeColorsUpdate(true, false, source);
  actual = MmxSaberBlackZeroNativeColorsHook(source);
  check(actual != NULL, "native black-only hook returns cached colours");
  for (unsigned i = 16; i < 32; ++i) {
    if (i == 23 || i == 24 || i == 25 || i == 29 || i == 30 || i == 31)
      continue;
    check(actual[i] == source[i], "black-only leaves body greens unchanged");
  }
  for (unsigned i = 48; i < 64; ++i)
    check(actual[i] == source[i], "black-only leaves wave greens unchanged");
  check(actual[15] == source[15] && actual[64] == source[64] &&
            actual[80] == source[80],
        "black-only leaves out-of-range greens unchanged");
  check(actual[23] == 0x2D6B && actual[24] == 0x1CE7 &&
            actual[25] == 0x0CA5 && actual[29] == 0x4EF9 &&
            actual[30] == 0x2DD1 && actual[31] == 0x0C63,
        "black-only changes six body entries");

  MmxSaberBlackZeroNativeColorsUpdate(true, true, source);
  actual = MmxSaberBlackZeroNativeColorsHook(source);
  check(actual != NULL, "native black-and-arms hook returns cached colours");
  for (unsigned i = 16; i < 32; ++i) {
    if (i == 23 || i == 24 || i == 25 || i == 29 || i == 30 || i == 31)
      continue;
    check(actual[i] == MmxSaberPurpleBlade(source[i]),
          "black-and-arms recolours body greens");
  }
  for (unsigned i = 48; i < 64; ++i)
    check(actual[i] == MmxSaberPurpleBlade(source[i]),
          "black-and-arms recolours wave greens");
  check(actual[23] == 0x2D6B && actual[24] == 0x1CE7 &&
            actual[25] == 0x0CA5 && actual[29] == 0x4EF9 &&
            actual[30] == 0x2DD1 && actual[31] == 0x0C63,
        "black-and-arms keeps six body entries black");

  MmxSaberBlackZeroNativeColorsUpdate(false, false, source);
  check(!MmxSaberBlackZeroNativeColorsHook(source),
        "native hook returns NULL with neither native colour flag");
}

static void native_cache_lifecycle(void) {
  uint16_t source[128];
  for (unsigned i = 0; i < 128; ++i) source[i] = (uint16_t)(0x5000 + i);
  MmxSaberBlackZeroNativeColorsUpdate(true, false, source);
  const uint16_t *replacement = MmxSaberBlackZeroNativeColorsHook(source);
  check(replacement && replacement != source && replacement[23] == 0x2D6B,
        "native color hook returns cached Black Zero colours");
  uint16_t changed[128];
  for (unsigned i = 0; i < 128; ++i) changed[i] = (uint16_t)(0x6000 + i);
  MmxSaberBlackZeroNativeColorsUpdate(true, false, changed);
  replacement = MmxSaberBlackZeroNativeColorsHook(changed);
  check(replacement && replacement[0] == 0x6000 && replacement[23] == 0x2D6B,
        "native color hook rebuilds when native table pointer changes");
  MmxSaberBlackZeroNativeColorsUpdate(false, false, NULL);
  check(!MmxSaberBlackZeroNativeColorsHook(source),
        "native color hook clears after Black Zero is disabled");
}

int main(void) {
  static const uint16_t source[] = {
    0x0000, 0x1284, 0x0B08,
    0x009C, 0x0094, 0x008C, 0x031C, 0x0194, 0x1084,
    0x0092, 0x008D, 0x008A, 0x006A, 0x010D,
  };
  static const uint16_t expected[] = {
    0x0000, 0x1284, 0x0B08,
    0x2D6B, 0x1CE7, 0x0CA5, 0x4EF9, 0x2DD1, 0x0C63,
    0x18C6, 0x10A5, 0x0C84, 0x0863, 0x214C,
  };
  uint16_t actual[sizeof(source) / sizeof(source[0]) + 1];
  unsigned count = (unsigned)(sizeof(source) / sizeof(source[0]));

  purple_blade_values();
  purple_blade_palette();
  native_color_indices();
  native_color_modes();
  native_cache_lifecycle();
  for (unsigned i = 0; i < sizeof(actual) / sizeof(actual[0]); ++i)
    actual[i] = 0x7FFF;
  MmxSaberBlackZeroPalette(source, count, actual);
  check(!memcmp(actual, expected, sizeof(expected)),
        "every Black Zero replacement and every non-table colour matches");
  check(actual[count] == 0x7FFF,
        "palette conversion respects count and does not write past it");
  puts("MMX SABER BLACK ZERO CHECKS PASSED");
  return 0;
}

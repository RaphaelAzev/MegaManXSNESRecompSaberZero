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

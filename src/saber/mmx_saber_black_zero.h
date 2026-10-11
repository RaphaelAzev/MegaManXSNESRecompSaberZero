#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MmxSaberBlackZeroPalette(const uint16_t *src, unsigned count,
                              uint16_t *dst);
void MmxSaberBlackZeroNativeColors(const uint16_t src[128],
                                   uint16_t dst[128]);
void MmxSaberBlackZeroNativeColorsUpdate(bool black,
                                         const uint16_t *native);
const uint16_t *MmxSaberBlackZeroNativeColorsHook(const uint16_t *native);

#ifdef __cplusplus
}
#endif

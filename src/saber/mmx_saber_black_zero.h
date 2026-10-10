#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MmxSaberBlackZeroPalette(const uint16_t *src, unsigned count,
                              uint16_t *dst);

#ifdef __cplusplus
}
#endif

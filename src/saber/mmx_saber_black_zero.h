#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MmxSaberBlackZeroPalette(const uint16_t *src, unsigned count,
                              uint16_t *dst);
uint16_t MmxSaberPurpleBlade(uint16_t bgr555);
/* `blade_indices` and `body_indices` are count-sized masks. Returns the
 * number of nonzero blade indices also referenced by body pixels. */
unsigned MmxSaberPurpleBladePalette(const uint16_t *src, unsigned count,
                                    const uint8_t *blade_indices,
                                    const uint8_t *body_indices,
                                    uint16_t *dst);
void MmxSaberBlackZeroNativeColors(const uint16_t src[128],
                                   bool black, bool arms,
                                   uint16_t dst[128]);
void MmxSaberBlackZeroNativeColorsUpdate(bool black, bool arms,
                                         const uint16_t *native);
const uint16_t *MmxSaberBlackZeroNativeColorsHook(const uint16_t *native);
uint16_t MmxSaberBlackZeroDeathOrbColor(uint16_t native);

#ifdef __cplusplus
}
#endif

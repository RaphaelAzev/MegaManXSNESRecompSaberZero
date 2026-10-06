#pragma once

#include <stdbool.h>

#include "mmx_renderer.h"
#include "mmx_saber_assets.h"
#include "mmx_saber_attack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve one explicit attack snapshot. This is also the independent seam used
 * by the live resolver below and by the Saber-owned sidecar tests. */
bool MmxSaberRenderResolveSnapshot(const MmxSaberAssets *assets,
                                   MmxSaberAttackSnapshot snapshot,
                                   MmxRenderPlayerOverlay *out);

/* Resolve the current live Saber attack state against the loaded sidecar. */
bool MmxSaberRenderResolve(const MmxSaberAssets *assets,
                           MmxRenderPlayerOverlay *out);

#ifdef __cplusplus
}
#endif

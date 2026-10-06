#include "mmx_saber_render.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MMX_SABER_RENDER_CACHE_DIR
#define MMX_SABER_RENDER_CACHE_DIR ""
#endif

/* The isolated resolver target does not link the full Zero controller. */
bool MmxZeroActive(void) { return true; }

static const uint8_t kSaberManifestSha[32] = {
  0x4e, 0x29, 0x1e, 0x5f, 0x03, 0x57, 0xaf, 0xa0,
  0x35, 0x76, 0x14, 0xe0, 0xc4, 0x97, 0xf9, 0xb3,
  0x65, 0xee, 0x99, 0xe3, 0x70, 0x64, 0x5d, 0x84,
  0x99, 0xb2, 0xe4, 0xfd, 0x07, 0xf8, 0x0f, 0xd0,
};

typedef struct OldRenderTuple {
  uint8_t tick;
  uint8_t step;
  uint8_t source_frame;
  int16_t body_x;
  int16_t body_y;
  uint16_t body_width;
  uint16_t body_height;
} OldRenderTuple;

/* Independent oracle copied from oldsaber/saber-zero-variant:src/mmx_saber.c:
 * 1351-1365. The tuples are the sidecar frame/step, origin, and dimensions
 * selected by that old walk; they are deliberately not obtained from the
 * resolver under test. The old compositor's plane contract is at
 * oldsaber/saber-zero-variant:src/mmx_renderer.c:1199-1215. */
static const OldRenderTuple kOldTuples[7][4] = {
  {
    {0, 0, 0, 46, 44, 37, 44},
    {29, 14, 14, 44, 44, 39, 44},
    {30, 14, 14, 44, 44, 39, 44},
    {35, 14, 14, 44, 44, 39, 44},
  },
  {
    {0, 0, 0, 46, 49, 47, 39},
    {29, 14, 14, 44, 44, 39, 44},
    {30, 14, 14, 44, 44, 39, 44},
    {35, 14, 14, 44, 44, 39, 44},
  },
  {
    {0, 0, 0, 44, 44, 39, 44},
    {30, 15, 15, 43, 44, 41, 44},
    {38, 17, 17, 45, 44, 39, 44},
    {44, 17, 17, 45, 44, 39, 44},
  },
  {
    {0, 0, 0, 37, 37, 40, 51},
    {17, 8, 8, 50, 23, 29, 64},
    {18, 8, 8, 50, 23, 29, 64},
    {23, 8, 8, 50, 23, 29, 64},
  },
  {
    {0, 0, 9, 48, 38, 40, 50},
    {19, 9, 0, 48, 38, 35, 50},
    {20, 9, 0, 48, 38, 35, 50},
    {25, 9, 0, 48, 38, 35, 50},
  },
  {
    {0, 0, 0, 46, 55, 51, 35},
    {29, 14, 14, 44, 44, 39, 44},
    {30, 14, 14, 44, 44, 39, 44},
    {35, 14, 14, 44, 44, 39, 44},
  },
  {
    {0, 0, 0, 36, 43, 41, 45},
    {17, 8, 8, 43, 44, 37, 44},
    {18, 8, 8, 43, 44, 37, 44},
    {23, 8, 8, 43, 44, 37, 44},
  },
};

static const uint8_t kTotals[7] = {30, 30, 39, 18, 20, 30, 18};
static const MmxSaberPadKind kKinds[7] = {
  SABER_KIND_GROUND1, SABER_KIND_GROUND2, SABER_KIND_GROUND3,
  SABER_KIND_AIR, SABER_KIND_WALL, SABER_KIND_DASH,
  SABER_KIND_SABER_LAND,
};
static const uint8_t kIndices[7] = {0, 1, 2, 0, 0, 0, 0};

static void check(int ok, const char *message) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
  }
  printf("ok: %s\n", message);
}

static int join_path(char *out, size_t out_size, const char *directory,
                     const char *name) {
  int written = snprintf(out, out_size, "%s/%s", directory, name);
  return written >= 0 && (size_t)written < out_size;
}

static int check_plane(const MmxRenderPlayerOverlayPlane *actual,
                       const MmxSaberPlane *expected) {
  return actual->pixels == expected->pixels &&
      actual->width == expected->width && actual->height == expected->height &&
      actual->origin_x == expected->origin_x &&
      actual->origin_y == expected->origin_y;
}

static void check_tuple(const MmxSaberAssets *assets, unsigned animation_index,
                        const OldRenderTuple *expected) {
  const uint16_t animation_id = (uint16_t)(animation_index + 1);
  const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(
      assets, animation_id, expected->step);
  MmxSaberAttackSnapshot snapshot = {
    kKinds[animation_index], kIndices[animation_index], SABER_PHASE_ACTIVE,
    expected->tick, (uint8_t)animation_id, 0, 0x40};
  MmxRenderPlayerOverlay actual;

  check(frame != NULL, "old tuple resolves to a sidecar frame");
  check(frame->source_id == animation_id &&
            frame->source_frame_id == expected->source_frame &&
            frame->body.origin_x == expected->body_x &&
            frame->body.origin_y == expected->body_y &&
            frame->body.width == expected->body_width &&
            frame->body.height == expected->body_height &&
            frame->blade.pixels == NULL && frame->blade.width == 0 &&
            frame->blade.height == 0 && frame->blade.origin_x == 0 &&
            frame->blade.origin_y == 0 && frame->blade_layer == 0,
        "hard-coded old sidecar tuple is unchanged");
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual),
        "Saber resolver accepts the live-shaped attack snapshot");
  check(actual.active && check_plane(&actual.body, &frame->body) &&
            check_plane(&actual.blade, &frame->blade) &&
            actual.blade_layer == frame->blade_layer &&
            actual.palette == MmxSaberAssetsPalette(assets) &&
            actual.palette_count == MmxSaberAssetsPaletteCount(assets) &&
            !actual.facing_left,
        "resolver returns the exact donor planes, palette, layer, and facing");
}

int main(void) {
  char path[4096];
  char reason[128] = {0};
  MmxSaberAssets *assets;

  if (!MMX_SABER_RENDER_CACHE_DIR[0] ||
      !join_path(path, sizeof(path), MMX_SABER_RENDER_CACHE_DIR,
                 "saber-v1.bin")) {
    fprintf(stderr, "FAIL: Saber render cache directory is not configured\n");
    return 1;
  }
  FILE *file = fopen(path, "rb");
  if (!file) {
    printf("SKIPPED: private Saber sidecar is absent (%s)\n", path);
    return 77;
  }
  fclose(file);

  assets = MmxSaberAssetsLoadFile(path, kSaberManifestSha,
                                  reason, sizeof(reason));
  if (!assets) {
    fprintf(stderr, "FAIL: Saber sidecar rejected: %s\n",
            reason[0] ? reason : "unknown reason");
    return 1;
  }

  for (unsigned animation = 0; animation < 7; ++animation)
    for (unsigned tuple = 0; tuple < 4; ++tuple)
      check_tuple(assets, animation, &kOldTuples[animation][tuple]);

  MmxSaberAttackReset();
  MmxSaberAttackStep(true, true, true, 0, 0);
  MmxRenderPlayerOverlay live;
  check(MmxSaberRenderResolve(assets, &live) && live.active &&
            live.body.origin_x == 46 && live.body.origin_y == 44,
        "live attack state resolves ground animation 1");
  MmxSaberAttackReset();
  check(!MmxSaberRenderResolve(assets, &live) && !live.active,
        "idle attack state resolves inactive");
  check(!MmxSaberRenderResolve(NULL, &live) && !live.active,
        "missing sidecar resolves inactive");

  MmxSaberAssetsFree(assets);
  puts("MMX SABER RENDER CHECKS PASSED");
  return 0;
}

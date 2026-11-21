#include "mmx_saber_render.h"
#include "mmx_saber_armor.h"
#include "mmx_saber_black_zero.h"
#include "mmx_saber_wave_runtime.h"
#include "mmx_saber_tuning.h"
#include "mmx_zero.h"
#include "sha256.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MMX_SABER_RENDER_CACHE_DIR
#define MMX_SABER_RENDER_CACHE_DIR ""
#endif

/* The isolated resolver target does not link the full Zero controller. These
 * doubles expose the same native body-palette query used by the provider. */
static MmxZeroState test_zero_state;
static uint16_t test_zero_colors[128];
static uint16_t test_charge_colors[3][16];
static bool test_arms;
static bool test_black_zero;

bool MmxZeroActive(void) { return true; }
/* The render test links the attack module without mmx_zero.c. */
unsigned test_air_refresh_calls;
void MmxZeroRefreshAirActions(void) { ++test_air_refresh_calls; }
MmxSaberArmorFlags MmxSaberArmorCurrent(void) {
  MmxSaberArmorFlags flags = {0};
  flags.black = test_black_zero;
  flags.arms = test_arms;
  return flags;
}
MmxZeroState MmxZeroGetState(void) { return test_zero_state; }
void MmxZeroSelectState(MmxZeroState state) { test_zero_state = state; }
const uint16_t *MmxZeroColors(void) { return test_zero_colors; }
int MmxZeroChargeFlashPaletteIndex(const MmxZeroState *s) {
  if (!s || s->active_x || s->swap_phase || s->slash ||
      (s->charge < 25 && !(s->combo && s->saber_ready)) || (s->charge_phase & 2))
    return -1;
  return s->saber_ready || s->charge >= 201 ? 2 : s->charge >= 141 ? 1 : 0;
}
const uint16_t *MmxZeroBodyColors(const MmxZeroState *s) {
  int index = MmxZeroChargeFlashPaletteIndex(s);
  return index < 0 ? test_zero_colors + 16 : test_charge_colors[index];
}

static const uint8_t kSaberManifestSha[32] = {
  0xca, 0xed, 0x42, 0xcb, 0x0b, 0xa6, 0xe3, 0xe2,
 0xa1, 0xc9, 0x67, 0x26, 0x4d, 0xd8, 0x11, 0x80,
 0xa6, 0x90, 0x5e, 0x2b, 0xf2, 0xac, 0x8e, 0x39,
 0x70, 0xb7, 0x8c, 0x78, 0x03, 0x02, 0x4b, 0xd8,
};

static const uint8_t kRideManifestSha[32] = {
  0xdf, 0x56, 0x36, 0x93, 0x59, 0x9e, 0x7d, 0x4d,
  0x37, 0xcb, 0xb7, 0x28, 0x7c, 0x4d, 0x95, 0x3e,
  0xbf, 0x23, 0xdb, 0x14, 0xa5, 0xc4, 0xbd, 0x29,
  0x91, 0x91, 0xf8, 0xf4, 0xb1, 0xff, 0x7d, 0xac,
};

static const uint16_t kRideSourceFrames[23] = {
  4, 35, 36, 7, 8, 9, 19, 14, 15, 16, 5, 6,
  26, 21, 22, 23, 1, 2, 3, 28, 30, 31, 32,
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

static void put_u16(uint8_t *bytes, unsigned offset, unsigned value) {
  bytes[offset] = (uint8_t)value;
  bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *bytes, unsigned offset, unsigned value) {
  put_u16(bytes, offset, value);
  put_u16(bytes, offset + 2, value >> 16);
}

typedef struct TestOpaqueRect {
  int min_x, min_y, max_x, max_y;
} TestOpaqueRect;

static void include_opaque_pixel(TestOpaqueRect *rect, int x, int y,
                                 bool *found) {
  if (!*found) {
    rect->min_x = x;
    rect->max_x = x + 1;
    rect->min_y = y;
    rect->max_y = y + 1;
    *found = true;
    return;
  }
  if (x < rect->min_x) rect->min_x = x;
  if (x + 1 > rect->max_x) rect->max_x = x + 1;
  if (y < rect->min_y) rect->min_y = y;
  if (y + 1 > rect->max_y) rect->max_y = y + 1;
}

/* Same final world-space mapping as src/mmx_renderer.c:2230-2257. */
static bool compose_world_opaque_rect(const MmxRenderWorldSprite *sprite,
                                      TestOpaqueRect *rect) {
  bool found = false;
  if (!sprite || !rect || !sprite->pixels) return false;
  for (unsigned row = 0; row < sprite->height; ++row) {
    for (unsigned col = 0; col < sprite->width; ++col) {
      unsigned pixel = sprite->pixels[row * sprite->width + col];
      int world_x;
      int world_y;
      if (!pixel || pixel >= sprite->palette_count) continue;
      world_x = sprite->world_x + (sprite->facing_left ?
          -1 - sprite->origin_x - (int)col :
          sprite->origin_x + (int)col);
      world_y = sprite->world_y + sprite->origin_y + (int)row;
      include_opaque_pixel(rect, world_x, world_y, &found);
    }
  }
  return found;
}

static bool compose_world_has_opaque_pixel(const MmxRenderWorldSprite *sprite,
                                           int target_x, int target_y) {
  if (!sprite || !sprite->pixels) return false;
  for (unsigned row = 0; row < sprite->height; ++row) {
    for (unsigned col = 0; col < sprite->width; ++col) {
      unsigned pixel = sprite->pixels[row * sprite->width + col];
      int world_x;
      int world_y;
      if (!pixel || pixel >= sprite->palette_count) continue;
      world_x = sprite->world_x + (sprite->facing_left ?
          -1 - sprite->origin_x - (int)col :
          sprite->origin_x + (int)col);
      world_y = sprite->world_y + sprite->origin_y + (int)row;
      if (world_x == target_x && world_y == target_y) return true;
    }
  }
  return false;
}

/* Same player-overlay mapping as src/mmx_renderer.c:2216-2223. */
static bool compose_overlay_opaque_rect(
    const MmxRenderPlayerOverlayPlane *plane, bool facing_left, int zero_x,
    int zero_y, unsigned palette_count, TestOpaqueRect *rect) {
  bool found = false;
  if (!plane || !rect || !plane->pixels) return false;
  for (unsigned row = 0; row < plane->height; ++row) {
    for (unsigned col = 0; col < plane->width; ++col) {
      unsigned pixel = plane->pixels[row * plane->width + col];
      int canvas_col;
      int x;
      int y;
      if (!pixel || pixel >= palette_count) continue;
      canvas_col = plane->origin_x + (int)col;
      x = zero_x + (facing_left ? 63 - canvas_col : canvas_col - 64);
      y = zero_y - 64 + plane->origin_y + (int)row;
      include_opaque_pixel(rect, x, y, &found);
    }
  }
  return found;
}

static bool blade_palette_color(uint16_t color) {
  const unsigned red = color & 31u;
  const unsigned green = (color >> 5) & 31u;
  const unsigned blue = (color >> 10) & 31u;
  return green > red && green > blue;
}

static bool compose_overlay_blade_rect(const MmxRenderPlayerOverlay *overlay,
                                       int zero_x, int zero_y,
                                       TestOpaqueRect *rect) {
  const MmxRenderPlayerOverlayPlane *planes[3];
  const uint16_t *palettes[3];
  unsigned palette_counts[3];
  bool found = false;

  if (!overlay || !rect) return false;
  planes[0] = &overlay->body;
  planes[1] = &overlay->blade;
  planes[2] = &overlay->front;
  palettes[0] = overlay->palette;
  palettes[1] = overlay->palette;
  palettes[2] = overlay->front_palette;
  palette_counts[0] = overlay->palette_count;
  palette_counts[1] = overlay->palette_count;
  palette_counts[2] = overlay->front_palette_count;
  for (unsigned plane_index = 0; plane_index < 3; ++plane_index) {
    const MmxRenderPlayerOverlayPlane *plane = planes[plane_index];
    const uint16_t *palette = palettes[plane_index];
    const unsigned palette_count = palette_counts[plane_index];
    if (!plane->pixels || !palette || !palette_count) continue;
    for (unsigned row = 0; row < plane->height; ++row) {
      for (unsigned col = 0; col < plane->width; ++col) {
        const unsigned pixel = plane->pixels[row * plane->width + col];
        int canvas_col;
        int world_x;
        int world_y;
        if (!pixel || pixel >= palette_count ||
            !blade_palette_color(palette[pixel]))
          continue;
        canvas_col = plane->origin_x + (int)col;
        world_x = zero_x + (overlay->facing_left ?
            63 - canvas_col : canvas_col - 64);
        world_y = zero_y - 64 + plane->origin_y + (int)row;
        include_opaque_pixel(rect, world_x, world_y, &found);
      }
    }
  }
  return found;
}

static int drawn_zero_feet_y(const MmxSaberAssets *assets, int zero_x,
                             int zero_y, uint8_t facing) {
  static const MmxSaberAttackSnapshot standing[] = {
    {SABER_KIND_GROUND1, 0, SABER_PHASE_ACTIVE, 0, 1, 0, 0},
    {SABER_KIND_GROUND2, 1, SABER_PHASE_ACTIVE, 0, 2, 0, 0},
  };
  int feet_y = -1;

  for (unsigned i = 0; i < sizeof(standing) / sizeof(standing[0]); ++i) {
    MmxSaberAttackSnapshot snapshot = standing[i];
    MmxRenderPlayerOverlay overlay;
    TestOpaqueRect rect;
    snapshot.facing = facing;
    check(MmxSaberRenderResolveSnapshot(assets, snapshot, &overlay) &&
              compose_overlay_opaque_rect(
                  &overlay.body, overlay.facing_left, zero_x, zero_y,
                  overlay.palette_count, &rect),
          "standing body composite exposes drawn Zero feet");
    if (feet_y < 0) feet_y = rect.max_y;
    check(rect.max_y == feet_y,
          "ground standing frames share the drawn Zero feet line");
  }
  return feet_y;
}

static const MmxSaberFrame *manifest_frame_for_tick(
    const MmxSaberAssets *assets, uint8_t animation_id, uint8_t tick) {
  const MmxSaberAnimation *animation =
      MmxSaberAssetsAnimationById(assets, animation_id);
  uint16_t remaining;

  if (!animation || !animation->total_ticks) return NULL;
  remaining = tick < animation->total_ticks ? tick :
      (uint16_t)(animation->total_ticks - 1);
  for (uint16_t step_index = 0; step_index < animation->step_count;
       ++step_index) {
    const MmxSaberStep *step = MmxSaberAssetsAnimationStep(
        assets, animation_id, step_index);
    if (!step) return NULL;
    if (remaining < step->duration_ticks) {
      return MmxSaberAssetsFrameForStep(assets, animation_id, step_index);
    }
    remaining = (uint16_t)(remaining - step->duration_ticks);
  }
  return NULL;
}

static int drawn_ground_blade_center_y(const MmxSaberAssets *assets,
                                       MmxSaberAttackSnapshot snapshot,
                                       int zero_x, int zero_y,
                                       uint8_t facing,
                                       uint16_t *source_frame_id_out) {
  const MmxSaberFrame *drawn_frame = manifest_frame_for_tick(
      assets, snapshot.anim_id, snapshot.tick);
  MmxRenderPlayerOverlay overlay;
  TestOpaqueRect rect;

  MmxSaberAttackSnapshot oriented = snapshot;
  oriented.facing = facing;
  if (source_frame_id_out) *source_frame_id_out = 0xffff;
  check(drawn_frame && MmxSaberRenderResolveSnapshot(assets, oriented, &overlay) &&
            overlay.body.pixels == drawn_frame->body.pixels &&
            overlay.body.width == drawn_frame->body.width &&
            overlay.body.height == drawn_frame->body.height &&
            overlay.body.origin_x == drawn_frame->body.origin_x &&
            overlay.body.origin_y == drawn_frame->body.origin_y &&
            overlay.blade.pixels == drawn_frame->blade.pixels &&
            overlay.blade.width == drawn_frame->blade.width &&
            overlay.blade.height == drawn_frame->blade.height &&
            overlay.blade.origin_x == drawn_frame->blade.origin_x &&
            overlay.blade.origin_y == drawn_frame->blade.origin_y &&
            overlay.blade_layer == drawn_frame->blade_layer &&
            compose_overlay_blade_rect(&overlay, zero_x, zero_y, &rect),
        "spawn-tick renderer frame exposes drawn blade");
  if (source_frame_id_out) *source_frame_id_out = drawn_frame->source_frame_id;
  return (rect.min_y + rect.max_y + 1) / 2;
}

static void thunder_slam_floor_checks(const MmxSaberAssets *assets) {
  const int zero_x = 200;
  const int zero_y = 160;
  const int floor_row = 64 + MMX_SABER_ZERO_FEET_BELOW_Y;
  const int standing_bottom = drawn_zero_feet_y(
      assets, zero_x, zero_y, 0) - 1 - (zero_y - 64);
  MmxSaberAttackSnapshot snapshot = {
    .kind = SABER_KIND_THUNDER_SLAM,
    .index = 0,
    .phase = SABER_PHASE_ACTIVE,
    .tick = 0,
    .anim_id = 19,
    .anim_step = 0,
    .facing = 0x40,
  };
  bool body_floor_ok = true;
  bool dome_floor_ok = true;

  /* Animation 19 stores Zero and dome FX in one body plane. The plane's
   * lowest drawn row is therefore the conservative lower edge for both. */
  for (unsigned tick = 0; tick < 42; ++tick) {
    MmxRenderPlayerOverlay overlay = {0};
    TestOpaqueRect rect;
    int lowest_row;
    snapshot.tick = (uint8_t)tick;
    snapshot.anim_step = (uint8_t)(tick / 3);
    if (!MmxSaberRenderResolveSnapshot(assets, snapshot, &overlay) ||
        !compose_overlay_opaque_rect(&overlay.body, overlay.facing_left,
                                     zero_x, zero_y, overlay.palette_count,
                                     &rect)) {
      body_floor_ok = false;
      dome_floor_ok = false;
      continue;
    }
    lowest_row = rect.max_y - 1 - (zero_y - 64);
    if (lowest_row != standing_bottom) body_floor_ok = false;
    if (lowest_row > floor_row) dome_floor_ok = false;
  }
  check(body_floor_ok,
        "every thunder slam tick keeps drawn body bottom at standing floor contact");
  check(dome_floor_ok,
        "every thunder slam tick keeps dome FX bottom on or above floor row");
}

static MmxSaberAssets *make_flash_assets(bool include_new) {
  enum {
    CAPACITY = 320,
    SOURCE_OFFSET = 112,
    PALETTE_OFFSET = 148,
    ANIMATION_SIZE = 16,
    STEP_SIZE = 8,
    FRAME_SIZE = 40,
  };
  const unsigned palette_count = include_new ? 7 : 4;
  const unsigned animation_count = include_new ? 2 : 1;
  const unsigned step_count = animation_count;
  const unsigned frame_count = animation_count;
  /* The parser pads the palette section to a 4-byte boundary. */
  const unsigned animation_offset = PALETTE_OFFSET + ((palette_count * 2 + 3u) & ~3u);
  const unsigned step_offset = animation_offset +
      animation_count * ANIMATION_SIZE;
  const unsigned frame_offset = step_offset + step_count * STEP_SIZE;
  const unsigned pixel_offset = frame_offset + frame_count * FRAME_SIZE;
  const unsigned pixel_size = include_new ? 10 : 6;
  const unsigned size = pixel_offset + pixel_size;
  uint8_t data[CAPACITY] = {0};
  uint8_t digest[32];
  char reason[128] = {0};

  memcpy(data, "MMXSABR\0", 8);
  put_u16(data, 8, 1);
  put_u16(data, 10, 112);
  put_u32(data, 12, size);
  put_u16(data, 20, 1); /* source count */
  put_u16(data, 22, palette_count); /* palette count */
  put_u16(data, 24, animation_count); /* animation count */
  put_u16(data, 26, step_count); /* step count */
  put_u16(data, 28, frame_count); /* frame count */
  put_u32(data, 32, pixel_size); /* frame pixel data size */
  put_u16(data, SOURCE_OFFSET, 1); /* source id */
  put_u16(data, PALETTE_OFFSET + 0, 0);
  if (include_new) {
    put_u16(data, PALETTE_OFFSET + 2, 0x0001); /* legacy: luminance 299 */
    put_u16(data, PALETTE_OFFSET + 4, 0x0009); /* legacy: luminance 2691 */
    put_u16(data, PALETTE_OFFSET + 6, 0x0010); /* legacy: luminance 4784 */
    put_u16(data, PALETTE_OFFSET + 8, 0x0003); /* shared: luminance 897 */
    put_u16(data, PALETTE_OFFSET + 10, 0x0005); /* new: luminance 1495 */
    put_u16(data, PALETTE_OFFSET + 12, 0x0006); /* new: luminance 1794 */
  } else {
    put_u16(data, PALETTE_OFFSET + 2, 0x009c);
    put_u16(data, PALETTE_OFFSET + 4, 0x0094);
    put_u16(data, PALETTE_OFFSET + 6, 0x2388);
  }
  put_u16(data, animation_offset + 0, 1); /* legacy animation id */
  put_u16(data, animation_offset + 2, 0); /* first step */
  put_u16(data, animation_offset + 4, 1); /* step count */
  put_u16(data, animation_offset + 6, 1); /* total ticks */
  if (include_new) {
    put_u16(data, animation_offset + ANIMATION_SIZE + 0, 8);
    put_u16(data, animation_offset + ANIMATION_SIZE + 2, 1);
    put_u16(data, animation_offset + ANIMATION_SIZE + 4, 1);
    put_u16(data, animation_offset + ANIMATION_SIZE + 6, 1);
  }
  put_u16(data, step_offset + 0, 0); /* legacy frame index */
  put_u16(data, step_offset + 2, 1); /* duration */
  if (include_new) {
    put_u16(data, step_offset + STEP_SIZE + 0, 1); /* new frame index */
    put_u16(data, step_offset + STEP_SIZE + 2, 1); /* duration */
  }
  put_u32(data, frame_offset, 0); /* body offset */
  put_u32(data, frame_offset + 4, 4); /* body length */
  put_u16(data, frame_offset + 8, 2);
  put_u16(data, frame_offset + 10, 2);
  put_u32(data, frame_offset + 16, 4); /* blade offset */
  put_u32(data, frame_offset + 20, 2); /* blade length */
  put_u16(data, frame_offset + 24, 2);
  put_u16(data, frame_offset + 26, 1);
  put_u16(data, frame_offset + 32, 1); /* source id */
  put_u16(data, frame_offset + 34, 0); /* source frame */
  data[frame_offset + 36] = 1; /* blade behind body */
  if (include_new) {
    const unsigned new_frame = frame_offset + FRAME_SIZE;
    put_u32(data, new_frame, 6); /* body offset */
    put_u32(data, new_frame + 4, 4); /* body length */
    put_u16(data, new_frame + 8, 4);
    put_u16(data, new_frame + 10, 1);
    put_u32(data, new_frame + 16, 0); /* empty blade offset */
    put_u32(data, new_frame + 20, 0); /* blade length */
    put_u16(data, new_frame + 32, 1); /* source id */
    put_u16(data, new_frame + 34, 1); /* source frame */
  }
  data[pixel_offset + 0] = 1;
  data[pixel_offset + 1] = 2;
  data[pixel_offset + 2] = include_new ? 3 : 0;
  data[pixel_offset + 3] = include_new ? 4 : 1;
  data[pixel_offset + 4] = 2;
  data[pixel_offset + 5] = 3;
  if (include_new) {
    data[pixel_offset + 6] = 4;
    data[pixel_offset + 7] = 5;
    data[pixel_offset + 8] = 6;
    data[pixel_offset + 9] = 5;
  }
  memset(data + 68, 0, sizeof(digest));
  sha256_compute(data, size, digest);
  memcpy(data + 68, digest, sizeof(digest));
  return MmxSaberAssetsParse(data, size, NULL, reason, sizeof(reason));
}

static int check_plane(const MmxRenderPlayerOverlayPlane *actual,
                       const MmxSaberPlane *expected) {
  return actual->pixels == expected->pixels &&
      actual->width == expected->width && actual->height == expected->height &&
      actual->origin_x == expected->origin_x &&
      actual->origin_y == expected->origin_y;
}

static int same_overlay_plane(const MmxRenderPlayerOverlayPlane *left,
                              const MmxRenderPlayerOverlayPlane *right) {
  return left->pixels == right->pixels && left->width == right->width &&
      left->height == right->height && left->origin_x == right->origin_x &&
      left->origin_y == right->origin_y;
}

static int empty_overlay_plane(const MmxRenderPlayerOverlayPlane *plane) {
  return plane && !plane->pixels && !plane->width && !plane->height &&
      !plane->origin_x && !plane->origin_y;
}

static unsigned color_luminance(uint16_t color) {
  unsigned red = color & 31, green = (color >> 5) & 31, blue = (color >> 10) & 31;
  return red * 299 + green * 587 + blue * 114;
}

static void mark_expected_palette_indices(const MmxSaberPlane *plane,
                                          uint16_t palette_count,
                                          uint8_t *indices) {
  size_t pixel_count;
  if (!plane || !plane->pixels || !indices) return;
  pixel_count = (size_t)plane->width * plane->height;
  for (size_t i = 0; i < pixel_count; ++i) {
    uint8_t index = plane->pixels[i];
    if (index && index < palette_count) indices[index] = 1;
  }
}

/* Public-accessor-only oracle for entries used only by animations after 7. */
static void expected_special_only_palette_indices(const MmxSaberAssets *assets,
                                                  uint16_t palette_count,
                                                  uint8_t *special_only) {
  uint8_t special_frame_bits[UINT16_MAX / 8 + 1] = {0};
  uint8_t other_palette_indices[256] = {0};
  uint16_t frame_count;

  if (!assets || !special_only) return;
  memset(special_only, 0, 256 * sizeof(*special_only));
  frame_count = MmxSaberAssetsFrameCount(assets);
  for (unsigned animation_id = 8; animation_id <= UINT16_MAX;
       ++animation_id) {
    const MmxSaberAnimation *animation = MmxSaberAssetsAnimationById(
        assets, (uint16_t)animation_id);
    if (!animation) continue;
    for (uint16_t step = 0; step < animation->step_count; ++step) {
      const MmxSaberFrame *special_frame = MmxSaberAssetsFrameForStep(
          assets, (uint16_t)animation_id, step);
      if (!special_frame) continue;
      for (uint16_t frame_index = 0; frame_index < frame_count;
           ++frame_index) {
        if (MmxSaberAssetsFrameAt(assets, frame_index) == special_frame)
          special_frame_bits[frame_index >> 3] |=
              (uint8_t)(1u << (frame_index & 7));
      }
    }
  }
  for (uint16_t frame_index = 0; frame_index < frame_count; ++frame_index) {
    const MmxSaberFrame *frame = MmxSaberAssetsFrameAt(assets, frame_index);
    uint8_t *indices =
        (special_frame_bits[frame_index >> 3] &
         (uint8_t)(1u << (frame_index & 7))) ?
        special_only : other_palette_indices;
    if (!frame) continue;
    mark_expected_palette_indices(&frame->body, palette_count, indices);
    mark_expected_palette_indices(&frame->blade, palette_count, indices);
  }
  for (unsigned i = 1; i < palette_count; ++i)
    if (other_palette_indices[i]) special_only[i] = 0;
}

/* Independent nearest-group oracle for entries added after legacy animations. */
static uint16_t expected_nearest_legacy_flash(const uint16_t *palette,
                                              const uint16_t *legacy_mapped,
                                              uint16_t legacy_palette_count,
                                              uint16_t color) {
  unsigned color_value = color_luminance(color);
  unsigned best_index = 1;
  unsigned best_luminance = color_luminance(palette[best_index]);
  unsigned best_distance = color_value > best_luminance ?
      color_value - best_luminance : best_luminance - color_value;
  for (unsigned i = 2; i < legacy_palette_count; ++i) {
    unsigned luminance = color_luminance(palette[i]);
    unsigned distance = color_value > luminance ?
        color_value - luminance : luminance - color_value;
    if (distance < best_distance ||
        (distance == best_distance && luminance < best_luminance)) {
      best_index = i;
      best_luminance = luminance;
      best_distance = distance;
    }
  }
  return legacy_mapped[best_index];
}

/* Independent oracle for the old donor-palette mapping at
 * oldsaber/saber-zero-variant:src/mmx_renderer.c:1138-1191, with special-only
 * entries excluded from ranking. */
static void expected_guarded_flash_palette(uint16_t *mapped,
                                           const uint16_t *palette,
                                           uint16_t palette_count,
                                           const uint16_t *flash,
                                           const uint8_t *special_only) {
  uint16_t donor_order[256], flash_order[16];
  uint16_t ranked_palette[256] = {0};
  uint16_t ranked_mapped[256] = {0};
  unsigned donor_count = 0, groups = 0;
  bool has_special_only = false;
  bool has_ranked_indices = false;

  memcpy(mapped, palette, (size_t)palette_count * sizeof(*mapped));
  mapped[0] = 0;
  for (unsigned i = 1; i < palette_count; ++i) {
    if (special_only && special_only[i])
      has_special_only = true;
    else
      has_ranked_indices = true;
  }
  if (!has_ranked_indices) has_special_only = false;
  for (unsigned i = 1; i < palette_count; ++i) {
    if (has_special_only && special_only[i]) continue;
    unsigned j = donor_count;
    while (j && (color_luminance(palette[donor_order[j - 1]]) >
                     color_luminance(palette[i]) ||
                 (color_luminance(palette[donor_order[j - 1]]) ==
                      color_luminance(palette[i]) && donor_order[j - 1] > i))) {
      donor_order[j] = donor_order[j - 1];
      --j;
    }
    donor_order[j] = (uint16_t)i;
    ++donor_count;
  }
  for (unsigned i = 0; i < 16; ++i) {
    unsigned j = i;
    while (j && (color_luminance(flash[flash_order[j - 1]]) >
                     color_luminance(flash[i]) ||
                 (color_luminance(flash[flash_order[j - 1]]) ==
                      color_luminance(flash[i]) && flash_order[j - 1] > i))) {
      flash_order[j] = flash_order[j - 1];
      --j;
    }
    flash_order[j] = (uint16_t)i;
  }
  for (unsigned rank = 0; rank < donor_count;) {
    unsigned end = rank + 1;
    unsigned luminance = color_luminance(palette[donor_order[rank]]);
    while (end < donor_count &&
           color_luminance(palette[donor_order[end]]) == luminance) ++end;
    ++groups;
    rank = end;
  }
  for (unsigned group = 0, rank = 0; rank < donor_count; ++group) {
    unsigned end = rank + 1;
    unsigned luminance = color_luminance(palette[donor_order[rank]]);
    while (end < donor_count &&
           color_luminance(palette[donor_order[end]]) == luminance) ++end;
    unsigned flash_rank = groups > 1 ? group * 15 / (groups - 1) : 0;
    for (unsigned i = rank; i < end; ++i)
      mapped[donor_order[i]] = flash[flash_order[flash_rank]];
    rank = end;
  }
  if (has_special_only) {
    unsigned ranked_count = 1;
    for (unsigned i = 0; i < donor_count; ++i) {
      unsigned index = donor_order[i];
      ranked_palette[ranked_count] = palette[index];
      ranked_mapped[ranked_count] = mapped[index];
      ++ranked_count;
    }
    for (unsigned i = 1; i < palette_count; ++i)
      if (special_only[i])
        mapped[i] = expected_nearest_legacy_flash(
            ranked_palette, ranked_mapped, (uint16_t)ranked_count,
            palette[i]);
  }
}

static void expected_flash_palette(uint16_t *mapped, const uint16_t *palette,
                                   uint16_t palette_count,
                                   const uint16_t *flash) {
  expected_guarded_flash_palette(mapped, palette, palette_count, flash, NULL);
}

static bool find_blade_snapshot(const MmxSaberAssets *assets,
                               MmxSaberAttackSnapshot *snapshot,
                               const MmxSaberFrame **expected) {
  for (size_t i = 0; i < MmxSaberAttackTableCount(); ++i) {
    const MmxSaberAttack *attack = MmxSaberAttackTableAt(i);
    const MmxSaberAnimation *animation;
    uint16_t tick = 0;
    if (!attack) continue;
    animation = MmxSaberAssetsAnimationById(assets, attack->visual_animation);
    if (!animation) continue;
    for (uint16_t step = 0; step < animation->step_count; ++step) {
      const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(
          assets, attack->visual_animation, step);
      if (frame && frame->blade.pixels && frame->blade.width && frame->blade.height) {
        *snapshot = (MmxSaberAttackSnapshot){
          attack->kind, attack->index, SABER_PHASE_ACTIVE, (uint8_t)tick,
          (uint8_t)attack->visual_animation, (uint8_t)step, 0};
        *expected = frame;
        return true;
      }
      tick = (uint16_t)(tick + animation->steps[step].duration_ticks);
    }
  }
  return false;
}

static void shipped_purple_blade_checks(const MmxSaberAssets *assets) {
  const uint16_t *base = MmxSaberAssetsPalette(assets);
  const uint16_t count = MmxSaberAssetsPaletteCount(assets);
  const MmxSaberAttackSnapshot snapshot = {
    kKinds[0], kIndices[0], SABER_PHASE_ACTIVE, kOldTuples[0][0].tick, 1,
    0, 0x40};
  MmxRenderPlayerOverlay actual;
  unsigned greens = 0, others = 0;
  bool ok;

  test_zero_state = (MmxZeroState){0};
  test_black_zero = false;
  test_arms = true;
  ok = MmxSaberRenderResolveSnapshot(assets, snapshot, &actual);
  check(ok && actual.palette != base,
        "shipped sheet with arms resolves a recoloured palette");
  for (unsigned i = 1; ok && i < count; ++i) {
    const unsigned r = base[i] & 31u, g = (base[i] >> 5) & 31u,
                   b = (base[i] >> 10) & 31u;
    if (g > r && g > b) {
      ++greens;
      check(actual.palette[i] == MmxSaberPurpleBlade(base[i]),
            "shipped green blade entries turn purple with arms");
    } else {
      ++others;
      check(actual.palette[i] == base[i],
            "shipped non-green entries stay unchanged with arms");
    }
  }
  check(greens >= 4 && others >= 4,
        "shipped palette has both blade greens and body colours");
  test_arms = false;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette == base,
        "shipped sheet without arms keeps the loaded palette");
}

static void black_zero_render_checks(const MmxSaberAssets *assets,
                                     const MmxSaberAssets *ride) {
  const uint16_t *base = MmxSaberAssetsPalette(assets);
  const uint16_t *ride_base = MmxSaberAssetsPalette(ride);
  uint16_t base_count = MmxSaberAssetsPaletteCount(assets);
  uint16_t ride_count = MmxSaberAssetsPaletteCount(ride);
  uint16_t expected_black[256] = {0};
  uint16_t expected_flash[256] = {0};
  uint16_t normal_flash[256] = {0};
  uint8_t special_only[256] = {0};
  bool legacy_unchanged;
  MmxSaberAttackSnapshot snapshot;
  const MmxSaberFrame *expected;
  MmxRenderPlayerOverlay actual;
  const uint16_t *cached;
  uint8_t ram[0x20000] = {0};

  /* The shipped sheets keep the blade in separate frames, so use the first
   * ground-slash tuple (body plane) as the overlay frame under test. */
  snapshot = (MmxSaberAttackSnapshot){
    kKinds[0], kIndices[0], SABER_PHASE_ACTIVE, kOldTuples[0][0].tick, 1,
    0, 0x40};
  expected = MmxSaberAssetsFrameForStep(assets, 1, kOldTuples[0][0].step);
  check(expected != NULL,
        "Black Zero render test finds an overlay frame");
  MmxSaberBlackZeroPalette(base, base_count, expected_black);
  test_zero_state = (MmxZeroState){0};
  test_black_zero = false;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette == base && actual.palette_count == base_count,
        "normal overlay state keeps the loaded palette");
  test_black_zero = true;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette != base &&
            !memcmp(actual.palette, expected_black,
                    (size_t)base_count * sizeof(*base)) &&
            actual.palette_count == base_count,
        "Black Zero overlay uses the converted palette");
  cached = actual.palette;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette == cached,
        "Black Zero overlay reuses its cached palette");

  test_zero_state.charge = 30;
  test_zero_state.charge_phase = 0;
  expected_special_only_palette_indices(assets, base_count, special_only);
  expected_guarded_flash_palette(expected_flash, expected_black, base_count,
                                 MmxZeroBodyColors(&test_zero_state),
                                 special_only);
  expected_guarded_flash_palette(normal_flash, base, base_count,
                                 MmxZeroBodyColors(&test_zero_state),
                                 special_only);
  check(memcmp(expected_flash, normal_flash,
               (size_t)base_count * sizeof(*base)) != 0 &&
            MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            !memcmp(actual.palette, expected_flash,
                    (size_t)base_count * sizeof(*base)),
        "charge flash ranks against the Black Zero palette");
  legacy_unchanged = MmxSaberRenderResolveSnapshot(assets, snapshot, &actual);
  for (unsigned i = 0; legacy_unchanged && i < base_count; ++i)
    legacy_unchanged = special_only[i] ||
        actual.palette[i] == expected_flash[i];
  check(legacy_unchanged,
        "legacy charged colours unchanged by special-move art");

  ram[0x0baa] = 0x2c;
  MmxSaberBlackZeroPalette(ride_base, ride_count, expected_black);
  test_zero_state = (MmxZeroState){0};
  test_black_zero = false;
  test_arms = true;
  check(MmxSaberRenderResolveRide(ride, ram, &actual) &&
            actual.palette == ride_base && actual.palette_count == ride_count,
        "arms do not recolour the Ride Armor pilot");
  test_arms = false;
  check(MmxSaberRenderResolveRide(ride, ram, &actual) &&
            actual.palette == ride_base && actual.palette_count == ride_count,
        "normal Ride Armor pilot keeps the loaded palette");
  test_black_zero = true;
  check(MmxSaberRenderResolveRide(ride, ram, &actual) &&
            actual.palette != ride_base &&
            !memcmp(actual.palette, expected_black,
                    (size_t)ride_count * sizeof(*ride_base)) &&
            actual.palette_count == ride_count,
        "Black Zero Ride Armor pilot uses the converted palette");

  test_black_zero = false;
  test_zero_state = (MmxZeroState){0};
}
static unsigned first_pixel(const MmxSaberPlane *plane) {
  size_t size = (size_t)plane->width * plane->height;
  for (size_t i = 0; i < size; ++i) if (plane->pixels[i]) return plane->pixels[i];
  return 0;
}

static void purple_blade_render_checks(const MmxSaberAssets *assets) {
  const uint16_t *base = MmxSaberAssetsPalette(assets);
  uint16_t base_count = MmxSaberAssetsPaletteCount(assets);
  uint16_t expected_black[256] = {0};
  uint16_t expected_flash[256] = {0};
  MmxSaberAttackSnapshot snapshot;
  const MmxSaberFrame *expected;
  MmxRenderPlayerOverlay actual;
  const uint16_t *cached;

  check(find_blade_snapshot(assets, &snapshot, &expected),
        "purple render test finds a body-and-blade frame");
  test_zero_state = (MmxZeroState){0};
  test_black_zero = false;
  test_arms = false;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette == base,
        "without arms the overlay palette stays original");
  test_arms = true;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette != base && actual.palette[1] == base[1] &&
            actual.palette[2] == base[2] &&
            actual.palette[3] == MmxSaberPurpleBlade(base[3]) &&
            MmxSaberRenderSharedBladePaletteCount() == 1,
        "arms recolour only the unique blade index and report one shared index");
  cached = actual.palette;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette == cached,
        "arms overlay reuses its cached palette");
  test_zero_state.charge = 30;
  test_zero_state.charge_phase = 0;
  expected_flash_palette(expected_flash, cached, base_count,
                         MmxZeroBodyColors(&test_zero_state));
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            !memcmp(actual.palette, expected_flash,
                    (size_t)base_count * sizeof(*base)),
        "charge flash ranks against the purple overlay palette");
  test_zero_state = (MmxZeroState){0};
  MmxSaberBlackZeroPalette(base, base_count, expected_black);
  test_black_zero = true;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
            actual.palette[1] == expected_black[1] &&
            actual.palette[2] == expected_black[2] &&
            actual.palette[2] != MmxSaberPurpleBlade(expected_black[2]) &&
            actual.palette[3] == MmxSaberPurpleBlade(expected_black[3]) &&
            actual.palette[3] != expected_black[3],
        "Black Zero keeps body conversion and applies purple to the unique blade");
  test_arms = false;
  test_black_zero = false;
  test_zero_state = (MmxZeroState){0};
}
static void charge_flash_checks(const MmxSaberAssets *assets) {
  const uint16_t *base = MmxSaberAssetsPalette(assets);
  uint16_t palette_count = MmxSaberAssetsPaletteCount(assets);
  MmxSaberAttackSnapshot snapshot;
  const MmxSaberFrame *expected;
  bool native_flash[8], overlay_flash[8];
  uint16_t expected_palette[256];

  check(find_blade_snapshot(assets, &snapshot, &expected),
        "sidecar exposes a donor frame with body and blade planes");
  for (unsigned i = 0; i < 128; ++i) test_zero_colors[i] = (uint16_t)(i & 0x7fff);
  for (unsigned tier = 0; tier < 3; ++tier)
    for (unsigned i = 0; i < 16; ++i)
      test_charge_colors[tier][i] = (uint16_t)((((i + tier + 1) * 3) & 31) |
          ((((i + tier + 2) * 5) & 31) << 5) |
          ((((i + tier + 3) * 7) & 31) << 10));
  test_zero_state = (MmxZeroState){0};
  test_zero_state.charge = 30;
  for (unsigned frame = 0; frame < 8; ++frame) {
    MmxRenderPlayerOverlay actual;
    test_zero_state.charge_phase = (uint8_t)frame;
    native_flash[frame] = MmxZeroBodyColors(&test_zero_state) != test_zero_colors + 16;
    check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual),
          "charged donor frame resolves");
    overlay_flash[frame] = actual.palette != base;
    check(actual.body.pixels == expected->body.pixels &&
              actual.blade.pixels == expected->blade.pixels,
          "charge palette selection preserves both donor planes");
    if (native_flash[frame]) {
      expected_flash_palette(expected_palette, base, palette_count,
                             MmxZeroBodyColors(&test_zero_state));
      check(!memcmp(actual.palette, expected_palette,
                    (size_t)palette_count * sizeof(*base)),
            "flashing donor palette matches the old role mapping");
    } else {
      check(actual.palette == base,
            "non-flashing charge frame keeps the donor palette");
    }
  }
  check(!memcmp(native_flash, overlay_flash, sizeof(native_flash)) &&
            native_flash[0] && native_flash[1] && !native_flash[2] &&
            !native_flash[3] && native_flash[4] && native_flash[5],
        "donor palette flashes on exactly native Zero's body frames");

  test_zero_state.charge_phase = 0;
  MmxRenderPlayerOverlay active;
  unsigned body_pixel = first_pixel(&expected->body);
  unsigned blade_pixel = first_pixel(&expected->blade);
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &active),
        "active flash frame resolves for body and blade color assertion");
  expected_flash_palette(expected_palette, base, palette_count,
                         MmxZeroBodyColors(&test_zero_state));
  check(active.palette[body_pixel] == expected_palette[body_pixel] &&
            active.palette[blade_pixel] == expected_palette[blade_pixel],
        "body and blade both use the flashing donor palette");

  test_zero_state.charge = 0;
  bool no_charge_flash = false;
  for (unsigned frame = 0; frame < 8; ++frame) {
    MmxRenderPlayerOverlay actual;
    test_zero_state.charge_phase = (uint8_t)frame;
    check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual),
          "uncharged donor frame resolves");
    no_charge_flash |= actual.palette != base;
  }
  check(!no_charge_flash, "no charge never flashes the donor palette");
  test_zero_state = (MmxZeroState){0};
}

static void legacy_flash_palette_checks(void) {
  MmxSaberAssets *extended = make_flash_assets(true);
  MmxSaberAssets *legacy = make_flash_assets(false);
  const uint16_t *extended_palette;
  const uint16_t *legacy_palette;
  MmxSaberAttackSnapshot extended_snapshot;
  MmxSaberAttackSnapshot legacy_snapshot;
  const MmxSaberFrame *expected;
  MmxRenderPlayerOverlay actual;
  uint16_t expected_legacy[256] = {0};
  uint16_t expected_all[256] = {0};
  uint16_t legacy_count;

  check(extended != NULL && legacy != NULL,
        "flash regression sidecars parse");
  check(MmxSaberAssetsAnimationById(extended, 1) != NULL &&
            MmxSaberAssetsAnimationById(extended, 8) != NULL &&
            MmxSaberAssetsFrameForStep(extended, 8, 0) != NULL,
        "flash regression sidecar has legacy and new animation frames");
  extended_palette = MmxSaberAssetsPalette(extended);
  legacy_palette = MmxSaberAssetsPalette(legacy);
  legacy_count = MmxSaberAssetsPaletteCount(legacy);
  check(find_blade_snapshot(extended, &extended_snapshot, &expected),
        "flash regression finds legacy donor frame");
  check(find_blade_snapshot(legacy, &legacy_snapshot, &expected),
        "legacy-only flash regression finds donor frame");

  /* Red-channel values 1, 9, 16 and shared 3 are ranked groups. New-only 5
   * and 6 sit between the first two; 6 is exactly tied and chooses darker 3. */
  for (unsigned i = 0; i < 16; ++i) test_charge_colors[0][i] = (uint16_t)i;
  test_zero_state = (MmxZeroState){0};
  test_zero_state.charge = 30;
  test_zero_state.charge_phase = 0;
  test_arms = false;
  test_black_zero = false;
  expected_flash_palette(expected_legacy, extended_palette, 5,
                         MmxZeroBodyColors(&test_zero_state));
  check(MmxSaberRenderResolveSnapshot(extended, extended_snapshot, &actual) &&
            actual.palette_count == 7,
        "extended flash palette resolves through public snapshot path");
  check(actual.palette[1] == expected_legacy[1] &&
            actual.palette[2] == expected_legacy[2] &&
            actual.palette[3] == expected_legacy[3],
        "legacy indices keep old flash colours after new entries are added");
  check(actual.palette[4] == expected_legacy[4],
        "index shared by animation 1 and animation 8 stays ranked");
  check(actual.palette[5] == expected_nearest_legacy_flash(
                extended_palette, expected_legacy, 5, extended_palette[5]) &&
            actual.palette[6] == expected_nearest_legacy_flash(
                extended_palette, expected_legacy, 5, extended_palette[6]),
        "new indices use closest legacy flash group with darker tie-break");

  expected_flash_palette(expected_all, legacy_palette, legacy_count,
                         MmxZeroBodyColors(&test_zero_state));
  check(MmxSaberRenderResolveSnapshot(legacy, legacy_snapshot, &actual) &&
            !memcmp(actual.palette, expected_all,
                    (size_t)legacy_count * sizeof(*legacy_palette)),
        "legacy-only animations keep full old flash mapping");

  MmxSaberAssetsFree(legacy);
  MmxSaberAssetsFree(extended);
  test_zero_state = (MmxZeroState){0};
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
            !actual.facing_left && empty_overlay_plane(&actual.front) &&
            actual.front_palette == NULL && actual.front_palette_count == 0,
        "resolver returns the exact donor planes, palette, layer, and facing");
}

static void special_render_checks(const MmxSaberAssets *assets) {
  const uint16_t *raw_palette = MmxSaberAssetsPalette(assets);
  const uint16_t raw_palette_count = MmxSaberAssetsPaletteCount(assets);
  const MmxSaberAnimation *upper =
      MmxSaberAssetsAnimationById(assets, 8);
  const MmxSaberAnimation *upper_arc =
      MmxSaberAssetsAnimationById(assets, 9);
  const MmxSaberAnimation *rolling =
      MmxSaberAssetsAnimationById(assets, 10);
  const MmxSaberAnimation *rolling_arc =
      MmxSaberAssetsAnimationById(assets, 11);
  const MmxSaberAnimation *crescent =
      MmxSaberAssetsAnimationById(assets, 12);
  const MmxSaberAnimation *thrust =
      MmxSaberAssetsAnimationById(assets, 13);
  const MmxSaberAnimation *thrust_fx =
      MmxSaberAssetsAnimationById(assets, 14);
  MmxRenderPlayerOverlay actual;
  MmxRenderPlayerOverlay normal_step_one = {0};
  MmxSaberAttackSnapshot rolling_step_one;
  uint16_t tick;
  uint8_t rolling_step_one_tick = 0;

  check(upper && upper_arc && rolling && rolling_arc && crescent &&
            upper->step_count == 6 && upper_arc->step_count == 6 &&
            rolling->step_count == 9 && rolling_arc->step_count == 8 &&
            crescent->step_count == 9,
        "production asset exposes six-step uppercut and rolling pairing animations");
  check(thrust && thrust_fx && thrust->step_count == 6 &&
            thrust_fx->step_count == 5,
        "production asset exposes six Enkoujin thrust body steps and retains the streak asset");
  check(MmxSaberRenderCrescentEnabled(),
        "MmxSaberRenderSetCrescentEnabled defaults to enabled");
  MmxSaberRenderSetCrescentEnabled(true);

  test_zero_state = (MmxZeroState){0};
  test_black_zero = false;
  test_arms = false;
  tick = 0;
  for (uint16_t step = 0; step < upper->step_count; ++step) {
    const MmxSaberStep *timing =
        MmxSaberAssetsAnimationStep(assets, 8, step);
    const MmxSaberFrame *body =
        MmxSaberAssetsFrameForStep(assets, 8, step);
    const MmxSaberFrame *arc =
        MmxSaberAssetsFrameForStep(assets, 9, step);
    MmxSaberAttackSnapshot snapshot = {
      SABER_KIND_UPPERCUT, 0, SABER_PHASE_ACTIVE, (uint8_t)tick, 8,
      (uint8_t)step, 0x40};

    check(timing && body && arc,
          "production asset exposes every uppercut body and arc step");
    check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual),
          "production edit resolve_snapshot resolves every uppercut step");
    check(actual.active && check_plane(&actual.body, &body->body) &&
              check_plane(&actual.blade, &arc->body) &&
              actual.blade_layer == 2 &&
              empty_overlay_plane(&actual.front) &&
              actual.front_palette == NULL && actual.front_palette_count == 0 &&
              actual.palette == raw_palette &&
              actual.palette_count == raw_palette_count && !actual.facing_left,
          "production edit resolve_snapshot pairs animation 8 body with animation 9 arc at layer 2");
    tick = (uint16_t)(tick + timing->duration_ticks);
  }

  {
    const MmxSaberFrame *final_body =
        MmxSaberAssetsFrameForStep(assets, 8, 5);
    const MmxSaberFrame *final_arc =
        MmxSaberAssetsFrameForStep(assets, 9, 5);
    MmxSaberAttackSnapshot snapshot = {
      SABER_KIND_UPPERCUT, 0, SABER_PHASE_ACTIVE, 29, 8, 5, 0x40};

    check(final_body && final_arc &&
              MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
              check_plane(&actual.body, &final_body->body) &&
              check_plane(&actual.blade, &final_arc->body) &&
              actual.blade_layer == 2,
          "uppercut render draws published body and arc step 5 after the apex");
  }

  {
    const MmxSaberFrame *rising_body =
        MmxSaberAssetsFrameForStep(assets, 8, 3);
    const MmxSaberFrame *rising_arc =
        MmxSaberAssetsFrameForStep(assets, 9, 3);
    const MmxSaberFrame *final_body =
        MmxSaberAssetsFrameForStep(assets, 8, 5);
    const MmxSaberFrame *final_arc =
        MmxSaberAssetsFrameForStep(assets, 9, 5);
    bool rising_hold = rising_body && rising_arc && final_body && final_arc;

    for (uint16_t rising_tick = 6; rising_tick < 29 && rising_hold;
         ++rising_tick) {
      MmxSaberAttackSnapshot snapshot = {
        SABER_KIND_UPPERCUT, 0, SABER_PHASE_ACTIVE, rising_tick, 8, 3, 0x40};
      if (!MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) ||
          !check_plane(&actual.body, &rising_body->body) ||
          !check_plane(&actual.blade, &rising_arc->body) ||
          actual.blade_layer != 2)
        rising_hold = false;
    }
    MmxSaberAttackSnapshot snapshot = {
      SABER_KIND_UPPERCUT, 0, SABER_PHASE_ACTIVE, 29, 8, 5, 0x40};
    check(rising_body && rising_arc && final_body && final_arc && rising_hold &&
              MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) &&
              check_plane(&actual.body, &final_body->body) &&
              check_plane(&actual.blade, &final_arc->body) &&
              actual.blade_layer == 2,
          "uppercut renderer holds step 3 through rising ticks, then pairs final step 5");
  }

  tick = 0;
  for (uint16_t step = 0; step < rolling->step_count; ++step) {
    const MmxSaberStep *timing =
        MmxSaberAssetsAnimationStep(assets, 10, step);
    const MmxSaberFrame *body =
        MmxSaberAssetsFrameForStep(assets, 10, step);
    const MmxSaberFrame *arc = step ?
        MmxSaberAssetsFrameForStep(assets, 11, (uint16_t)(step - 1)) : NULL;
    const MmxSaberFrame *front =
        MmxSaberAssetsFrameForStep(assets, 12, step);
    MmxSaberAttackSnapshot snapshot = {
      SABER_KIND_ROLLING, 0, SABER_PHASE_ACTIVE, (uint8_t)tick, 10, 0, 0x40};

    check(timing && body && front && (!step || arc),
          "production asset exposes every rolling body, delayed arc, and crescent step");
    check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual),
          "production edit resolve_snapshot resolves every rolling step");
    check(actual.active && check_plane(&actual.body, &body->body) &&
              (step ? check_plane(&actual.blade, &arc->body) :
                      empty_overlay_plane(&actual.blade)) &&
              actual.blade_layer == (step ? 2 : 0) &&
              check_plane(&actual.front, &front->body) &&
              actual.front_palette == raw_palette &&
              actual.front_palette_count == raw_palette_count &&
              actual.palette == raw_palette && !actual.facing_left,
          "production edit resolve_snapshot pairs animation 10 body, delayed animation 11 arc, and animation 12 crescent");
    if (step == 1) {
      normal_step_one = actual;
      rolling_step_one_tick = (uint8_t)tick;
    }
    tick = (uint16_t)(tick + timing->duration_ticks);
  }

  static const uint8_t thrust_ticks[] = {0, 2, 4, 6, 8, 10};
  for (uint16_t step = 0; step < thrust->step_count; ++step) {
    const MmxSaberStep *timing =
        MmxSaberAssetsAnimationStep(assets, 13, step);
    const MmxSaberFrame *body =
        MmxSaberAssetsFrameForStep(assets, 13, step);
    MmxSaberAttackSnapshot snapshot = {
      SABER_KIND_THRUST, 0, SABER_PHASE_ACTIVE, thrust_ticks[step], 13,
      (uint8_t)step, 0x40};

    check(timing && body && body->source_id == 16 &&
              body->source_frame_id == step,
          "production asset exposes every Enkoujin thrust body frame (source 16 cells 0-5)");
    check(MmxSaberRenderResolveSnapshot(assets, snapshot, &actual),
          "production edit resolve_snapshot resolves every thrust step");
    check(actual.active && check_plane(&actual.body, &body->body) &&
              empty_overlay_plane(&actual.blade) && actual.blade_layer == 0 &&
              empty_overlay_plane(&actual.front) &&
              actual.front_palette == NULL && actual.front_palette_count == 0 &&
              actual.palette == raw_palette &&
              actual.palette_count == raw_palette_count && !actual.facing_left,
          "production edit resolve_snapshot draws Enkoujin body only with no thrust streak layer");
  }

  {
    static const uint8_t dive_ticks[] = {4, 6, 8, 10, 12};
    static const uint8_t expected_frames[] = {2, 3, 4, 5, 2};
    bool cycle_ok = true;
    for (unsigned i = 0; i < sizeof(dive_ticks) / sizeof(dive_ticks[0]); ++i) {
      MmxSaberAttackSnapshot snapshot = {
        SABER_KIND_THRUST, 0, SABER_PHASE_ACTIVE, dive_ticks[i], 13, 0, 0x40};
      const MmxSaberFrame *body = MmxSaberAssetsFrameForStep(
          assets, 13, MmxSaberThrustAnimationStepForTick(dive_ticks[i]));
      if (!body || body->source_frame_id != expected_frames[i] ||
          !MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) ||
          !actual.active || !check_plane(&actual.body, &body->body) ||
          !empty_overlay_plane(&actual.blade) || actual.blade_layer != 0)
        cycle_ok = false;
    }
    check(cycle_ok, "thrust dive renders Enkoujin cells 2, 3, 4, 5, 2");
  }

  {
    static const uint8_t bounce_ticks[] = {20, 22, 24, 26, 28};
    static const uint8_t expected_frames[] = {2, 3, 4, 5, 2};
    bool cycle_ok = true;
    for (unsigned i = 0; i < sizeof(bounce_ticks) / sizeof(bounce_ticks[0]); ++i) {
      MmxSaberAttackSnapshot snapshot = {
        SABER_KIND_THRUST, 0, SABER_PHASE_ACTIVE, bounce_ticks[i], 13, 0, 0x40};
      const MmxSaberFrame *body = MmxSaberAssetsFrameForStep(
          assets, 13, MmxSaberThrustAnimationStepForTick(bounce_ticks[i]));
      if (!body || body->source_frame_id != expected_frames[i] ||
          !MmxSaberRenderResolveSnapshot(assets, snapshot, &actual) ||
          !actual.active || !check_plane(&actual.body, &body->body) ||
          !empty_overlay_plane(&actual.blade) || actual.blade_layer != 0)
        cycle_ok = false;
    }
    check(cycle_ok, "thrust post-bounce render continues cells 2, 3, 4, 5 without wind-up");
  }

  rolling_step_one = (MmxSaberAttackSnapshot){
    SABER_KIND_ROLLING, 0, SABER_PHASE_ACTIVE, rolling_step_one_tick,
    10, 0, 0x40};
  test_black_zero = true;
  check(MmxSaberRenderResolveSnapshot(assets, rolling_step_one, &actual) &&
            actual.palette != raw_palette &&
            actual.front_palette == raw_palette &&
            actual.front_palette_count == raw_palette_count &&
            same_overlay_plane(&actual.front, &normal_step_one.front),
        "production edit resolve_snapshot keeps crescent raw under Black Zero palette");
  test_black_zero = false;
  test_arms = true;
  check(MmxSaberRenderResolveSnapshot(assets, rolling_step_one, &actual) &&
            actual.palette != raw_palette &&
            actual.front_palette == raw_palette &&
            actual.front_palette_count == raw_palette_count &&
            same_overlay_plane(&actual.front, &normal_step_one.front),
        "production edit resolve_snapshot keeps crescent raw under Arms palette");
  test_arms = false;
  test_zero_state.charge = 30;
  test_zero_state.charge_phase = 0;
  check(MmxSaberRenderResolveSnapshot(assets, rolling_step_one, &actual) &&
            actual.palette != raw_palette &&
            actual.front_palette == raw_palette &&
            actual.front_palette_count == raw_palette_count &&
            same_overlay_plane(&actual.front, &normal_step_one.front),
        "production edit resolve_snapshot keeps crescent raw under charge flash palette");

  test_zero_state = (MmxZeroState){0};
  MmxSaberRenderSetCrescentEnabled(false);
  check(!MmxSaberRenderCrescentEnabled(),
        "MmxSaberRenderSetCrescentEnabled disables crescent");
  check(MmxSaberRenderResolveSnapshot(assets, rolling_step_one, &actual),
        "production edit resolve_snapshot resolves rolling slash without crescent");
  check(actual.active && same_overlay_plane(&actual.body, &normal_step_one.body) &&
            same_overlay_plane(&actual.blade, &normal_step_one.blade) &&
            actual.blade_layer == normal_step_one.blade_layer &&
            actual.palette == normal_step_one.palette &&
            actual.palette_count == normal_step_one.palette_count &&
            actual.facing_left == normal_step_one.facing_left &&
            empty_overlay_plane(&actual.front) && actual.front_palette == NULL &&
            actual.front_palette_count == 0,
        "production edit MmxSaberRenderSetCrescentEnabled clears only front fields");
  MmxSaberRenderSetCrescentEnabled(true);
  check(MmxSaberRenderCrescentEnabled(),
        "MmxSaberRenderSetCrescentEnabled restores crescent");
  test_zero_state = (MmxZeroState){0};
}

static void ride_matrix_checks(const MmxSaberAssets *assets) {
  uint8_t ram[0x20000] = {0};
  bool matrix_ok = true;
  const MmxSaberAnimation *animation =
      MmxSaberAssetsAnimationById(assets, 0x006b);

  ram[0x0baa] = 0x2c;
  ram[0x0e18] = 1;
  ram[0x0e22] = 0x40;
  for (unsigned group = 0; group < 2; ++group) {
    ram[0x0bbe] = (uint8_t)(group ? 0x6b : 0x6a);
    for (unsigned facing = 0; facing < 2; ++facing) {
      ram[0x0bb9] = (uint8_t)(facing ? 0x40 : 0);
      for (unsigned pose = 0; pose < 23; ++pose) {
        MmxRenderPlayerOverlay actual;
        const MmxSaberFrame *expected = MmxSaberAssetsFrameForStep(
            assets, 0x006b, (uint16_t)pose);
        bool resolved = MmxSaberRenderResolveRide(assets, ram, &actual);
        matrix_ok &= resolved && expected && actual.active &&
            actual.body.pixels == expected->body.pixels &&
            actual.body.width == expected->body.width &&
            actual.body.height == expected->body.height &&
            actual.body.origin_x == expected->body.origin_x &&
            actual.body.origin_y == expected->body.origin_y &&
            expected->source_frame_id == kRideSourceFrames[pose] &&
            actual.palette == MmxSaberAssetsPalette(assets) &&
            actual.palette_count == MmxSaberAssetsPaletteCount(assets) &&
            actual.blade.pixels == NULL && actual.blade.width == 0 &&
            actual.blade.height == 0 && actual.blade_layer == 0 &&
            animation && actual.facing_left == ((facing != 0) ^
                                                (animation->facing_xor != 0));
        ram[0x0bbf] = (uint8_t)(pose + 1);
      }
      ram[0x0bbf] = 0;
    }
  }
  check(matrix_ok,
        "Ride Armor resolver maps both pilot groups, all 23 poses, palette, and facing");

  ram[0x0baa] = 0x10;
  ram[0x0bbe] = 0x6b;
  MmxRenderPlayerOverlay inactive;
  check(!MmxSaberRenderResolveRide(assets, ram, &inactive) &&
            !inactive.active,
        "a non-riding action never resolves a Ride Armor overlay");

  ram[0x0baa] = 0x2c;
  ram[0x0bbf] = 23;
  const MmxSaberFrame *pose_zero = MmxSaberAssetsFrameForStep(assets, 0x006b, 0);
  MmxRenderPlayerOverlay fallback;
  check(pose_zero && MmxSaberRenderResolveRide(assets, ram, &fallback) &&
            fallback.body.pixels == pose_zero->body.pixels,
        "an out-of-range native pose falls back to ride pose zero");
}

static void wave_sequence_checks(const MmxSaberWave *wave) {
  static const uint8_t expected_frames[34] = {
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0,
  };
  for (unsigned age = 0; age < sizeof(expected_frames); ++age) {
    MmxRenderWorldSprite actual;
    const MmxSaberWaveFrame *expected = MmxSaberWaveFrameAt(
        wave, expected_frames[age]);
    check(expected != NULL &&
              MmxSaberRenderResolveWaveSnapshot(wave, (uint8_t)age,
                  320, 224, false, &actual),
          "wave resolver accepts every old-cycle age");
    check(actual.pixels == expected->pixels && actual.width == expected->width &&
              actual.height == expected->height &&
              actual.origin_x == expected->origin_x &&
              actual.origin_y == expected->origin_y && actual.world_x == 320 &&
              actual.world_y == 224 && !actual.facing_left &&
              actual.palette == MmxSaberWavePalette(wave) &&
              actual.palette_count == MmxSaberWavePaletteCount(wave) &&
              actual.z == 0xa680,
          "wave resolver selects the hard-coded old two-tick frame sequence");
  }
  {
    const uint16_t *base = MmxSaberWavePalette(wave);
    MmxRenderWorldSprite purple, repeat, normal;
    bool matches = true;

    test_arms = true;
    check(MmxSaberRenderResolveWaveSnapshot(wave, 2, 320, 224, false,
                                            &purple),
          "arms wave resolves");
    for (unsigned i = 0; i < MmxSaberWavePaletteCount(wave); ++i)
      matches &= purple.palette[i] == (base[i] ?
          MmxSaberPurpleBlade(base[i]) : 0);
    check(matches && purple.palette != base,
          "arms wave uses purple copy for every nonzero entry");
    check(MmxSaberRenderResolveWaveSnapshot(wave, 2, 320, 224, false,
                                            &repeat) &&
              repeat.palette == purple.palette,
          "arms wave reuses cached purple palette");
    test_arms = false;
    check(MmxSaberRenderResolveWaveSnapshot(wave, 2, 320, 224, false,
                                            &normal) &&
              normal.palette == base,
          "without arms wave keeps original palette");
  }
  MmxRenderWorldSprite right, left;
  check(MmxSaberRenderResolveWaveSnapshot(wave, 2, 320, 224, false, &right) &&
            MmxSaberRenderResolveWaveSnapshot(wave, 2, 320, 224, true, &left) &&
            !right.facing_left && left.facing_left,
        "wave resolver carries the slot facing into the mirror flag");
}

static void tornado_asset_checks(const MmxSaberAssets *assets) {
  const MmxSaberAnimation *animation =
      MmxSaberAssetsAnimationById(assets, 15);
  MmxSaberProjectileFrame frames[3];
  MmxRenderWorldSprite actual[1];
  TestOpaqueRect rect;
  const uint16_t *palette = MmxSaberAssetsPalette(assets);
  const uint16_t palette_count = MmxSaberAssetsPaletteCount(assets);
  const int takeoff_zero_y = 123;
  const int floor_y = takeoff_zero_y + MMX_SABER_ZERO_FEET_BELOW_Y;
  uint8_t ram[0x20000] = {0};
  MmxSaberProjectileSpawnInfo info = {0};
  unsigned index;

  test_zero_state = (MmxZeroState){0};
  test_arms = false;
  test_black_zero = false;

  check(animation && animation->step_count == 3 &&
            animation->total_ticks == 12,
        "loaded asset exposes three four-tick tornado frames");
  for (unsigned i = 0; i < 3; ++i) {
    const MmxSaberStep *step = MmxSaberAssetsAnimationStep(assets, 15, i);
    const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(assets, 15, i);
    check(step && step->duration_ticks == 4 && frame && frame->body.pixels &&
              frame->body.width == 28 && frame->body.height == 120 &&
              frame->body.origin_x == 14 && frame->body.origin_y == 120,
          "loaded tornado frame keeps 28x120 bottom-centre geometry");
    frames[i] = (MmxSaberProjectileFrame){
      frame->body.pixels,
      frame->body.width,
      frame->body.height,
      frame->body.origin_x,
      frame->body.origin_y,
    };
  }

  MmxSaberProjectileReset(ram);
  MmxSaberRenderSetWave(NULL);
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_TORNADO, frames, 3, 4,
                                   false, true, palette, palette_count);
  info.kind = SABER_PROJECTILE_TORNADO;
  info.x = 321 * 256;
  info.y = floor_y * 256;
  info.lifetime = 20;
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[0].pixels && actual[0].width == 28 &&
            actual[0].height == 120 && actual[0].origin_x == -14 &&
            actual[0].origin_y == -120 && actual[0].palette == palette &&
            actual[0].palette_count == palette_count,
        "loaded tornado strip registers frame zero with palette and origin");
  check(actual[0].facing_left && compose_world_opaque_rect(&actual[0], &rect) &&
            rect.min_x == 308 && rect.max_x == 335 &&
            rect.min_y == floor_y - 120 &&
            rect.max_y == floor_y,
        "left tornado opaque rect is centred on spawn and ends at floor");
  MmxSaberProjectileReset(ram);
  info.facing = 0x40;
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            !actual[0].facing_left && compose_world_opaque_rect(&actual[0], &rect) &&
            rect.min_x == 307 && rect.max_x == 334 &&
            rect.min_y == floor_y - 120 &&
            rect.max_y == floor_y,
        "right tornado opaque rect is centred on spawn and ends at floor");
  for (unsigned i = 0; i < 4; ++i) MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[1].pixels,
        "loaded tornado strip advances after four ticks");
  for (unsigned i = 0; i < 8; ++i) MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[0].pixels,
        "loaded tornado strip loops after three frames");
  MmxSaberProjectileReset(ram);
  put_u16(ram, MMX_SABER_PROJECTILE_PLAYER_Y, 110);
  info.tornado_uppercut_live = true;
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[0].pixels + (120 - 70) * 28 &&
            actual[0].height == 70 && actual[0].origin_y == -120 &&
            actual[0].world_y == floor_y + 120 - 70 &&
            actual[0].world_y + actual[0].origin_y + actual[0].height == floor_y,
        "growing tornado crops its top rows and keeps the bottom at the floor");
  put_u16(ram, MMX_SABER_PROJECTILE_PLAYER_Y, 90);
  MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[0].pixels + (120 - 90) * 28 &&
            actual[0].height == 90 && actual[0].origin_y == -120 &&
            actual[0].world_y == floor_y + 120 - 90 &&
            actual[0].world_y + actual[0].origin_y + actual[0].height == floor_y,
        "tornado crop grows with Zero and remains bottom-anchored");
  {
    static uint8_t collision_rom[0x40000];
    const int zero_x = 321;
    const int zero_y = 123;
    for (unsigned facing = 0; facing < 2; ++facing) {
      const uint8_t native_facing = facing ? 0x40 : 0;
      const uint8_t direction = facing ? 1 : 2;
      const int drawn_feet = drawn_zero_feet_y(
          assets, zero_x, zero_y, native_facing);
      MmxSaberProjectileLiveInfo live[1];
      MmxRenderWorldSprite placed[1];
      TestOpaqueRect placed_rect;
      const MmxSaberProjectile *projectile;
      const int expected_x = zero_x + (facing ? 45 : -45);

      memset(ram, 0, sizeof(ram));
      memset(collision_rom, 0xff, sizeof(collision_rom));
      put_u16(ram, 0x0bad, (unsigned)zero_x);
      put_u16(ram, 0x0bb0, (unsigned)zero_y);
      ram[0x0bd3] = 4;
      ram[0x1f90] = 0x40;
      put_u16(ram, 0x1f8f, 0x4100);
      MmxSaberProjectileReset(ram);
      MmxSaberAttackResetRam(ram);
      MmxSaberProjectileCollisionRom(collision_rom, sizeof(collision_rom));
      MmxSaberAttackStepWithWallAndDash(
          true, true, false, false, true, false, true, false, true, ram,
          native_facing, direction);
      for (unsigned tick = 1; tick <= 4; ++tick)
        MmxSaberAttackStepWithWallAndDash(
            false, true, false, false, true, false, true, false, true, ram,
            native_facing, direction);
      MmxSaberAttackObservePreNative(ram);
      ram[0x0bd3] = 0;
      ram[0x0baa] = 0x06;
      MmxSaberAttackPlayerEnd(ram);
      check(MmxSaberProjectileLive(live, 1) == 1 &&
                live[0].kind == SABER_PROJECTILE_TORNADO &&
                live[0].world_x == expected_x &&
                live[0].world_y == drawn_feet,
            "storm tornado attack uses drawn Zero feet and measured saber-tip x");
      projectile = MmxSaberProjectileGet(0);
      check(MmxSaberRenderProvideWorldSprites(placed, 1) == 1 &&
                placed[0].world_x == expected_x &&
                compose_world_opaque_rect(&placed[0], &placed_rect) &&
                placed_rect.min_x == expected_x - (facing ? 14 : 13) &&
                placed_rect.max_x == expected_x + (facing ? 13 : 14) &&
                projectile &&
                (ram[projectile->slot + 5] |
                 ((unsigned)ram[projectile->slot + 6] << 8)) ==
                    (unsigned)expected_x &&
                (ram[projectile->slot + 8] |
                 ((unsigned)ram[projectile->slot + 9] << 8)) ==
                    (unsigned)drawn_feet &&
                placed_rect.max_y == drawn_feet &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 2] == 10,
            "storm tornado composite and ROM position follow arc front edge");
      MmxSaberAttackResetRam(ram);
      MmxSaberProjectileReset(ram);
    }
  }
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_TORNADO, NULL, 0, 0,
                                   false, false, NULL, 0);
  MmxSaberProjectileReset(ram);
}

static void cutter_asset_checks(const MmxSaberAssets *assets) {
  enum {
    FORWARD_ANIMATION_ID = 16,
    DIAGONAL_ANIMATION_ID = 17,
    CUTTER_FRAME_COUNT = 4,
    CUTTER_TICKS_PER_FRAME = 3,
  };
  const MmxSaberAnimation *forward =
      MmxSaberAssetsAnimationById(assets, FORWARD_ANIMATION_ID);
  const MmxSaberAnimation *diagonal =
      MmxSaberAssetsAnimationById(assets, DIAGONAL_ANIMATION_ID);
  MmxSaberProjectileFrame forward_frames[CUTTER_FRAME_COUNT];
  MmxSaberProjectileFrame diagonal_frames[CUTTER_FRAME_COUNT];
  MmxRenderWorldSprite actual[2];
  TestOpaqueRect rect;
  const uint16_t *palette = MmxSaberAssetsPalette(assets);
  const uint16_t palette_count = MmxSaberAssetsPaletteCount(assets);
  uint8_t ram[0x20000] = {0};
  MmxSaberProjectileSpawnInfo info = {0};
  unsigned index;

  test_zero_state = (MmxZeroState){0};
  test_arms = false;
  test_black_zero = false;

  check(forward && forward->step_count == CUTTER_FRAME_COUNT &&
            forward->total_ticks == CUTTER_FRAME_COUNT * CUTTER_TICKS_PER_FRAME &&
            diagonal && diagonal->step_count == CUTTER_FRAME_COUNT &&
            diagonal->total_ticks == CUTTER_FRAME_COUNT * CUTTER_TICKS_PER_FRAME,
        "loaded asset exposes four three-tick forward and diagonal cutter frames");
  for (unsigned i = 0; i < CUTTER_FRAME_COUNT; ++i) {
    const MmxSaberStep *forward_step = MmxSaberAssetsAnimationStep(
        assets, FORWARD_ANIMATION_ID, i);
    const MmxSaberFrame *forward_frame = MmxSaberAssetsFrameForStep(
        assets, FORWARD_ANIMATION_ID, i);
    const MmxSaberStep *diagonal_step = MmxSaberAssetsAnimationStep(
        assets, DIAGONAL_ANIMATION_ID, i);
    const MmxSaberFrame *diagonal_frame = MmxSaberAssetsFrameForStep(
        assets, DIAGONAL_ANIMATION_ID, i);
    check(forward_step && forward_step->duration_ticks == CUTTER_TICKS_PER_FRAME &&
              forward_frame && forward_frame->body.pixels &&
              forward_frame->body.width == 32 && forward_frame->body.height == 12 &&
              forward_frame->body.origin_x == 16 &&
              forward_frame->body.origin_y == 6 &&
              forward_frame->source_id == 17 &&
              forward_frame->source_frame_id == i,
          "forward cutter frame keeps 32x12 centre-origin geometry");
    check(diagonal_step && diagonal_step->duration_ticks == CUTTER_TICKS_PER_FRAME &&
              diagonal_frame && diagonal_frame->body.pixels &&
              diagonal_frame->body.width == 26 && diagonal_frame->body.height == 24 &&
              diagonal_frame->body.origin_x == 13 &&
              diagonal_frame->body.origin_y == 12 &&
              diagonal_frame->source_id == 17 &&
              diagonal_frame->source_frame_id == i + CUTTER_FRAME_COUNT,
          "diagonal cutter frame keeps 26x24 centre-origin geometry");
    forward_frames[i] = (MmxSaberProjectileFrame){
      forward_frame->body.pixels,
      forward_frame->body.width,
      forward_frame->body.height,
      forward_frame->body.origin_x,
      forward_frame->body.origin_y,
    };
    diagonal_frames[i] = (MmxSaberProjectileFrame){
      diagonal_frame->body.pixels,
      diagonal_frame->body.width,
      diagonal_frame->body.height,
      diagonal_frame->body.origin_x,
      diagonal_frame->body.origin_y,
    };
  }

  MmxSaberProjectileReset(ram);
  MmxSaberRenderSetWave(NULL);
  MmxSaberRenderSetProjectileStrip(
      SABER_PROJECTILE_CUTTER, forward_frames, CUTTER_FRAME_COUNT,
      CUTTER_TICKS_PER_FRAME, false, true, palette, palette_count);
  MmxSaberRenderSetProjectileStrip(
      SABER_PROJECTILE_CUTTER_AIR, diagonal_frames, CUTTER_FRAME_COUNT,
      CUTTER_TICKS_PER_FRAME, false, true, palette, palette_count);
  info.kind = SABER_PROJECTILE_CUTTER;
  info.x = 321 * 256;
  info.y = 123 * 256;
  info.lifetime = 20;
  check(MmxSaberProjectileSpawn(ram, &info, &index),
        "forward cutter projectile spawns for loaded strip");
  info.kind = SABER_PROJECTILE_CUTTER_AIR;
  info.x = 333 * 256;
  check(MmxSaberProjectileSpawn(ram, &info, &index),
        "air cutter projectile spawns for loaded strip");
  check(MmxSaberRenderProvideWorldSprites(actual, 2) == 2 &&
            actual[0].pixels == forward_frames[0].pixels &&
            actual[0].width == 32 && actual[0].height == 12 &&
            actual[0].origin_x == -16 && actual[0].origin_y == -6 &&
            actual[0].palette == palette && actual[0].palette_count == palette_count &&
            actual[1].pixels == diagonal_frames[0].pixels &&
            actual[1].width == 26 && actual[1].height == 24 &&
            actual[1].origin_x == -13 && actual[1].origin_y == -12 &&
            actual[1].palette == palette && actual[1].palette_count == palette_count,
        "loaded cutter strips register forward and diagonal frame zero");
  check(actual[0].facing_left && actual[1].facing_left &&
            compose_world_opaque_rect(&actual[0], &rect) &&
            rect.min_x == 305 && rect.max_x == 337 && rect.min_y == 117 &&
            rect.max_y == 129 && compose_world_opaque_rect(&actual[1], &rect) &&
            rect.min_x == 321 && rect.max_x == 346 && rect.min_y == 111 &&
            rect.max_y == 135 && compose_world_has_opaque_pixel(&actual[1], 343, 111),
        "left cutters are centred and air cutter points down-forward");
  MmxSaberProjectileReset(ram);
  info.facing = 0x40;
  info.kind = SABER_PROJECTILE_CUTTER;
  info.x = 321 * 256;
  check(MmxSaberProjectileSpawn(ram, &info, &index),
        "right-facing ground cutter spawns for composite check");
  info.kind = SABER_PROJECTILE_CUTTER_AIR;
  info.x = 333 * 256;
  check(MmxSaberProjectileSpawn(ram, &info, &index),
        "right-facing air cutter spawns for composite check");
  check(MmxSaberRenderProvideWorldSprites(actual, 2) == 2 &&
            !actual[0].facing_left && !actual[1].facing_left &&
            compose_world_opaque_rect(&actual[0], &rect) &&
            rect.min_x == 305 && rect.max_x == 337 && rect.min_y == 117 &&
            rect.max_y == 129 && compose_world_opaque_rect(&actual[1], &rect) &&
            rect.min_x == 320 && rect.max_x == 345 && rect.min_y == 111 &&
            rect.max_y == 135 && compose_world_has_opaque_pixel(&actual[1], 322, 111),
        "right cutters are centred and air cutter points down-forward");
  for (unsigned i = 0; i < CUTTER_TICKS_PER_FRAME; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 2) == 2 &&
            actual[0].pixels == forward_frames[1].pixels &&
            actual[1].pixels == diagonal_frames[1].pixels,
        "loaded cutter strips advance after three ticks");
  for (unsigned i = 0; i < CUTTER_TICKS_PER_FRAME * 3; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 2) == 2 &&
            actual[0].pixels == forward_frames[0].pixels &&
            actual[1].pixels == diagonal_frames[0].pixels,
        "loaded cutter strips loop after four frames");
  {
    static uint8_t collision_rom[0x40000];
    const int zero_x = 321;
    const int zero_y = 123;
    /* MmxRenderer passes RAM Zero y plus MmxZeroPoseOffsetY(). The standing
     * pose in this RAM is -8, so use the same anchor as player_overlay_row. */
    const int render_zero_y = zero_y - 8;
    for (unsigned facing = 0; facing < 2; ++facing) {
      const uint8_t native_facing = facing ? 0x40 : 0;
      const uint8_t direction = facing ? 1 : 2;
      MmxSaberProjectileLiveInfo live[1];
      MmxRenderWorldSprite placed[1];
      TestOpaqueRect placed_rect;
      MmxSaberAttackSnapshot snapshot;
      const MmxSaberProjectile *projectile;
      uint16_t drawn_source_frame;
      int drawn_blade_center;
      const int expected_x = zero_x + (facing ? 30 : -30);
      int expected_y;

      memset(ram, 0, sizeof(ram));
      memset(collision_rom, 0xff, sizeof(collision_rom));
      put_u16(ram, 0x0bad, (unsigned)zero_x);
      put_u16(ram, 0x0bb0, (unsigned)zero_y);
      ram[0x0bd3] = 4;
      MmxSaberProjectileReset(ram);
      MmxSaberAttackResetRam(ram);
      MmxSaberProjectileCollisionRom(collision_rom, sizeof(collision_rom));
      MmxSaberAttackStepWithWallAndDashRequested(
          true, SABER_KIND_GROUND2, true, false, false, false, false, true,
          false, true, ram, native_facing, direction);
      for (unsigned tick = 0; tick < 6; ++tick)
        MmxSaberAttackStepWithWallAndDashRequested(
            false, SABER_KIND_NONE, true, false, false, false, false, true,
            false, true, ram, native_facing, direction);
      snapshot = MmxSaberAttackSnapshotGet();
      drawn_blade_center = drawn_ground_blade_center_y(
          assets, snapshot, zero_x, render_zero_y, native_facing,
          &drawn_source_frame);
      expected_y = drawn_blade_center;
      check(snapshot.kind == SABER_KIND_GROUND2 && snapshot.tick == 6 &&
                snapshot.anim_id == 2 && snapshot.anim_step == 3 &&
                drawn_source_frame == 3 && expected_y == zero_y - 1 &&
                expected_y != zero_y + 7,
            "spawn tick draws animation 2 frame 1019 at renderer Zero y - 8");
      check(MmxSaberProjectileLive(live, 1) == 1 &&
                live[0].kind == SABER_PROJECTILE_CUTTER &&
                live[0].world_x == expected_x && live[0].world_y == expected_y,
            "ground cutter attack follows drawn blade centre for both facings");
      projectile = MmxSaberProjectileGet(0);
      check(MmxSaberRenderProvideWorldSprites(placed, 1) == 1 &&
                placed[0].world_x == expected_x &&
                compose_world_opaque_rect(&placed[0], &placed_rect) &&
                (placed_rect.min_y + placed_rect.max_y + 1) / 2 ==
                    drawn_blade_center &&
                placed_rect.min_y == expected_y - 6 &&
                placed_rect.max_y == expected_y + 6 && projectile &&
                (ram[projectile->slot + 8] |
                 ((unsigned)ram[projectile->slot + 9] << 8)) ==
                    (unsigned)expected_y &&
                collision_rom[MmxSaberProjectileCollisionOffset(0)] == 0 &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 1] == 0 &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 2] == 16 &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 3] == 6,
            "ground cutter composite and ROM box follow measured blade centre");
      MmxSaberAttackResetRam(ram);
      MmxSaberProjectileReset(ram);
    }
  }
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_CUTTER, NULL, 0, 0,
                                   false, false, NULL, 0);
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_CUTTER_AIR, NULL, 0, 0,
                                   false, false, NULL, 0);
  MmxSaberProjectileReset(ram);
}

static void ice_clone_asset_checks(const MmxSaberAssets *assets) {
  enum {
    ICE_CLONE_ANIMATION_ID = 18,
    ICE_CLONE_FRAME_COUNT = 3,
    ICE_CLONE_TICKS_PER_FRAME = 3,
    ICE_CLONE_WIDTH = 94,
    ICE_CLONE_HEIGHT = 38,
    ICE_CLONE_ORIGIN_X = 94,
    ICE_CLONE_ORIGIN_Y = 37,
  };
  const MmxSaberAnimation *animation =
      MmxSaberAssetsAnimationById(assets, ICE_CLONE_ANIMATION_ID);
  MmxSaberProjectileFrame frames[ICE_CLONE_FRAME_COUNT];
  MmxRenderWorldSprite actual[1];
  TestOpaqueRect rect;
  const uint16_t *palette = MmxSaberAssetsPalette(assets);
  const uint16_t palette_count = MmxSaberAssetsPaletteCount(assets);
  uint8_t ram[0x20000] = {0};
  MmxSaberProjectileSpawnInfo info = {0};
  unsigned index;

  check(animation && animation->step_count == ICE_CLONE_FRAME_COUNT &&
            animation->total_ticks ==
                ICE_CLONE_FRAME_COUNT * ICE_CLONE_TICKS_PER_FRAME,
        "loaded asset exposes three three-tick ice clone frames");
  for (unsigned i = 0; i < ICE_CLONE_FRAME_COUNT; ++i) {
    const MmxSaberStep *step = MmxSaberAssetsAnimationStep(
        assets, ICE_CLONE_ANIMATION_ID, i);
    const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(
        assets, ICE_CLONE_ANIMATION_ID, i);
    check(step && step->duration_ticks == ICE_CLONE_TICKS_PER_FRAME &&
              frame && frame->body.pixels &&
              frame->body.width == ICE_CLONE_WIDTH &&
              frame->body.height == ICE_CLONE_HEIGHT &&
              frame->body.origin_x == ICE_CLONE_ORIGIN_X &&
              frame->body.origin_y == ICE_CLONE_ORIGIN_Y &&
              frame->source_id == 18 && frame->source_frame_id == i &&
              frame->blade.pixels == NULL && frame->blade.width == 0 &&
              frame->blade.height == 0 && frame->blade_layer == 0,
          "ice clone frame keeps 94x38 bottom-right origin geometry");
    frames[i] = (MmxSaberProjectileFrame){
      frame->body.pixels,
      frame->body.width,
      frame->body.height,
      frame->body.origin_x,
      frame->body.origin_y,
    };
  }

  MmxSaberProjectileReset(ram);
  MmxSaberRenderSetWave(NULL);
  MmxSaberRenderSetProjectileStrip(
      SABER_PROJECTILE_ICE_CLONE, frames, ICE_CLONE_FRAME_COUNT,
      ICE_CLONE_TICKS_PER_FRAME, true, true, palette, palette_count);
  info.kind = SABER_PROJECTILE_ICE_CLONE;
  info.x = 142 * 256;
  info.y = 160 * 256;
  info.lifetime = 20;
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[0].pixels &&
            actual[0].width == ICE_CLONE_WIDTH &&
            actual[0].height == ICE_CLONE_HEIGHT &&
            actual[0].origin_x == -ICE_CLONE_ORIGIN_X &&
            actual[0].origin_y == -ICE_CLONE_ORIGIN_Y &&
            actual[0].palette == palette &&
            actual[0].palette_count == palette_count,
        "loaded ice clone strip registers frame zero with palette and origin");
  check(actual[0].facing_left && compose_world_opaque_rect(&actual[0], &rect) &&
            rect.min_x == 142 && rect.max_x == 221 && rect.min_y == 125 &&
            rect.max_y == 161 && rect.max_y - 1 == 160,
        "left ice clone leading edge is at projectile point and opaque bottom is on feet");
  MmxSaberProjectileReset(ram);
  info.facing = 0x40;
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            !actual[0].facing_left && compose_world_opaque_rect(&actual[0], &rect) &&
            rect.min_x == 63 && rect.max_x == 142 && rect.min_y == 125 &&
            rect.max_y == 161 && rect.max_y - 1 == 160,
        "right ice clone leading edge is at projectile point and opaque bottom is on feet");
  for (unsigned i = 0; i < ICE_CLONE_TICKS_PER_FRAME; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[1].pixels &&
            compose_world_opaque_rect(&actual[0], &rect) &&
            rect.max_y - 1 == 160,
        "ice clone frame two keeps its opaque bottom on feet");
  for (unsigned i = 0; i < ICE_CLONE_TICKS_PER_FRAME; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[2].pixels &&
            compose_world_opaque_rect(&actual[0], &rect) &&
            rect.max_y - 1 == 160,
        "ice clone frame three keeps its opaque bottom on feet");
  for (unsigned i = 0; i < ICE_CLONE_TICKS_PER_FRAME; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == frames[2].pixels &&
            compose_world_opaque_rect(&actual[0], &rect) &&
            rect.max_y - 1 == 160,
        "ice clone held frame keeps its opaque bottom on feet");
  {
    static uint8_t collision_rom[0x40000];
    const int zero_x = 321;
    const int zero_y = 123;

    test_zero_state = (MmxZeroState){0};
    test_arms = false;
    test_black_zero = false;
    for (unsigned facing = 0; facing < 2; ++facing) {
      const uint8_t native_facing = facing ? 0x40 : 0;
      const uint8_t direction = facing ? 1 : 2;
      const int drawn_feet = drawn_zero_feet_y(
          assets, zero_x, zero_y, native_facing);
      const int expected_x = zero_x + (facing ? 58 : -58);
      MmxSaberProjectileLiveInfo live[1];
      MmxRenderWorldSprite placed[1];
      TestOpaqueRect placed_rect;
      const MmxSaberProjectile *projectile;

      memset(ram, 0, sizeof(ram));
      memset(collision_rom, 0xff, sizeof(collision_rom));
      put_u16(ram, 0x0bad, (unsigned)zero_x);
      put_u16(ram, 0x0bb0, (unsigned)zero_y);
      ram[0x1f96] = 0x40;
      put_u16(ram, 0x1f95, 0x5c00);
      MmxSaberProjectileReset(ram);
      MmxSaberAttackResetRam(ram);
      MmxSaberProjectileCollisionRom(collision_rom, sizeof(collision_rom));
      MmxSaberAttackStepWithWallAndDash(
          true, true, false, true, false, true, true, false, true, ram,
          native_facing, direction);
      MmxSaberAttackRuntimeTick(ram);
      check(MmxSaberProjectileLive(live, 1) == 1 &&
                live[0].kind == SABER_PROJECTILE_ICE_CLONE &&
                live[0].world_x == expected_x &&
                live[0].world_y == drawn_feet,
            "ice clone attack uses drawn Zero feet for both facings");
      projectile = MmxSaberProjectileGet(0);
      check(MmxSaberRenderProvideWorldSprites(placed, 1) == 1 &&
                placed[0].world_x == expected_x &&
                compose_world_opaque_rect(&placed[0], &placed_rect) &&
                (facing ? placed_rect.max_x == expected_x :
                          placed_rect.min_x == expected_x) &&
                placed_rect.max_y - 1 == drawn_feet && projectile &&
                (ram[projectile->slot + 8] |
                 ((unsigned)ram[projectile->slot + 9] << 8)) ==
                    (unsigned)drawn_feet &&
                collision_rom[MmxSaberProjectileCollisionOffset(0)] == 47 &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 1] ==
                    (uint8_t)-18 &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 2] == 47 &&
                collision_rom[MmxSaberProjectileCollisionOffset(0) + 3] == 19,
            "ice clone composite and ROM box follow opaque art bottom at feet");
      MmxSaberAttackResetRam(ram);
      MmxSaberProjectileReset(ram);
    }
  }
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_ICE_CLONE, NULL, 0, 0,
                                   false, false, NULL, 0);
  MmxSaberProjectileReset(ram);
}

static void thunder_slam_asset_checks(const MmxSaberAssets *assets) {
  enum {
    THUNDER_SLAM_ANIMATION_ID = 19,
    THUNDER_SLAM_FRAME_COUNT = 14,
    THUNDER_ORB_ANIMATION_ID = 20,
    THUNDER_ORB_FRAME_COUNT = 2,
    THUNDER_TICKS_PER_FRAME = 3,
    THUNDER_SLAM_WIDTH = 128,
    THUNDER_SLAM_HEIGHT = 128,
    THUNDER_SLAM_ORIGIN_X = 0,
    THUNDER_SLAM_ORIGIN_Y = 0,
    THUNDER_ORB_WIDTH = 32,
    THUNDER_ORB_HEIGHT = 32,
    THUNDER_ORB_ORIGIN_X = 16,
    THUNDER_ORB_ORIGIN_Y = 16,
  };
  const MmxSaberAnimation *slam =
      MmxSaberAssetsAnimationById(assets, THUNDER_SLAM_ANIMATION_ID);
  const MmxSaberAnimation *orb =
      MmxSaberAssetsAnimationById(assets, THUNDER_ORB_ANIMATION_ID);
  MmxSaberProjectileFrame orb_frames[THUNDER_ORB_FRAME_COUNT];
  MmxRenderWorldSprite actual[1];
  MmxRenderPlayerOverlay overlay;
  MmxSaberAttackSnapshot snapshot = {
    .kind = SABER_KIND_THUNDER_SLAM,
    .index = 0,
    .phase = SABER_PHASE_ACTIVE,
    .tick = 0,
    .anim_id = THUNDER_SLAM_ANIMATION_ID,
    .anim_step = 0,
    .facing = 0x40,
  };
  TestOpaqueRect rect;
  const uint16_t *palette = MmxSaberAssetsPalette(assets);
  const uint16_t palette_count = MmxSaberAssetsPaletteCount(assets);
  uint8_t ram[0x20000] = {0};
  MmxSaberProjectileSpawnInfo info = {0};
  unsigned index;

  check(slam && slam->step_count == THUNDER_SLAM_FRAME_COUNT &&
            slam->total_ticks == THUNDER_SLAM_FRAME_COUNT * THUNDER_TICKS_PER_FRAME &&
            orb && orb->step_count == THUNDER_ORB_FRAME_COUNT &&
            orb->total_ticks == THUNDER_ORB_FRAME_COUNT * THUNDER_TICKS_PER_FRAME,
        "loaded asset exposes 14 three-tick thunder slam frames and two orb frames");
  thunder_slam_floor_checks(assets);
  for (unsigned i = 0; i < THUNDER_SLAM_FRAME_COUNT; ++i) {
    const MmxSaberStep *step = MmxSaberAssetsAnimationStep(
        assets, THUNDER_SLAM_ANIMATION_ID, i);
    const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(
        assets, THUNDER_SLAM_ANIMATION_ID, i);
    check(step && step->duration_ticks == THUNDER_TICKS_PER_FRAME &&
              frame && frame->body.pixels &&
              frame->body.width == THUNDER_SLAM_WIDTH &&
              frame->body.height == THUNDER_SLAM_HEIGHT &&
              frame->body.origin_x == THUNDER_SLAM_ORIGIN_X &&
              frame->body.origin_y ==
                  ((i >= 6 && i <= 9) ? -2 : THUNDER_SLAM_ORIGIN_Y) &&
              frame->source_id == 19 && frame->source_frame_id == i &&
              frame->blade.pixels == NULL && frame->blade.width == 0 &&
              frame->blade.height == 0 && frame->blade_layer == 0,
          "thunder slam body frame keeps 128x128 crop, grounded origin and no blade");
  }
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &overlay) &&
            !overlay.facing_left && overlay.body.origin_x == 0 &&
            overlay.body.origin_y == 0 &&
            compose_overlay_opaque_rect(&overlay.body, overlay.facing_left,
                                        200, 160, overlay.palette_count, &rect) &&
            rect.min_x == 182 && rect.max_x == 219 && rect.min_y == 139 &&
            rect.max_y == 184,
        "right thunder slam body keeps art anchor on Zero");
  snapshot.facing = 0;
  check(MmxSaberRenderResolveSnapshot(assets, snapshot, &overlay) &&
            overlay.facing_left &&
            compose_overlay_opaque_rect(&overlay.body, overlay.facing_left,
                                        200, 160, overlay.palette_count, &rect) &&
            rect.min_x == 181 && rect.max_x == 218 && rect.min_y == 139 &&
            rect.max_y == 184,
        "left thunder slam body mirrors around Zero");
  for (unsigned i = 0; i < THUNDER_ORB_FRAME_COUNT; ++i) {
    const MmxSaberStep *step = MmxSaberAssetsAnimationStep(
        assets, THUNDER_ORB_ANIMATION_ID, i);
    const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(
        assets, THUNDER_ORB_ANIMATION_ID, i);
    check(step && step->duration_ticks == THUNDER_TICKS_PER_FRAME &&
              frame && frame->body.pixels && frame->body.width == THUNDER_ORB_WIDTH &&
              frame->body.height == THUNDER_ORB_HEIGHT &&
              frame->body.origin_x == THUNDER_ORB_ORIGIN_X &&
              frame->body.origin_y == THUNDER_ORB_ORIGIN_Y &&
              frame->source_id == 20 && frame->source_frame_id == i &&
              frame->blade.pixels == NULL && frame->blade.width == 0 &&
              frame->blade.height == 0 && frame->blade_layer == 0,
          "thunder orb frame keeps 32x32 centre-origin geometry and no blade");
    orb_frames[i] = (MmxSaberProjectileFrame){
      frame->body.pixels,
      frame->body.width,
      frame->body.height,
      frame->body.origin_x,
      frame->body.origin_y,
    };
  }

  MmxSaberProjectileReset(ram);
  MmxSaberRenderSetWave(NULL);
  MmxSaberRenderSetProjectileStrip(
      SABER_PROJECTILE_THUNDER_ORB, orb_frames, THUNDER_ORB_FRAME_COUNT,
      THUNDER_TICKS_PER_FRAME, false, true, palette, palette_count);
  info.kind = SABER_PROJECTILE_THUNDER_ORB;
  info.x = 218 * 256;
  info.y = 141 * 256;
  info.lifetime = 20;
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == orb_frames[0].pixels &&
            actual[0].width == THUNDER_ORB_WIDTH &&
            actual[0].height == THUNDER_ORB_HEIGHT &&
            actual[0].origin_x == -THUNDER_ORB_ORIGIN_X &&
            actual[0].origin_y == -THUNDER_ORB_ORIGIN_Y &&
            actual[0].palette == palette &&
            actual[0].palette_count == palette_count,
        "registered thunder orb strip outputs frame zero with palette and origin");
  for (unsigned i = 0; i < THUNDER_TICKS_PER_FRAME; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == orb_frames[1].pixels,
        "registered thunder orb strip advances after three ticks");
  for (unsigned i = 0; i < THUNDER_TICKS_PER_FRAME; ++i)
    MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 1) == 1 &&
            actual[0].pixels == orb_frames[0].pixels,
        "registered thunder orb strip loops after two frames");
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_THUNDER_ORB, NULL, 0, 0,
                                   false, false, NULL, 0);
  MmxSaberProjectileReset(ram);
}

static void projectile_provider_checks(const MmxSaberWave *wave) {
  static const uint8_t frame_zero_pixels[] = {1};
  static const uint8_t frame_one_pixels[] = {2};
  static const uint16_t palette[] = {0, 0x001f, 0x03e0};
  const MmxSaberProjectileFrame frames[] = {
    {frame_zero_pixels, 1, 1, 0, 0},
    {frame_one_pixels, 1, 1, 2, -1},
  };
  uint8_t ram[0x20000] = {0};
  MmxRenderWorldSprite actual[8];
  MmxSaberProjectileSpawnInfo info = {0};
  const unsigned wave_slot = MMX_SABER_PROJECTILE_SLOT_FIRST;
  unsigned index;

  MmxSaberWaveRuntimeReset(ram);
  MmxSaberProjectileReset(ram);
  ram[MMX_SABER_PROJECTILE_STAGE] = 4;
  put_u16(ram, MMX_SABER_PROJECTILE_CAMERA, 0);
  ram[wave_slot] = 1;
  put_u16(ram, wave_slot + 5, 300);
  put_u16(ram, wave_slot + 8, 120);
  ram[wave_slot + 0x2a] = 0;
  ram[wave_slot + 0x11] = 0x40;
  put_u16(ram, wave_slot + 0x3e, MMX_SABER_WAVE_TAG_FAMILY | 1);

  info.kind = SABER_PROJECTILE_CUTTER;
  info.x = 321 * 256;
  info.y = 123 * 256;
  info.lifetime = 20;
  info.facing = 0x40;
  info.fade_level = 6;
  check(MmxSaberProjectileSpawn(ram, &info, &index),
        "synthetic cutter projectile spawns for render provider");
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_CUTTER, frames, 2, 2,
                                    false, false, palette, 3);
  MmxSaberRenderSetWave(wave);
  check(MmxSaberRenderProvideWorldSprites(actual, 8) == 2 &&
            actual[0].world_x == 300 && actual[0].facing_left,
        "live waves remain first in world-sprite provider output");
  check(actual[1].pixels == frame_zero_pixels && actual[1].width == 1 &&
            actual[1].height == 1 && actual[1].origin_x == 0 &&
            actual[1].origin_y == 0 && actual[1].world_x == 321 &&
            actual[1].world_y == 123 && actual[1].facing_left &&
            actual[1].palette == palette && actual[1].palette_count == 3 &&
            actual[1].z == 0xa680 && actual[1].dither_drop == 6,
        "registered projectile outputs selected frame, position, facing and fade");

  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 8) == 2 &&
            actual[1].pixels == frame_one_pixels,
        "projectile frame advances after ticks_per_frame");

  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_CUTTER, frames, 2, 2,
                                   true, false, palette, 3);
  MmxSaberProjectileTick(ram);
  MmxSaberProjectileTick(ram);
  check(MmxSaberRenderProvideWorldSprites(actual, 8) == 2 &&
            actual[1].pixels == frame_one_pixels,
        "hold_last clamps projectile frame at strip end");

  MmxSaberProjectileReset(ram);
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_CUTTER, NULL, 0, 0,
                                   false, false, NULL, 0);
  info.kind = SABER_PROJECTILE_TORNADO;
  check(MmxSaberProjectileSpawn(ram, &info, &index),
        "unregistered projectile kind spawns for provider filter");
  check(MmxSaberRenderProvideWorldSprites(actual, 8) == 1 &&
            actual[0].world_x == 300,
        "unregistered projectile kind outputs no sprite");

  MmxSaberProjectileReset(ram);
  MmxSaberRenderSetWave(NULL);
  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_TORNADO, frames, 2, 2,
                                   false, false, palette, 3);
  check(MmxSaberProjectileSpawn(ram, &info, &index) &&
            MmxSaberRenderProvideWorldSprites(actual, 8) == 1 &&
            actual[0].world_x == 321,
        "projectiles draw when no wave asset is registered");

  MmxSaberRenderSetProjectileStrip(SABER_PROJECTILE_TORNADO, NULL, 0, 0,
                                   false, false, NULL, 0);
  MmxSaberProjectileReset(ram);
  MmxSaberWaveRuntimeReset(ram);
}

static void uppercut_attack_checks(void) {
  uint8_t ram[0x20000] = {0};
  MmxSaberAttackSnapshot snapshot;
  bool steps_ok = true;
  bool rising_step_ok = true;
  const MmxSaberAttack *attack =
      MmxSaberAttackRecord(SABER_KIND_UPPERCUT, 0);

  ram[0x1f90] = 0x40; /* Storm Eagle unlocks the uppercut */
  ram[0x0bd3] = 4;
  check(attack && attack->visual_animation == 8 &&
            attack->startup_ticks == 4 && attack->active_ticks == 26 &&
            attack->recovery_ticks == 0 && attack->total_ticks == 30,
        "uppercut record keeps animation 8 and startup/active-only timing");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, false, true, false, true, ram, 0, 0);
  for (unsigned i = 0; i < attack->total_ticks; ++i) {
    snapshot = MmxSaberAttackSnapshotGet();
    const uint8_t expected_step = i < 6 ? (uint8_t)(i / 2) : 3;
    if (snapshot.kind != SABER_KIND_UPPERCUT ||
        snapshot.anim_step != expected_step)
      steps_ok = false;
    if (i + 1 == attack->total_ticks) break;
    MmxSaberAttackStepWithWallAndDash(
        false, true, false, false, true, false, true, false, true, ram, 0, 0);
  }
  check(steps_ok && snapshot.kind == SABER_KIND_UPPERCUT &&
            snapshot.tick == 29 && snapshot.phase == SABER_PHASE_ACTIVE &&
            snapshot.anim_step == 3,
        "uppercut holds animation 8 step 3 through the rising attack ticks");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, false, true, false, true, ram, 0, 0);
  for (unsigned i = 0; i < 6; ++i)
    MmxSaberAttackStepWithWallAndDash(
        false, true, false, false, true, false, true, false, true, ram, 0, 0);
  ram[0x0bd3] = 0;
  ram[0x0baa] = 0x06;
  ram[0x0bc4] = 0;
  ram[0x0bc5] = 0;
  MmxSaberAttackPlayerEnd(ram);
  snapshot = MmxSaberAttackSnapshotGet();
  if (snapshot.kind != SABER_KIND_UPPERCUT || snapshot.anim_step != 5)
    rising_step_ok = false;
  check(rising_step_ok && snapshot.phase == SABER_PHASE_ACTIVE,
        "uppercut publishes animation 8 step 5 when native rise reaches apex");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, false, true, false, true, ram, 0, 0);
  MmxSaberAttackStepWithWallAndDash(
      false, true, false, false, true, false, false, false, true, ram, 0, 0);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_RELEASE,
        "uppercut Y release exits with MMX_SABER_ATTACK_EXIT_RELEASE");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, false, true, false, true, ram, 0, 0);
  MmxSaberAttackObservePreNative(ram);
  ram[0x0bd3] = 0;
  ram[0x0baa] = 0x06;
  put_u16(ram, 0x0bc4, 0x0100);
  MmxSaberAttackPlayerEnd(ram);
  check(MmxSaberAttackSnapshotGet().kind == SABER_KIND_UPPERCUT &&
            MmxSaberAttackSnapshotGet().phase != SABER_PHASE_IDLE,
        "uppercut own grounded-to-air jump does not context-cancel Saber");

  MmxSaberAttackObservePreNative(ram);
  ram[0x0baa] = 0x06;
  put_u16(ram, 0x0bc4, 0xffff);
  MmxSaberAttackPlayerEnd(ram);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_NATURAL,
        "uppercut exits naturally on native signed downward VY");

  ram[0x0bd3] = 4;
  ram[0x0baa] = 0;
  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, false, true, false, true, ram, 0, 0);
  MmxSaberAttackObservePreNative(ram);
  ram[0x0baa] = 0;
  MmxSaberAttackPlayerEnd(ram);

  /* Steps 0-1 stay grounded; take-off is tick 4.  A native refusal of the
   * jump is only judged on the take-off tick. */
  for (unsigned t = 1; t < 4; ++t) {
    MmxSaberAttackStepWithWallAndDash(
        false, true, false, false, true, false, true, false, true, ram, 0, 0);
    MmxSaberAttackObservePreNative(ram);
    ram[0x0baa] = 0;
    MmxSaberAttackPlayerEnd(ram);
  }
  check(MmxSaberAttackSnapshotGet().phase != SABER_PHASE_IDLE,
        "uppercut stays live on the ground before take-off");
  MmxSaberAttackStepWithWallAndDash(
      false, true, false, false, true, false, true, false, true, ram, 0, 0);
  MmxSaberAttackObservePreNative(ram);
  ram[0x0baa] = 0;
  MmxSaberAttackPlayerEnd(ram);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_CONTEXT,
        "uppercut refused native action 6 exits with context on the take-off tick");
}

static unsigned render_test_saber_slot(const uint8_t *ram) {
  for (unsigned d = 0x1228; d < 0x1428; d += 64) {
    const unsigned tag = ram[d + 0x3e] | ((unsigned)ram[d + 0x3f] << 8);
    if (ram[d] && (tag & MMX_SABER_PROJECTILE_TAG_FAMILY_MASK) ==
                     MMX_SABER_PROJECTILE_TAG_FAMILY)
      return d;
  }
  return 0;
}

static void thrust_test_start(uint8_t *ram) {
  memset(ram, 0, 0x20000);
  for (unsigned i = 0; i < 4; ++i)
    ram[0x1f88 + i * 2] = 0x40;
  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, false, false, false, false, true, true, false, true, ram, 0, 0);
}

static void thrust_test_step(uint8_t *ram, bool y_held) {
  MmxSaberAttackStepWithWallAndDash(
      false, false, false, false, false, true, y_held, false, true, ram, 0, 0);
}

static unsigned thrust_test_vy(const uint8_t *ram) {
  return ram[0x0bc4] | ((unsigned)ram[0x0bc5] << 8);
}

static void thrust_attack_checks(void) {
  uint8_t ram[0x20000];
  const MmxSaberAttack *attack =
      MmxSaberAttackRecord(SABER_KIND_THRUST, 0);
  MmxSaberTuning saved_tuning;
  MmxSaberTuning *mutable_tuning;

  check(attack && attack->visual_animation == 13 &&
            attack->startup_ticks == 2 && attack->active_ticks == 253 &&
            attack->recovery_ticks == 0 && attack->total_ticks == 255,
        "thrust record publishes animation 13, startup 2, open-ended active 253, and clamped total 255");

  MmxSaberTuningLoad(NULL, NULL);
  {
    static const uint8_t expected_steps[] = {
      0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5,
      5, 2, 2, 3, 3, 4, 4, 5, 5, 2, 2
    };
    bool sequence_ok = true;

    thrust_test_start(ram);
    for (unsigned tick = 0;
         tick < sizeof(expected_steps) / sizeof(expected_steps[0]); ++tick) {
      const MmxSaberAttackSnapshot sequence_snapshot =
          MmxSaberAttackSnapshotGet();
      if (MmxSaberThrustAnimationStepForTick((uint8_t)tick) !=
              expected_steps[tick] ||
          sequence_snapshot.anim_step != expected_steps[tick])
        sequence_ok = false;
      if (tick + 1 < sizeof(expected_steps) / sizeof(expected_steps[0]))
        thrust_test_step(ram, true);
    }
    /* Mutation red: restore the old animation 13 hold in
     * animation_step_for_tick() in src/saber/mmx_saber_attack.c. */
    check(sequence_ok,
          "production edit shares the thrust animation step sequence through tick 21");
  }
  thrust_test_start(ram);
  thrust_test_step(ram, true);
  thrust_test_step(ram, true);
  for (unsigned i = 0; i < 12; ++i)
    thrust_test_step(ram, true);
  MmxSaberAttackSnapshot snapshot = MmxSaberAttackSnapshotGet();
  /* Mutation red: restore a permanently held final thrust step in
   * update_animation() in src/saber/mmx_saber_attack.c. */
  check(snapshot.kind == SABER_KIND_THRUST && snapshot.tick == 14 &&
            snapshot.phase == SABER_PHASE_ACTIVE && snapshot.anim_step == 3,
        "thrust reaches Enkoujin cell 3 at tick 14 without replaying wind-up");

  bool open_ended = true;
  for (unsigned i = 14; i < 60; ++i) {
    thrust_test_step(ram, true);
    snapshot = MmxSaberAttackSnapshotGet();
    if (snapshot.kind != SABER_KIND_THRUST ||
        snapshot.phase != SABER_PHASE_ACTIVE)
      open_ended = false;
  }
  check(open_ended && snapshot.tick == 60,
        "thrust has no natural exit from tick 14 through tick 60 while airborne");

  thrust_test_start(ram);
  thrust_test_step(ram, true);
  thrust_test_step(ram, true);
  thrust_test_step(ram, false);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_RELEASE,
        "thrust slash release exits with MMX_SABER_ATTACK_EXIT_RELEASE");

  thrust_test_start(ram);
  thrust_test_step(ram, true);
  MmxSaberAttackObservePreNative(ram);
  ram[0x0bd3] = 4;
  MmxSaberAttackPlayerEnd(ram);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_LANDING,
        "thrust landing exits with MMX_SABER_ATTACK_EXIT_LANDING");

  MmxSaberTuningLoad(NULL, NULL);
  thrust_test_start(ram);
  thrust_test_step(ram, true);
  thrust_test_step(ram, true);
  MmxSaberAttackRuntimeTick(ram);
  unsigned slot = render_test_saber_slot(ram);
  const unsigned enemy = 0xe68;
  ram[enemy] = 1;
  ram[enemy + 0x27] = 32;
  put_u16(ram, 0x0bc4, 0x0123);
  const unsigned damage = MmxSaberAttackDamage(ram, enemy, slot, 1);
  const unsigned enemy_bit = 1u << ((enemy - 0xe68) / 64);
  /* Mutation red: re-add the immediate hit-mask clear in
   * bounce_thrust_on_hit. */
  check(slot != 0 && damage == 4 && thrust_test_vy(ram) == 0x0420 &&
            (MmxSaberAttackHitSlots() & enemy_bit) &&
            MmxSaberAttackSnapshotGet().phase == SABER_PHASE_ACTIVE,
        "thrust hit writes VY 0x0420 at bounce height 60, keeps the hit mask, and stays active");

  put_u16(ram, 0x0bc4, 0x0123);
  MmxSaberAttackPlayerEnd(ram);
  check((MmxSaberAttackHitSlots() & enemy_bit) != 0,
        "thrust bounce keeps hit mask while VY is nonnegative");
  put_u16(ram, 0x0bc4, 0xffff);
  MmxSaberAttackPlayerEnd(ram);
  check((MmxSaberAttackHitSlots() & enemy_bit) == 0,
        "thrust bounce clears hit mask on first falling frame");

  saved_tuning = *MmxSaberTuningGet();
  mutable_tuning = (MmxSaberTuning *)(uintptr_t)MmxSaberTuningGet();
  mutable_tuning->thrust_bounce_height = 0;
  thrust_test_start(ram);
  thrust_test_step(ram, true);
  thrust_test_step(ram, true);
  MmxSaberAttackRuntimeTick(ram);
  slot = render_test_saber_slot(ram);
  ram[enemy] = 1;
  ram[enemy + 0x27] = 32;
  put_u16(ram, 0x0bc4, 0x0123);
  const unsigned no_bounce_damage = MmxSaberAttackDamage(ram, enemy, slot, 1);
  check(no_bounce_damage == 4 && thrust_test_vy(ram) == 0x0123 &&
            (MmxSaberAttackHitSlots() & enemy_bit),
        "thrust bounce height 0 leaves VY alone");
  *mutable_tuning = saved_tuning;
  MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_CONTEXT);
}

static const MmxSaberBoundsSegment *render_test_saber_segment(
    const MmxSaberAttack *attack, uint8_t tick, unsigned *index) {
  if (!attack) return NULL;
  if (attack->kind == SABER_KIND_THRUST) {
    const uint8_t step = MmxSaberThrustAnimationStepForTick(tick);
    if (step < 1 || (unsigned)step > attack->bounds_segment_count ||
        step > MMX_SABER_MAX_ACTIVE_SEGMENTS)
      return NULL;
    if (index) *index = step - 1;
    return attack->bounds_segments + step - 1;
  }
  for (unsigned i = 0; i < attack->bounds_segment_count; ++i) {
    const MmxSaberBoundsSegment *segment = attack->bounds_segments + i;
    if (tick >= segment->first_tick && tick <= segment->last_tick) {
      if (index) *index = i;
      return segment;
    }
  }
  return NULL;
}

static const MmxSaberBoundsSegment kRenderThrustFrameBounds[] = {
  {0, 1, 0, 0, 0, 0},
  {2, 3, 18, -10, 9, 14},
  {4, 5, 14, 14, 10, 25},
  {6, 7, 13, 11, 10, 25},
  {8, 9, 13, 8, 9, 29},
  {10, 11, 13, 10, 9, 26},
};

static const MmxSaberBoundsSegment *render_test_thrust_frame_bounds(
    uint8_t step) {
  return step < sizeof(kRenderThrustFrameBounds) /
                     sizeof(kRenderThrustFrameBounds[0]) ?
      kRenderThrustFrameBounds + step : kRenderThrustFrameBounds;
}

static void special_live_bounds_checks_one(MmxSaberPadKind kind,
                                            bool grounded,
                                            unsigned boss_clear,
                                            uint8_t direction,
                                            const char *label) {
  static uint8_t ram[0x20000];
  static uint8_t rom[0x40000];
  const MmxSaberAttack *attack = MmxSaberAttackRecord(kind, 0);
  bool seen[MMX_SABER_MAX_ACTIVE_SEGMENTS] = {false};
  bool held_tick_checked = false;
  const uint8_t native_facing = direction == 1 ? 0x40 : 0;
  const uint8_t expected_projectile_facing = direction == 1 ? 0 : 0x40;
  const bool down_held = kind == SABER_KIND_THRUST;

  memset(ram, 0, sizeof(ram));
  ram[boss_clear] = 0x40;
  ram[0x0bd3] = grounded ? 4 : 0;
  check(attack && attack->bounds_pointer == MMX_SABER_AIR_BOUNDS_POINTER,
        "special uses the shared AIR collision-record window");
  check(attack && attack->bounds_segment_count <=
            MMX_SABER_MAX_ACTIVE_SEGMENTS,
        "special bounds fit the live segment capacity");

  MmxSaberAttackResetRam(ram);
  memset(rom, 0xff, sizeof(rom));
  MmxSaberAttackCollisionRom(rom, sizeof(rom));
  MmxSaberAttackStepWithWallAndDash(
      true, grounded, false, false, true, down_held, true, false, true, ram,
      native_facing, direction);
  check(MmxSaberAttackSnapshotGet().kind == kind,
        "special starts through the normal attack selector");
  printf("reference: %s facing=%s segments=%u\n", label,
         direction == 1 ? "right" : "left", attack->bounds_segment_count);

  for (unsigned tick = 0; tick < attack->total_ticks; ++tick) {
    const MmxSaberAttackSnapshot snapshot = MmxSaberAttackSnapshotGet();
    unsigned segment_index = 0;
    const MmxSaberBoundsSegment *segment = render_test_saber_segment(
        attack, snapshot.tick, &segment_index);
    const MmxSaberBoundsSegment *record_segment =
        kind == SABER_KIND_THRUST && snapshot.phase == SABER_PHASE_ACTIVE ?
        render_test_thrust_frame_bounds(snapshot.anim_step) : segment;
    unsigned slot;

    MmxSaberAttackRuntimeTick(ram);
    slot = render_test_saber_slot(ram);
    if (!segment) {
      check(kind == SABER_KIND_THRUST && snapshot.phase == SABER_PHASE_ACTIVE ?
                slot != 0 : slot == 0,
            kind == SABER_KIND_THRUST && snapshot.phase == SABER_PHASE_ACTIVE ?
                "open-ended thrust keeps its owner live outside the placeholder art window" :
                "special startup without an art frame has no live slot");
    } else {
      const unsigned pointer = attack->bounds_pointer + segment_index * 4;
      const unsigned offset = 0x37f40 +
          (pointer - MMX_SABER_AIR_BOUNDS_POINTER);
      const uint8_t expected_record[4] = {
          (uint8_t)record_segment->bounds_x,
          (uint8_t)record_segment->bounds_y,
          record_segment->bounds_half_width, record_segment->bounds_half_height};
      check(!memcmp(rom + offset, expected_record, sizeof(expected_record)),
            "live AIR record equals the current art segment");
      seen[segment_index] = true;
      if (kind == SABER_KIND_THRUST && snapshot.tick == 40) {
        /* Mutation red: restore old tick-segment selection in
         * src/saber/mmx_saber_attack.c. */
        check(segment_index == 3 && segment->first_tick == 8 &&
                  segment->last_tick == 9 && segment->bounds_x == 13 &&
                  segment->bounds_y == 8 && segment->bounds_half_width == 9 &&
                  segment->bounds_half_height == 29 &&
                  record_segment->bounds_x == 13 &&
                  record_segment->bounds_y == 8 &&
                  record_segment->bounds_half_width == 9 &&
                  record_segment->bounds_half_height == 29,
              "production edit kThrustBounds selects the current Enkoujin flame box at tick 40");
        held_tick_checked = true;
      }

      if (snapshot.phase == SABER_PHASE_ACTIVE) {
        check(slot && (ram[slot + 0x11] & 0x40) ==
                  expected_projectile_facing,
              "active slot carries the native mirror facing");
        const int stored_x = rom[offset] < 128 ? rom[offset] : rom[offset] - 256;
        const int effective_x = (ram[slot + 0x11] & 0x40) ?
            -stored_x : stored_x;
        const int expected_x = direction == 1 ?
            (int)record_segment->bounds_x : -(int)record_segment->bounds_x;
        check(slot && (ram[slot + 0x20] == (uint8_t)pointer) &&
                  (ram[slot + 0x21] == (uint8_t)(pointer >> 8)) &&
                  effective_x == expected_x,
              "active slot points at the mirrored current segment");
      } else {
        check(slot == 0, "startup art record is not a live projectile yet");
      }
    }

    if (tick + 1 < attack->total_ticks)
      MmxSaberAttackStepWithWallAndDash(
          false, grounded, false, false, true, down_held, true, false, true,
          ram,
          native_facing, direction);
  }
  for (unsigned i = 0; i < attack->bounds_segment_count; ++i)
    check(seen[i], "every special art segment reaches the live AIR window");
  if (kind == SABER_KIND_THRUST)
    check(held_tick_checked,
          "production edit kThrustBounds reaches the tick-40 Enkoujin box in the live AIR window");
  MmxSaberAttackExit(ram, MMX_SABER_ATTACK_EXIT_CONTEXT);
}

static void special_live_bounds_checks(void) {
  special_live_bounds_checks_one(SABER_KIND_UPPERCUT, true, 0x1f90, 1,
                                 "uppercut");
  special_live_bounds_checks_one(SABER_KIND_UPPERCUT, true, 0x1f90, 2,
                                 "uppercut");
  special_live_bounds_checks_one(SABER_KIND_ROLLING, false, 0x1f8c, 1,
                                 "rolling slash");
  special_live_bounds_checks_one(SABER_KIND_ROLLING, false, 0x1f8c, 2,
                                 "rolling slash");
  /* Mutation red: point THRUST back to kAirSlashBounds in
   * src/saber/mmx_saber_attack.c. */
  special_live_bounds_checks_one(SABER_KIND_THRUST, false, 0x1f8e, 1,
                                 "thrust");
  special_live_bounds_checks_one(SABER_KIND_THRUST, false, 0x1f8e, 2,
                                 "thrust");
}

int main(void) {
  char path[4096];
  char ride_path[4096];
  char wave_path[4096];
  char reason[128] = {0};
  MmxSaberAssets *assets;
  MmxSaberAssets *ride;
  MmxSaberWave *wave;

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
  if (!join_path(ride_path, sizeof(ride_path), MMX_SABER_RENDER_CACHE_DIR,
                 "ride-zero-v1.bin")) {
    MmxSaberAssetsFree(assets);
    fprintf(stderr, "FAIL: Ride Armor cache path is not configured\n");
    return 1;
  }
  file = fopen(ride_path, "rb");
  if (!file) {
    printf("SKIPPED: private Ride Armor sidecar is absent (%s)\n", ride_path);
    MmxSaberAssetsFree(assets);
    return 77;
  }
  fclose(file);
  ride = MmxSaberAssetsLoadFile(ride_path, kRideManifestSha,
                                reason, sizeof(reason));
  if (!ride) {
    fprintf(stderr, "FAIL: Ride Armor sidecar rejected: %s\n",
            reason[0] ? reason : "unknown reason");
    MmxSaberAssetsFree(assets);
    return 1;
  }
  if (!join_path(wave_path, sizeof(wave_path), MMX_SABER_RENDER_CACHE_DIR,
                 "x3-saber-wave-v1.bin")) {
    MmxSaberAssetsFree(ride);
    MmxSaberAssetsFree(assets);
    fprintf(stderr, "FAIL: Saber wave cache path is not configured\n");
    return 1;
  }
  file = fopen(wave_path, "rb");
  if (!file) {
    printf("SKIPPED: private Saber wave sidecar is absent (%s)\n", wave_path);
    MmxSaberAssetsFree(ride);
    MmxSaberAssetsFree(assets);
    return 77;
  }
  fclose(file);
  wave = MmxSaberWaveLoadFile(wave_path, reason, sizeof(reason));
  if (!wave) {
    fprintf(stderr, "FAIL: Saber wave sidecar rejected: %s\n",
            reason[0] ? reason : "unknown reason");
    MmxSaberAssetsFree(ride);
    MmxSaberAssetsFree(assets);
    return 1;
  }

  for (unsigned animation = 0; animation < 7; ++animation)
    for (unsigned tuple = 0; tuple < 4; ++tuple)
      check_tuple(assets, animation, &kOldTuples[animation][tuple]);
  special_render_checks(assets);
  tornado_asset_checks(assets);
  cutter_asset_checks(assets);
  ice_clone_asset_checks(assets);
  thunder_slam_asset_checks(assets);
  ride_matrix_checks(ride);
  uppercut_attack_checks();
  thrust_attack_checks();
  special_live_bounds_checks();

  MmxSaberAttackReset();
  MmxSaberAttackStep(true, true, true, 0, 0);
  MmxRenderPlayerOverlay live;
  check(MmxSaberRenderResolve(assets, NULL, &live) && live.active &&
            live.body.origin_x == 46 && live.body.origin_y == 44,
        "live attack state resolves ground animation 1");
  MmxSaberAttackReset();
  check(!MmxSaberRenderResolve(assets, NULL, &live) && !live.active,
        "idle attack state resolves inactive");
  check(!MmxSaberRenderResolve(NULL, NULL, &live) && !live.active,
        "missing sidecar resolves inactive");
  wave_sequence_checks(wave);
  projectile_provider_checks(wave);

  MmxSaberAssets *flash_assets = make_flash_assets(false);
  check(flash_assets != NULL, "in-memory body-and-blade flash sidecar parses");
  charge_flash_checks(flash_assets);
  legacy_flash_palette_checks();
  black_zero_render_checks(assets, ride);
  shipped_purple_blade_checks(assets);
  purple_blade_render_checks(flash_assets);
  MmxSaberAssetsFree(flash_assets);

  MmxSaberWaveFree(wave);
  MmxSaberAssetsFree(ride);
  MmxSaberAssetsFree(assets);
  puts("MMX SABER RENDER CHECKS PASSED");
  return 0;
}

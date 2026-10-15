#include "mmx_saber_render.h"
#include "mmx_saber_armor.h"
#include "mmx_saber_black_zero.h"
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
MmxSaberArmorFlags MmxSaberArmorCurrent(void) {
  MmxSaberArmorFlags flags = {0};
  flags.black = test_black_zero;
  flags.arms = test_arms;
  return flags;
}
MmxZeroState MmxZeroGetState(void) { return test_zero_state; }
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
  0x99, 0x1d, 0x13, 0x43, 0xd8, 0x41, 0x07, 0xe7,
  0xa4, 0x23, 0x23, 0xc2, 0x71, 0x3b, 0xe3, 0xc2,
  0x12, 0xe3, 0x44, 0x32, 0x09, 0xfa, 0xdd, 0x1f,
  0x0a, 0xbe, 0x50, 0x8e, 0x90, 0x7e, 0x8f, 0xc5,
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
            !actual.facing_left,
        "resolver returns the exact donor planes, palette, layer, and facing");
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

static void uppercut_attack_checks(void) {
  uint8_t ram[0x20000] = {0};
  MmxSaberAttackSnapshot snapshot;
  const MmxSaberAttack *attack =
      MmxSaberAttackRecord(SABER_KIND_UPPERCUT, 0);

  ram[0x1f88] = 0x40;
  ram[0x1f8a] = 0x40;
  ram[0x0bd3] = 4;
  check(attack && attack->visual_animation == 8 &&
            attack->startup_ticks == 4 && attack->active_ticks == 26 &&
            attack->recovery_ticks == 0 && attack->total_ticks == 30,
        "uppercut record keeps animation 8 and startup/active-only timing");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, true, false, true, ram, 0, 0);
  for (unsigned i = 0; i < 14; ++i)
    MmxSaberAttackStepWithWallAndDash(
        false, true, false, false, true, true, false, true, ram, 0, 0);
  snapshot = MmxSaberAttackSnapshotGet();
  check(snapshot.kind == SABER_KIND_UPPERCUT && snapshot.tick == 14 &&
            snapshot.phase == SABER_PHASE_ACTIVE && snapshot.anim_step == 6,
        "uppercut holds animation 8 final frame after seven two-tick frames");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, true, false, true, ram, 0, 0);
  MmxSaberAttackStepWithWallAndDash(
      false, true, false, false, true, false, false, true, ram, 0, 0);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_RELEASE,
        "uppercut Y release exits with MMX_SABER_ATTACK_EXIT_RELEASE");

  MmxSaberAttackResetRam(ram);
  MmxSaberAttackStepWithWallAndDash(
      true, true, false, false, true, true, false, true, ram, 0, 0);
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
      true, true, false, false, true, true, false, true, ram, 0, 0);
  MmxSaberAttackObservePreNative(ram);
  ram[0x0baa] = 0;
  MmxSaberAttackPlayerEnd(ram);

  MmxSaberAttackStepWithWallAndDash(
      false, true, false, false, true, true, false, true, ram, 0, 0);
  MmxSaberAttackObservePreNative(ram);
  ram[0x0baa] = 0;
  MmxSaberAttackPlayerEnd(ram);
  check(MmxSaberAttackSnapshotGet().phase == SABER_PHASE_IDLE &&
            MmxSaberAttackLastExitReason() == MMX_SABER_ATTACK_EXIT_CONTEXT,
        "uppercut refused native action 6 exits with context after two frames");
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
  ride_matrix_checks(ride);
  uppercut_attack_checks();

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

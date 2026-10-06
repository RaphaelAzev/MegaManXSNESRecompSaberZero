#include "mmx_renderer.h"
#include "mmx_zero.h"
#include "saber/mmx_saber_assets.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MMX_SABER_RENDER_CACHE_DIR
#define MMX_SABER_RENDER_CACHE_DIR ""
#endif
#ifndef MMX_SABER_DRAW_ZERO_PATH
#define MMX_SABER_DRAW_ZERO_PATH "saber-render-draw-zero.bin"
#endif

static const uint8_t kSaberManifestSha[32] = {
  0x4e, 0x29, 0x1e, 0x5f, 0x03, 0x57, 0xaf, 0xa0,
  0x35, 0x76, 0x14, 0xe0, 0xc4, 0x97, 0xf9, 0xb3,
  0x65, 0xee, 0x99, 0xe3, 0x70, 0x64, 0x5d, 0x84,
  0x99, 0xb2, 0xe4, 0xfd, 0x07, 0xf8, 0x0f, 0xd0,
};

enum { ANCHOR_X = 40, ANCHOR_Y = 40, REGION_X = 32, REGION_Y = 34,
       REGION_WIDTH = 16, REGION_HEIGHT = 10 };

static uint8_t ram[0x20000], rom[0x100000];
static Ppu ppu;
static uint32_t stock[256 * 224], output[MMX_RENDER_MAX_WIDTH * 224];
static uint32_t right_output[256 * 224];
static MmxRenderPlayerOverlay provider_overlay;
static uint8_t body_pixels[] = {
  1, 2, 0, 0, 0,
  0, 3, 2, 0, 0,
  0, 0, 0, 3, 1,
};
static uint8_t blade_pixels[] = {1, 3, 1};
static const uint16_t overlay_palette[] = {0, 31, 31 << 5, 31 << 10};

static void check(bool okay, const char *message) {
  if (!okay) {
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
  }
  printf("ok: %s\n", message);
}

static void put_word(uint8_t *bytes, unsigned address, unsigned value) {
  bytes[address] = (uint8_t)value;
  bytes[address + 1] = (uint8_t)(value >> 8);
}

static uint32_t expand_color(uint16_t color) {
  unsigned red = color & 31, green = (color >> 5) & 31, blue = color >> 10;
  red = (red << 3) | (red >> 2);
  green = (green << 3) | (green >> 2);
  blue = (blue << 3) | (blue >> 2);
  return red << 16 | green << 8 | blue;
}

static uint32_t checksum_region(const uint32_t *pixels) {
  uint32_t checksum = 2166136261u;
  for (int y = REGION_Y; y < REGION_Y + REGION_HEIGHT; ++y)
    for (int x = REGION_X; x < REGION_X + REGION_WIDTH; ++x)
      checksum = (checksum ^ pixels[y * 256 + x]) * 16777619u;
  return checksum;
}

static bool provide_overlay(MmxRenderPlayerOverlay *out) {
  *out = provider_overlay;
  return true;
}

static void write_zero_asset(void) {
  FILE *file = fopen(MMX_SABER_DRAW_ZERO_PATH, "wb");
  const uint8_t header[] = {
    'M', 'M', 'X', 'Z', 'E', 'R', 'O', '6',
    128, 0, 128, 0, 64, 0, 64, 0, 117, 0, 35, 0,
  };
  uint8_t palette[256] = {0};
  uint8_t bounds[40] = {0};
  uint8_t hud[160] = {0};
  uint8_t animation[MMX_ZERO_ANIMATION_BYTES] = {0};
  uint8_t emission[MMX_ZERO_MUZZLE_BYTES] = {0};
  uint8_t page[MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT] = {0};

  check(file != NULL, "temporary native Zero cache opens");
  palette[2] = 31;
  for (unsigned i = 0; i < sizeof(bounds); i += 4) {
    bounds[i + 2] = 1;
    bounds[i + 3] = 1;
  }
  for (unsigned i = 0; i < 136; ++i) {
    animation[i * 2] = 0x10;
    animation[i * 2 + 1] = 1;
  }
  animation[0x110] = 1;
  page[64 * MMX_ZERO_WIDTH + 64] = 1;
  check(fwrite(header, sizeof(header), 1, file) == 1 &&
            fwrite(palette, sizeof(palette), 1, file) == 1 &&
            fwrite(bounds, sizeof(bounds), 1, file) == 1 &&
            fwrite(hud, sizeof(hud), 1, file) == 1 &&
            fwrite(animation, sizeof(animation), 1, file) == 1 &&
            fwrite(emission, sizeof(emission), 1, file) == 1,
        "temporary native Zero cache header writes");
  bool poses_written = true;
  for (unsigned pose = 0; pose < MMX_ZERO_POSES; ++pose) {
    poses_written &= fwrite(page, sizeof(page), 1, file) == 1;
    memset(page, 0, sizeof(page));
  }
  check(poses_written && fclose(file) == 0 &&
            MmxZeroLoad(MMX_SABER_DRAW_ZERO_PATH),
        "temporary native Zero cache loads");
  remove(MMX_SABER_DRAW_ZERO_PATH);
}

static void scene(void) {
  memset(&ppu, 0, sizeof(ppu));
  memset(ram, 0, sizeof(ram));
  memset(rom, 0, sizeof(rom));
  memset(stock, 0, sizeof(stock));
  MmxRendererSetPlayerOverlayProvider(NULL);
  MmxRendererReset();
  MmxRendererSetRom(rom, sizeof(rom));
  g_mmx_custom_renderer = true;
  g_mmx_expanded_sprites = false;
  g_mmx_render_asset_repairs = false;

  ram[0xd1] = 2;
  ram[0xd2] = 4;
  ram[0xd3] = 4;
  ram[0xba9] = 2;
  ram[0xbcf] = 16;
  ram[0xbbf] = 0;
  put_word(ram, 0xbad, ANCHOR_X);
  put_word(ram, 0xbb0, ANCHOR_Y + 8);
  put_word(ram, 0x1e4d, 0);
  put_word(ram, 0x1e50, 0);

  ppu.inidisp = 15;
  ppu.bgmode = 1;
  ppu.screenEnabled[0] = 16;
  for (unsigned i = 0; i < 128; ++i) ppu.oam[i * 2] = 0xe000;
  ppu.oam[32] = 0x2828;
  ppu.oam[33] = 0x2000;

  put_word(ram, 0, ANCHOR_X);
  put_word(ram, 2, ANCHOR_Y);
  put_word(ram, 0x18, 0x8000);
  ram[0x1a] = 0x80;
  ram[0xf] = 0x20;
  MmxRendererObserveObject(ram, 0xba8);
  MmxRendererRecordPiece(ram, 0);
  MmxRendererLatchSprites();
}

static void render(void) {
  memset(output, 0, sizeof(output));
  MmxRendererBeginFrame(ram);
  for (unsigned y = 1; y <= 224; ++y) MmxRendererCaptureLine(&ppu, y);
  check(MmxRendererEndFrame(stock), "offscreen renderer frame captures");
  check(MmxRendererDraw(output, (MmxRenderView){256, 0, 4.0 / 3.0}, false),
        "offscreen renderer frame draws");
}

static MmxRenderPlayerOverlay synthetic_overlay(bool left, uint8_t layer) {
  MmxRenderPlayerOverlay overlay = {0};
  overlay.active = true;
  overlay.body = (MmxRenderPlayerOverlayPlane){
    body_pixels, 5, 3, 61, 62};
  overlay.blade = (MmxRenderPlayerOverlayPlane){
    blade_pixels, 3, 1, 62, 63};
  overlay.blade_layer = layer;
  overlay.palette = overlay_palette;
  overlay.palette_count = sizeof(overlay_palette) / sizeof(overlay_palette[0]);
  overlay.facing_left = left;
  return overlay;
}

static void inactive_matches_baseline(void) {
  scene();
  render();
  memcpy(right_output, output, sizeof(right_output));

  scene();
  memset(&provider_overlay, 0, sizeof(provider_overlay));
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  render();
  check(!memcmp(right_output, output, sizeof(right_output)),
        "inactive overlay preserves the native renderer output exactly");
}

static void body_geometry_and_mirror(void) {
  scene();
  provider_overlay = synthetic_overlay(false, 0);
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  render();
  check(checksum_region(output) == 0xd95cc1edu,
        "right-facing donor body matches the golden region checksum");
  check(output[38 * 256 + 37] == 0xff0000 &&
            output[38 * 256 + 38] == 0x00ff00 &&
            output[39 * 256 + 38] == 0x0000ff &&
            output[40 * 256 + 40] == 0x0000ff,
        "right-facing donor body lands on the signed-origin pixel probes");
  memcpy(right_output, output, sizeof(right_output));

  scene();
  provider_overlay = synthetic_overlay(true, 0);
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  render();
  check(checksum_region(output) == 0xc8ca190du,
        "left-facing donor body matches the golden region checksum");
  check(output[38 * 256 + 42] == 0xff0000 &&
            output[38 * 256 + 41] == 0x00ff00 &&
            output[39 * 256 + 41] == 0x0000ff &&
            output[40 * 256 + 39] == 0x0000ff,
        "left-facing donor body lands on the mirrored pixel probes");
  for (int y = 0; y < 224; ++y) for (int x = 0; x < 256; ++x) {
    int mirror_x = 2 * ANCHOR_X - 1 - x;
    uint32_t expected = mirror_x >= 0 && mirror_x < 256 ?
        right_output[y * 256 + mirror_x] : 0;
    if (output[y * 256 + x] != expected) {
      check(false, "left-facing donor body is the exact anchor mirror");
      return;
    }
  }
  check(true, "left-facing donor body is the exact anchor mirror");
}

static void blade_order(void) {
  scene();
  provider_overlay = synthetic_overlay(false, 1);
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  render();
  check(output[39 * 256 + 38] == 0x0000ff &&
            output[39 * 256 + 39] == 0x00ff00 &&
            output[39 * 256 + 40] == 0xff0000,
        "layer-1 blade is behind body pixels and visible through transparency");

  scene();
  provider_overlay = synthetic_overlay(false, 2);
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  render();
  check(output[39 * 256 + 38] == 0xff0000 &&
            output[39 * 256 + 39] == 0x0000ff &&
            output[39 * 256 + 40] == 0xff0000,
        "layer-2 blade is in front of body pixels");
}

static void donor_origin_and_palette(const MmxSaberAssets *assets) {
  const MmxSaberFrame *frame = MmxSaberAssetsFrameForStep(assets, 1, 0);
  MmxRenderPlayerOverlay donor = {0};
  bool found = false;

  donor.active = true;
  donor.body = (MmxRenderPlayerOverlayPlane){
    frame->body.pixels, frame->body.width, frame->body.height,
    frame->body.origin_x, frame->body.origin_y};
  donor.palette = MmxSaberAssetsPalette(assets);
  donor.palette_count = MmxSaberAssetsPaletteCount(assets);
  /* The body-origin/palette probe is independent of blade overlap. Ordering is
   * covered by the controlled planes in blade_order(). */
  scene();
  provider_overlay = donor;
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  render();
  for (unsigned row = 0; row < frame->body.height && !found; ++row)
    for (unsigned col = 0; col < frame->body.width && !found; ++col) {
      unsigned pixel = frame->body.pixels[row * frame->body.width + col];
      int x = ANCHOR_X + frame->body.origin_x + (int)col - 64;
      int y = ANCHOR_Y - 64 + frame->body.origin_y + (int)row;
      if (pixel && pixel < donor.palette_count && x >= 0 && x < 256 &&
          y >= 0 && y < 224) {
        found = true;
        check(output[y * 256 + x] == expand_color(donor.palette[pixel]),
              "actual donor origin and palette reach the custom renderer");
      }
    }
  check(found, "actual donor frame exposes an in-view body pixel");
}

static void invisible_zero_is_empty(void) {
  scene();
  provider_overlay = synthetic_overlay(false, 2);
  MmxRendererSetPlayerOverlayProvider(provide_overlay);
  ppu.oam[32] = 0xe000;
  ppu.oam[33] = 0;
  render();
  bool any = false;
  for (int y = 0; y < 224 && !any; ++y)
    for (int x = 0; x < 256; ++x)
      any |= output[y * 256 + x] != 0;
  check(!any, "an invisible Zero OAM piece draws no overlay pixels");
}

int main(void) {
  char path[4096];
  char reason[128] = {0};
  MmxSaberAssets *assets;

  if (!MMX_SABER_RENDER_CACHE_DIR[0] ||
      snprintf(path, sizeof(path), "%s/%s", MMX_SABER_RENDER_CACHE_DIR,
               "saber-v1.bin") >= (int)sizeof(path)) {
    printf("SKIPPED: private Saber sidecar is not configured\n");
    return 77;
  }
  FILE *file = fopen(path, "rb");
  if (!file) {
    printf("SKIPPED: private Saber sidecar is absent (%s)\n", path);
    return 77;
  }
  fclose(file);
  assets = MmxSaberAssetsLoadFile(path, kSaberManifestSha, reason,
                                  sizeof(reason));
  if (!assets) {
    fprintf(stderr, "FAIL: Saber sidecar rejected: %s\n",
            reason[0] ? reason : "unknown reason");
    return 1;
  }
  check(MmxSaberAssetsFrameForStep(assets, 1, 0) != NULL,
        "private sidecar supplies the renderer donor frame");

  write_zero_asset();
  inactive_matches_baseline();
  body_geometry_and_mirror();
  blade_order();
  donor_origin_and_palette(assets);
  invisible_zero_is_empty();
  MmxSaberAssetsFree(assets);
  MmxRendererSetPlayerOverlayProvider(NULL);
  MmxZeroDisable();
  g_mmx_custom_renderer = false;
  g_mmx_render_asset_repairs = true;
  return 0;
}

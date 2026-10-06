#include "mmx_saber_render.h"

#include <string.h>

static void clear_overlay(MmxRenderPlayerOverlay *out) {
  if (out) memset(out, 0, sizeof(*out));
}

bool MmxSaberRenderResolveSnapshot(const MmxSaberAssets *assets,
                                   MmxSaberAttackSnapshot snapshot,
                                   MmxRenderPlayerOverlay *out) {
  const MmxSaberAttack *attack;
  const MmxSaberAnimation *animation;
  const MmxSaberFrame *frame = NULL;
  uint16_t tick;

  clear_overlay(out);
  if (!out || !assets || snapshot.phase == SABER_PHASE_IDLE)
    return false;

  /* Match oldsaber/saber-zero-variant:src/mmx_saber.c:1347-1349. The
   * attack record, rather than the cached animation-step convenience field,
   * owns the donor animation ID. */
  attack = MmxSaberAttackRecord(snapshot.kind, snapshot.index);
  if (!attack) return false;
  animation = MmxSaberAssetsAnimationById(assets, attack->visual_animation);
  if (!animation || !animation->total_ticks) return false;

  /* Match oldsaber/saber-zero-variant:src/mmx_saber.c:1351-1355: an attack
   * can outlive its sidecar animation, so its final sidecar tick is held. */
  tick = snapshot.tick < animation->total_ticks ? snapshot.tick :
      (uint16_t)(animation->total_ticks - 1);
  for (uint16_t i = 0; i < animation->step_count; ++i) {
    const MmxSaberStep *step = MmxSaberAssetsAnimationStep(
        assets, attack->visual_animation, i);
    if (!step) return false;
    if (tick < step->duration_ticks) {
      frame = MmxSaberAssetsFrameForStep(assets,
                                         attack->visual_animation, i);
      break;
    }
    tick = (uint16_t)(tick - step->duration_ticks);
  }
  if (!frame || !frame->body.pixels || !frame->body.width ||
      !frame->body.height) return false;

  out->body.pixels = frame->body.pixels;
  out->body.width = frame->body.width;
  out->body.height = frame->body.height;
  out->body.origin_x = frame->body.origin_x;
  out->body.origin_y = frame->body.origin_y;
  out->blade.pixels = frame->blade.pixels;
  out->blade.width = frame->blade.width;
  out->blade.height = frame->blade.height;
  out->blade.origin_x = frame->blade.origin_x;
  out->blade.origin_y = frame->blade.origin_y;
  out->blade_layer = frame->blade_layer;
  out->palette = MmxSaberAssetsPalette(assets);
  out->palette_count = MmxSaberAssetsPaletteCount(assets);
  /* The old compositor's mirror expression is the final left-facing bit:
   * oldsaber/saber-zero-variant:src/mmx_renderer.c:1595-1596. */
  out->facing_left = (snapshot.facing & 0x40) != 0 ^
      (animation->facing_xor != 0);
  out->active = out->palette && out->palette_count;
  if (!out->active) clear_overlay(out);
  return out->active;
}

bool MmxSaberRenderResolve(const MmxSaberAssets *assets,
                           MmxRenderPlayerOverlay *out) {
  return MmxSaberRenderResolveSnapshot(assets, MmxSaberAttackGetSnapshot(),
                                        out);
}

#include "mmx_zero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *poses;
static uint16_t colors[128];
static uint8_t saber_bounds[40];
static uint8_t hud_tiles[128];
static uint16_t hud_colors[16];
static uint8_t animation[MMX_ZERO_ANIMATION_BYTES];
static uint8_t muzzle[MMX_ZERO_MUZZLE_BYTES];
static MmxZeroState state;
_Static_assert(offsetof(MmxZeroState, anim_offset) == MMX_ZERO_LEGACY_STATE_SIZE,
               "Keep the v4 combat-state prefix readable");
static unsigned word(const uint8_t *p) { return p[0] | p[1] << 8; }
static void putword(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
MmxZeroState MmxZeroGetState(void) { return state; }
void MmxZeroResetState(void) { memset(&state, 0, sizeof(state)); }
void MmxZeroSetState(MmxZeroState s) {
  MmxZeroResetState();
  if (poses && s.combo <= 2 && s.slash <= 46 && s.charge <= 180 &&
      s.air <= 1 && (s.facing == 0 || s.facing == 64) && s.anim_valid <= 1 &&
      (!s.anim_valid || (s.anim_offset >= 272 && s.anim_offset + 3 <= sizeof(animation) &&
                        s.anim_timer && s.anim_pose < 117)) &&
      (!s.projectile || (s.projectile >= 0x1228 && s.projectile < 0x1428 && (s.projectile & 63) == 0x28))) state = s;
}

bool MmxZeroEnabled(void) { return poses != NULL; }
void MmxZeroDisable(void) {
  MmxZeroResetState();
  free(poses); poses = NULL;
}
bool MmxZeroLoad(const char *path) {
  FILE *f = path ? fopen(path, "rb") : NULL;
  if (!f) return false;
  uint8_t header[20], palette[256], bounds[40], hud[160], anim[MMX_ZERO_ANIMATION_BYTES];
  uint8_t emission[MMX_ZERO_MUZZLE_BYTES];
  size_t size = (size_t)MMX_ZERO_POSES * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
  uint8_t *data = NULL;
  bool ok = fread(header, 1, sizeof(header), f) == sizeof(header) &&
      !memcmp(header, "MMXZERO6", 8) && word(header + 8) == MMX_ZERO_WIDTH &&
      word(header + 10) == MMX_ZERO_HEIGHT && word(header + 12) == 64 &&
      word(header + 14) == 64 && word(header + 16) == 117 && word(header + 18) == 35 &&
      fread(palette, 1, sizeof(palette), f) == sizeof(palette) &&
      fread(bounds, 1, sizeof(bounds), f) == sizeof(bounds) &&
      fread(hud, 1, sizeof(hud), f) == sizeof(hud) &&
      fread(anim, 1, sizeof(anim), f) == sizeof(anim) &&
      fread(emission, 1, sizeof(emission), f) == sizeof(emission);
  if (ok) { data = malloc(size); ok = data && fread(data, 1, size, f) == size && fgetc(f) == EOF; }
  fclose(f);
  if (ok) for (unsigned i = 0; i < sizeof(bounds); i += 4)
    if (!bounds[i + 2] || bounds[i + 2] > 64 || !bounds[i + 3] || bounds[i + 3] > 64) { ok = false; break; }
  if (ok) for (size_t i = 0; i < size; ++i) if (data[i] >= 128) { ok = false; break; }
  if (ok) for (unsigned i = 0; i < 136; ++i) {
    unsigned p = word(anim + i * 2);
    if (p < 272 || p + 3 > sizeof(anim) || !anim[p] || anim[p + 2] >= 117) { ok = false; break; }
  }
  if (ok) for (unsigned i = 0; i < 117; ++i)
    if ((emission[i] & 1) || emission[i] > 74) { ok = false; break; }
  if (!ok) { free(data); return false; }
  MmxZeroDisable(); poses = data;
  memcpy(saber_bounds, bounds, sizeof(bounds));
  memcpy(animation, anim, sizeof(animation));
  memcpy(muzzle, emission, sizeof(muzzle));
  memcpy(hud_tiles, hud, sizeof(hud_tiles));
  for (unsigned i = 0; i < 16; ++i) hud_colors[i] = (uint16_t)(word(hud + 128 + 2 * i) & 0x7fff);
  for (unsigned i = 0; i < 128; ++i) colors[i] = (uint16_t)(word(palette + 2 * i) & 0x7fff);
  return true;
}
const uint16_t *MmxZeroColors(void) { return colors; }
const uint8_t *MmxZeroMenuPose(void) { return poses; }
int MmxZeroLifeColor(unsigned x, unsigned y) {
  if (!poses || x >= 16 || y >= 16) return -2;
  /* New front-facing 16px life art, shaded with Zero's original palette.
   * Vanilla X3 retains X's life icon; no original Zero life tile exists. */
  static const char half[16][9] = {
    "........", "...K....", "..KRK...", "..KRRKKK",
    ".KRRRWWG", ".KRrWWgG", "KRRrWKgg", "KRrWWKgg",
    "KWWrKKKK", "KWWKWEKs", ".KWKssSS", ".KWWKSSS",
    "..KWWSSS", "...KWKss", "....KKKK", "........"
  };
  unsigned color;
  switch (half[y][x < 8 ? x : 15 - x]) {
    case 'K': color = 31; break;
    case 'R': color = 23; break;
    case 'r': color = 24; break;
    case 'W': color = 20; break;
    case 'G': color = 28; break;
    case 'g': color = 17; break;
    case 's': color = 26; break;
    case 'S': color = 27; break;
    case 'E': color = 18; break;
    default: return -2;
  }
  return colors[color];
}
static void animation_record(unsigned offset) {
  if (offset < 272 || offset + 3 > sizeof(animation) || !animation[offset] || animation[offset + 2] >= 117) {
    state.anim_valid = 0; return;
  }
  state.anim_offset = (uint16_t)offset;
  state.anim_timer = animation[offset]; state.anim_flags = animation[offset + 1];
  state.anim_pose = animation[offset + 2]; state.anim_valid = 1;
}
static unsigned sequence_offset(unsigned sequence) {
  /* X1 group 0 -> original X3 group $4A. Aliases are kept: firing overlays
   * resume at interior records, not at the beginning of a movement cycle.
   * X1's two Hadouken actions use Zero's forward buster/recovery poses. */
  static const uint8_t map[] = {
    0x00,0x01,0x02,0x03,0x05,0x07,0x08,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,
    0x13,0x14,0x15,0x16,0x17,0x1a,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x50,0x51,0x52,0x53,0x55,0x57,0x58,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,
    0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x6a,0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,
    0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x84,0x30,
    0x34
  };
  return sequence < sizeof(map) ? word(animation + map[sequence] * 2) : 0;
}
void MmxZeroAnimationStart(unsigned object, unsigned sequence) {
  if (!poses || object != 0xba8) return;
  animation_record(sequence_offset(sequence));
}
void MmxZeroAnimationAdvance(unsigned object) {
  if (!poses || object != 0xba8 || !state.anim_valid) return;
  if (--state.anim_timer) return;
  unsigned next = state.anim_offset + 3;
  if (state.anim_flags & 128) {
    if (next + 2 > sizeof(animation)) { state.anim_valid = 0; return; }
    next = (unsigned)((int)next + (int16_t)word(animation + next));
  }
  animation_record(next);
}
unsigned MmxZeroMuzzle(const uint8_t r[0x20000], unsigned object,
                      unsigned native_index, unsigned axis, unsigned original) {
  if (!poses || !r || object < 0x1228 || object >= 0x1428 ||
      (object & 63) != 0x28 || axis > 1) return original;
  unsigned pose = state.anim_valid ? state.anim_pose : r[0xbbf] & 127;
  unsigned offset = pose < 117 ? muzzle[pose] : 0;
  if (!offset) {
    /* Delayed/formation projectiles can initialize after the firing overlay
     * ends. X1 retains its firing sequence index in the projectile's $3C. */
    unsigned record = sequence_offset(native_index / 2);
    if (!record) return original;
    pose = animation[record + 2]; offset = muzzle[pose];
  }
  if (!offset) return original;
  /* X3 stores signed Y then left-facing X. X1's native helpers mirror a
   * positive X and sign-extend Y. Keep their later spread/trajectory offsets. */
  return axis ? (unsigned)(uint8_t)(muzzle[120 + offset] - 8) :
                (unsigned)(uint8_t)(-(int8_t)muzzle[121 + offset]);
}
int MmxZeroHudColor(unsigned x, unsigned y) {
  /* Original X3 tile/palette data, independent of body visibility. */
  if (!MmxZeroEnabled() || x >= 16 || y >= 16) return -1;
  unsigned tile = (y / 8) * 2 + x / 8, shift = 7 - (x & 7);
  const uint8_t *p = hud_tiles + tile * 32 + (y & 7) * 2;
  unsigned pixel = ((p[0] >> shift) & 1) | (((p[1] >> shift) & 1) << 1) |
      (((p[16] >> shift) & 1) << 2) | (((p[17] >> shift) & 1) << 3);
  return pixel ? hud_colors[pixel] : -2;
}
static unsigned slash_pose(const MmxZeroState *s) {
  /* Vanilla X3 group $4B actions $00/$0E: duration, frame. */
  static const uint8_t duration[] = {3,3,1,2,3,3,6,16,3,3,3};
  static const uint8_t pose[] = {0,1,2,2,3,4,5,6,4,1,0};
  unsigned age = s->slash ? s->slash - 1 : 0;
  for (unsigned i = 0; i < sizeof(duration); ++i) {
    if (age < duration[i]) return pose[i] + (s->air ? 7 : 0);
    age -= duration[i];
  }
  return 0;
}
const uint8_t *MmxZeroPose(const uint8_t ram[0x20000], const MmxZeroState *s) {
  /* Visibility belongs to the submitted sprite list, not this RAM snapshot.
   * During invulnerability the next update can hide the player while OAM
   * still contains the preceding visible frame. The compositor owns blinking. */
  if (!poses || !ram) return NULL;
  /* Old saves without mirrored animation state use the shared pose vocabulary
   * until the next native animation start. New saves retain the exact phase. */
  unsigned pose = s && s->anim_valid ? s->anim_pose : ram[0xbbf] & 127;
  if (pose >= 117) pose = 0;
  if (s && s->slash) pose = 117 + slash_pose(s);
  return poses + (size_t)pose * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
}
const uint8_t *MmxZeroBlade(const MmxZeroState *s) {
  if (!poses || !s || s->slash < 7 || s->slash > 31) return NULL;
  static const uint8_t duration[] = {3,3,3,3,7,3,3};
  unsigned age = s->slash - 7, pose = 0;
  while (pose < 6 && age >= duration[pose]) age -= duration[pose++];
  return poses + (size_t)(138 + pose + (s->air ? 7 : 0)) * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
}
void MmxZeroSetCollisionRom(uint8_t *rom, size_t size) {
  if (!rom || size < 0x37fd8) return;
  /* X3 $86:B40E / B422, translated -8px vertically into X1's player origin.
   * This preserves Zero's full body dimensions and aligns both games' feet.
   * First four bytes are damage bounds; last six are terrain-probe geometry. */
  static const uint8_t normal[10] = {0, 0xfb, 6, 18, 0, 0, 0xfb, 6, 21, 8};
  static const uint8_t dash[10] = {0, 2, 6, 11, 0, 0, 0xfb, 6, 21, 8};
  static const uint8_t old_normal[10] = {0,255,6,14,0,0,255,7,17,8};
  static const uint8_t old_dash[10] = {0,5,6,8,0,0,255,9,17,8};
  /* Original X3 ground/air arc bounds in verified X1 $FF padding. */
  uint8_t empty[40]; memset(empty, 255, sizeof(empty));
  if ((!memcmp(rom + 0x32552, old_normal, 10) || !memcmp(rom + 0x32552, normal, 10)) &&
      (!memcmp(rom + 0x33b38, old_dash, 10) || !memcmp(rom + 0x33b38, dash, 10)) &&
      (!memcmp(rom + 0x37fb0, empty, 40) || !memcmp(rom + 0x37fb0, saber_bounds, 40))) {
    memcpy(rom + 0x32552, poses ? normal : old_normal, 10);
    memcpy(rom + 0x33b38, poses ? dash : old_dash, 10);
    memcpy(rom + 0x37fb0, poses ? saber_bounds : empty, 40);
  }
}
unsigned MmxZeroUpgradeBits(unsigned pc, unsigned original) {
  if (!poses) return original;
  switch (pc & 0x7fffff) {
    case 0x01971c: case 0x019793: case 0x0198fc: return original | 8;
    default: return original;
  }
}

static bool own_projectile(const uint8_t *r, unsigned d) {
  return d >= 0x1228 && d < 0x1428 && (d & 63) == 0x28 &&
      r[d] && word(r + d + 0x3e) == 0x5a53;
}
static void release_projectile(uint8_t *r) {
  unsigned d = state.projectile;
  if (own_projectile(r, d)) {
    memset(r + d, 0, 64);
    if (r[0xbdd]) --r[0xbdd];
  }
  state.projectile = 0;
}
void MmxZeroCancel(uint8_t ram[0x20000]) {
  if (ram) release_projectile(ram);
  /* Combat cancellation (hurt, weapon switch, menus) must not reset the
   * independent body animation. Full reset/load uses MmxZeroResetState. */
  memset(&state, 0, MMX_ZERO_LEGACY_STATE_SIZE);
}
static unsigned free_projectile(const uint8_t *r) {
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    if (!word(r + d)) return d;
  return 0;
}
static void clear_charge(uint8_t *r) {
  memset(r + 0xbff, 0, 5);
}
void MmxZeroPlayerTick(uint8_t r[0x20000]) {
  if (!poses || !r) return;
  /* This runs after X1 has latched mapped controls, before its player state.
   * Cutscenes, damage, death, ladders, ride armor and menus cancel the combo. */
  unsigned action = r[0xbaa];
  bool playable = r[0xd1] == 2 && r[0xd2] == 4 && r[0xba9] == 2 &&
      (r[0xbcf] & 127) && !r[0x1f0c] && !r[0xbdb] &&
      (action <= 8 || action == 0x10 || action == 0x12 || action == 0x14 || action == 0x20);
  if (!playable) { MmxZeroCancel(r); return; }
  if (state.cooldown) --state.cooldown;
  bool held = (r[0xbdf] & 64) != 0, pressed = (r[0xbe3] & 64) != 0;
  if (state.slash) {
    if (++state.slash > 46) { MmxZeroCancel(r); return; }
  } else if (state.combo && pressed && !state.cooldown) {
    if (state.combo == 1) {
      unsigned d = free_projectile(r);
      if (d && !r[0x1f0d]) {
        /* X1 rejects a second charged shot while $0C25 is nonzero.
         * Allocate the same native class directly; its initializer owns
         * position, velocity, graphics, lifetime and the charge-shot count. */
        clear_charge(r); memset(r + d, 0, 64);
        r[d] = 1; r[d + 10] = 3; ++r[0xbdd]; r[0x1f0d] = 4;
        state.combo = 2; state.cooldown = 18;
      }
    } else {
      unsigned d = free_projectile(r);
      if (d) {
        clear_charge(r); state.combo = 0; state.slash = 1;
        state.air = !(r[0xbd3] & 4); state.facing = r[0xc11] & 64;
        state.hit_slots = 0; state.projectile = (uint16_t)d;
        memset(r + d, 0, 64); r[d] = 1; r[d + 1] = 2;
        r[d + 10] = 3; /* X1's unarmored full buster damage class. */
        r[d + 0x11] = r[0xbb9]; putword(r + d + 0x3e, 0x5a53);
        ++r[0xbdd];
      }
    }
  } else if (!state.combo) {
    if (held) { if (state.charge < 180) ++state.charge; }
    else {
      if (state.charge == 180 && free_projectile(r) && !r[0x1f0d]) {
        clear_charge(r); r[0xc01] = 8; state.combo = 1; state.cooldown = 18;
      }
      state.charge = 0;
    }
  }
  if (state.combo) {
    /* Stored attacks must not start another native charge or pellet. */
    r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64;
  }
  if (state.slash) {
    r[0xbde] = 0; r[0xbdf] &= 128; r[0xbe2] = r[0xbe3] = 0;
    r[0xc11] = state.facing; clear_charge(r);
    r[0xbc2] = r[0xbc3] = 0; /* Stop horizontal velocity, preserve gravity. */
    if (!state.air && action != 0) { r[0xbaa] = 0; r[0xbab] = 0; }
    unsigned d = state.projectile;
    if (own_projectile(r, d)) {
      putword(r + d + 5, word(r + 0xbad));
      putword(r + d + 8, word(r + 0xbb0));
      unsigned age = state.slash >= 7 ? state.slash - 7 : 99;
      unsigned phase = age < 12 ? age / 3 : 4;
      putword(r + d + 0x20, age < 19 ? 0xffb0 + (state.air ? 20 : 0) + phase * 4 : 0);
      r[d + 0x30] = 0;
    }
  }
}
unsigned MmxZeroWeaponTick(uint8_t r[0x20000], unsigned d, unsigned active) {
  if (!own_projectile(r, d)) return active;
  if (!poses || d != state.projectile || !state.slash) {
    memset(r + d, 0, 64); if (r[0xbdd]) --r[0xbdd];
  }
  return 0; /* Our transient melee object has no native projectile update. */
}
unsigned MmxZeroDamage(uint8_t r[0x20000], unsigned enemy, unsigned projectile, unsigned original) {
  if (!poses || !own_projectile(r, projectile) || projectile != state.projectile ||
      enemy < 0xe68 || enemy >= 0x1228 || (enemy & 63) != 0x28 || !original || (original & 128)) return original;
  unsigned bit = 1u << ((enemy - 0xe68) / 64);
  if (state.hit_slots & bit) return 0;
  state.hit_slots |= (uint16_t)bit;
  return 16;
}
unsigned MmxZeroHitbox(const uint8_t r[0x20000], unsigned enemy, unsigned projectile, unsigned original) {
  if (poses && own_projectile(r, projectile) && projectile == state.projectile &&
      enemy >= 0xe68 && enemy < 0x1228 && (enemy & 63) == 0x28 &&
      (state.hit_slots & (1u << ((enemy - 0xe68) / 64)))) return 0;
  return original;
}

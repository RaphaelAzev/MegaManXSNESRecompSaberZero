#include "mmx_zero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *poses;
static uint16_t colors[128];
static uint8_t saber_bounds[40];
static MmxZeroState state;
static unsigned word(const uint8_t *p) { return p[0] | p[1] << 8; }
static void putword(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
MmxZeroState MmxZeroGetState(void) { return state; }
void MmxZeroResetState(void) { memset(&state, 0, sizeof(state)); }
void MmxZeroSetState(MmxZeroState s) {
  MmxZeroResetState();
  if (poses && s.combo <= 2 && s.slash <= 46 && s.charge <= 180 &&
      s.air <= 1 && (s.facing == 0 || s.facing == 64) &&
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
  uint8_t header[20], palette[256], bounds[40];
  size_t size = (size_t)MMX_ZERO_POSES * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
  uint8_t *data = NULL;
  bool ok = fread(header, 1, sizeof(header), f) == sizeof(header) &&
      !memcmp(header, "MMXZERO3", 8) && word(header + 8) == MMX_ZERO_WIDTH &&
      word(header + 10) == MMX_ZERO_HEIGHT && word(header + 12) == 64 &&
      word(header + 14) == 64 && word(header + 16) == 117 && word(header + 18) == 35 &&
      fread(palette, 1, sizeof(palette), f) == sizeof(palette) &&
      fread(bounds, 1, sizeof(bounds), f) == sizeof(bounds);
  if (ok) { data = malloc(size); ok = data && fread(data, 1, size, f) == size && fgetc(f) == EOF; }
  fclose(f);
  if (ok) for (unsigned i = 0; i < sizeof(bounds); i += 4)
    if (!bounds[i + 2] || bounds[i + 2] > 64 || !bounds[i + 3] || bounds[i + 3] > 64) { ok = false; break; }
  if (ok) for (size_t i = 0; i < size; ++i) if (data[i] >= 128) { ok = false; break; }
  if (!ok) { free(data); return false; }
  MmxZeroDisable(); poses = data;
  memcpy(saber_bounds, bounds, sizeof(bounds));
  for (unsigned i = 0; i < 128; ++i) colors[i] = (uint16_t)(word(palette + 2 * i) & 0x7fff);
  return true;
}
const uint16_t *MmxZeroColors(void) { return colors; }
int MmxZeroHudColor(unsigned x, unsigned y, const uint16_t palette[16]) {
  /* A red Z in the existing X1 badge. Keep the surrounding bar/frame live;
   * character identity is resolved here, independently of player visibility. */
  if (!MmxZeroEnabled() || x < 2 || x > 13 || y < 3 || y > 11) return -1;
  static const char badge[9][13] = {
    ".##########.",
    ".#rrrrrrrr#.",
    "..#####rr#..",
    ".....#rr#...",
    "....#rr#....",
    "...#rr#.....",
    "..#rr#####..",
    ".#rrrrrrrr#.",
    ".##########.",
  };
  switch (badge[y - 3][x - 2]) {
    case '#': return palette[15];
    case 'r': return 31 | (5 << 5) | (4 << 10);
    default: return palette[1];
  }
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
  if (!poses || !ram || !ram[0xbb6]) return NULL;
  /* The shared early-X animation vocabulary includes idle, run, jump,
   * dash and firing poses. X1-specific states remain a validation item. */
  unsigned pose = ram[0xbbf] & 127;
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
  MmxZeroResetState();
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

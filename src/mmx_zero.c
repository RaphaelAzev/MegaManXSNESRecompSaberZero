#include "mmx_zero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *poses;
static uint8_t *charge_poses;
static uint16_t charge_colors[3][16];
static uint16_t colors[128];
static uint8_t saber_bounds[40];
static uint8_t hud_tiles[128];
static uint16_t hud_colors[16];
static uint8_t animation[MMX_ZERO_ANIMATION_BYTES];
static uint8_t muzzle[MMX_ZERO_MUZZLE_BYTES];
static MmxZeroState state;
_Static_assert(offsetof(MmxZeroState, anim_offset) == MMX_ZERO_LEGACY_STATE_SIZE,
               "Keep the v4 combat-state prefix readable");
_Static_assert(offsetof(MmxZeroState, burst_offset) == MMX_ZERO_ANIMATION_STATE_SIZE,
               "Keep the v5 animation-state prefix readable");
_Static_assert(offsetof(MmxZeroState, active_x) == MMX_ZERO_COMBAT_STATE_SIZE,
               "Keep the v6 combat-state prefix readable");
_Static_assert(offsetof(MmxZeroState, hp) == MMX_ZERO_SWAP_STATE_SIZE,
               "Keep the v7 character/swap-state prefix readable");
static unsigned word(const uint8_t *p) { return p[0] | p[1] << 8; }
static void putword(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
MmxZeroState MmxZeroGetState(void) { return state; }
void MmxZeroResetState(void) { memset(&state, 0, sizeof(state)); }
void MmxZeroSetState(MmxZeroState s) {
  MmxZeroResetState();
  if (poses && s.combo <= 2 && s.slash <= 44 && s.charge <= 201 &&
      s.active_x <= 1 && s.swap_phase <= 6 && s.swap_tick <= 30 && s.swap_y <= 0 && s.swap_y >= -320 &&
      s.hp_valid <= 1 && s.hp_max <= 32 && s.hp[0] <= 32 && s.hp[1] <= 32 &&
      (!s.hp_valid || (s.hp_max >= 16 && s.hp[0] <= s.hp_max && s.hp[1] <= s.hp_max)) &&
      s.air <= 1 && (s.facing == 0 || s.facing == 64) && s.anim_valid <= 1 &&
      s.burst <= 2 && s.burst_end <= 1 && s.saber_ready <= 1 && s.burst_transition <= 1 && s.burst_fired <= 1 &&
      (!s.burst || (s.burst_offset >= 272 && s.burst_offset + 3 <= sizeof(animation) &&
                    s.burst_timer && animation[s.burst_offset + 2] < 117)) &&
      (!s.anim_valid || (s.anim_offset >= 272 && s.anim_offset + 3 <= sizeof(animation) &&
                        s.anim_timer && s.anim_pose < 117)) &&
      (!s.projectile || (s.projectile >= 0x1228 && s.projectile < 0x1428 && (s.projectile & 63) == 0x28))) state = s;
}
unsigned MmxZeroChargeTier(const MmxZeroState *s) {
  return !s || s->charge < 21 ? 0 : s->charge < 81 ? 4 :
         s->charge < 141 ? 6 : s->charge < 201 ? 8 : 10;
}

bool MmxZeroEnabled(void) { return poses != NULL; }
bool MmxZeroActive(void) { return poses && !state.active_x; }
bool MmxZeroSwapping(void) { return poses && state.swap_phase; }
void MmxZeroHealthSync(const uint8_t r[0x20000]) {
  if (!poses || !r || r[0xd1] != 2 || r[0x1f9a] < 16 || r[0x1f9a] > 32) return;
  unsigned max = r[0x1f9a], hp = r[0xbcf] & 127;
  if (hp > max) hp = max;
  /* Older saves have one pool. Seed both from it exactly once. All normal
   * damage, pickups and subtanks continue to affect only native active HP. */
  if (!state.hp_valid) state.hp[0] = state.hp[1] = (uint8_t)hp;
  state.hp_valid = 1; state.hp_max = (uint8_t)max;
  state.hp[state.active_x] = (uint8_t)hp;
  if (state.hp[state.active_x ^ 1] > max) state.hp[state.active_x ^ 1] = (uint8_t)max;
}
void MmxZeroHealthRespawn(const uint8_t r[0x20000]) {
  if (!poses || !r || r[0x1f9a] < 16 || r[0x1f9a] > 32) return;
  /* Called at the original stage/checkpoint HP initialization, not whenever
   * a pool happens to reach zero. Death and life loss remain native. */
  state.hp_valid = 1; state.hp_max = r[0x1f9a];
  state.hp[0] = state.hp[1] = state.hp_max;
}
void MmxZeroDisable(void) {
  MmxZeroResetState();
  free(poses); poses = NULL;
  free(charge_poses); charge_poses = NULL;
}
bool MmxZeroLoad(const char *path) {
  FILE *f = path ? fopen(path, "rb") : NULL;
  if (!f) return false;
  uint8_t header[20], palette[256], bounds[40], hud[160], anim[MMX_ZERO_ANIMATION_BYTES];
  uint8_t emission[MMX_ZERO_MUZZLE_BYTES];
  size_t size = (size_t)MMX_ZERO_POSES * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
  uint8_t *data = NULL, *particles = NULL, flash[96];
  bool modern = false;
  bool ok = fread(header, 1, sizeof(header), f) == sizeof(header) &&
      ((modern = !memcmp(header, "MMXZERO7", 8)) || !memcmp(header, "MMXZERO6", 8)) && word(header + 8) == MMX_ZERO_WIDTH &&
      word(header + 10) == MMX_ZERO_HEIGHT && word(header + 12) == 64 &&
      word(header + 14) == 64 && word(header + 16) == 117 && word(header + 18) == 35 &&
      fread(palette, 1, sizeof(palette), f) == sizeof(palette) &&
      fread(bounds, 1, sizeof(bounds), f) == sizeof(bounds) &&
      fread(hud, 1, sizeof(hud), f) == sizeof(hud) &&
      fread(anim, 1, sizeof(anim), f) == sizeof(anim) &&
      fread(emission, 1, sizeof(emission), f) == sizeof(emission);
  if (ok) { data = malloc(size); ok = data && fread(data, 1, size, f) == size; }
  size_t charge_size = (size_t)MMX_ZERO_CHARGE_POSES * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
  if (ok && modern) {
    particles = malloc(charge_size);
    ok = particles && fread(flash, 1, sizeof(flash), f) == sizeof(flash) &&
        fread(particles, 1, charge_size, f) == charge_size;
    if (ok) for (size_t i=0;i<charge_size;++i) if (particles[i]>=16) { ok=false; break; }
  }
  if (ok) ok = fgetc(f) == EOF;
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
  if (!ok) { free(data); free(particles); return false; }
  MmxZeroDisable(); poses = data; charge_poses = particles;
  if (modern) for (unsigned i=0;i<48;++i) charge_colors[i/16][i%16]=(uint16_t)(word(flash+i*2)&0x7fff);
  memcpy(saber_bounds, bounds, sizeof(bounds));
  memcpy(animation, anim, sizeof(animation));
  memcpy(muzzle, emission, sizeof(muzzle));
  memcpy(hud_tiles, hud, sizeof(hud_tiles));
  for (unsigned i = 0; i < 16; ++i) hud_colors[i] = (uint16_t)(word(hud + 128 + 2 * i) & 0x7fff);
  for (unsigned i = 0; i < 128; ++i) colors[i] = (uint16_t)(word(palette + 2 * i) & 0x7fff);
  return true;
}
const uint16_t *MmxZeroColors(void) { return colors; }
bool MmxZeroHasChargeArt(void) { return charge_poses != NULL; }
const uint16_t *MmxZeroBodyColors(const MmxZeroState *s) {
  if (!charge_poses || !s || s->active_x || s->swap_phase || s->slash ||
      (s->charge < 25 && !(s->combo && s->saber_ready)) || (s->charge_phase & 2)) return colors + 16;
  return charge_colors[s->saber_ready || s->charge >= 201 ? 2 : s->charge >= 141 ? 1 : 0];
}
const uint8_t *MmxZeroChargePose(const MmxZeroState *s) {
  if (!charge_poses || !s || s->active_x || s->swap_phase || s->slash || s->burst || s->combo || s->charge < 21) return NULL;
  unsigned group = s->charge < 81 ? 0 : s->charge < 141 ? 1 : 2;
  return charge_poses + (size_t)(group * 22 + s->charge_phase % 22) * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT;
}
bool MmxZeroNativeChargeObject(unsigned object, unsigned kind) {
  /* $82:82ED allocates any of twelve small actors, not just $0C98. */
  return object >= 0xc98 && object < 0xe18 && ((object - 0xc98) % 32) == 0 && kind == 1;
}
const uint8_t *MmxZeroMenuPose(void) { return poses; }
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
  if (!MmxZeroActive() || !r || object < 0x1228 || object >= 0x1428 ||
      (object & 63) != 0x28 || axis > 1) return original;
  unsigned pose = state.burst ? animation[state.burst_offset + 2] :
      state.anim_valid ? state.anim_pose : r[0xbbf] & 127;
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
unsigned MmxZeroWeaponOrigin(const uint8_t r[0x20000], unsigned object,
                             unsigned axis, unsigned original) {
  if (!MmxZeroActive() || !r || object < 0x1228 || object >= 0x1428 ||
      (object & 63) != 0x28) return original;
  /* These retail routines bypass the shared muzzle table. Apply the offset
   * where they assign the real object origin, so art and collision agree.
   * Tornado remains centered vertically; Boomerang keeps its return arc. */
  unsigned kind = r[object + 10];
  int delta = axis == 0 && kind == 0x0b ? (r[object + 17] & 64 ? 6 : -6) :
              axis == 1 && kind == 0x0d ? -8 :
              axis == 1 && kind == 0x12 ? -6 : 0;
  return (original + delta) & 0xffff;
}
static unsigned slash_pose(const MmxZeroState *s) {
  /* Vanilla X3 group $4B actions $00/$0E: duration, frame. */
  static const uint8_t duration[] = {3,3,1,2,3,3,6,16,3,3,3};
  static const uint8_t pose[] = {0,1,2,2,3,4,5,6,4,1,0};
  unsigned age = s->slash;
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
  if (!poses || !ram || (s && s->active_x)) return NULL;
  /* Old saves without mirrored animation state use the shared pose vocabulary
   * until the next native animation start. New saves retain the exact phase. */
  unsigned pose = s && s->anim_valid ? s->anim_pose : ram[0xbbf] & 127;
  if (s && s->burst && s->burst_offset + 3 <= sizeof(animation)) pose = animation[s->burst_offset + 2];
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
    memcpy(rom + 0x32552, MmxZeroActive() ? normal : old_normal, 10);
    memcpy(rom + 0x33b38, MmxZeroActive() ? dash : old_dash, 10);
    memcpy(rom + 0x37fb0, MmxZeroActive() ? saber_bounds : empty, 40);
  }
}
unsigned MmxZeroUpgradeBits(unsigned pc, unsigned original) {
  if (!MmxZeroActive()) return original;
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
static void clear_charge(uint8_t *r);
void MmxZeroCancel(uint8_t ram[0x20000]) {
  if (ram && (state.charge || state.combo || state.burst || state.slash)) clear_charge(ram);
  if (ram) release_projectile(ram);
  /* Combat cancellation (hurt, weapon switch, menus) must not reset the
   * independent body animation. Full reset/load uses MmxZeroResetState. */
  memset(&state, 0, MMX_ZERO_LEGACY_STATE_SIZE);
  memset((uint8_t *)&state + MMX_ZERO_ANIMATION_STATE_SIZE, 0,
         MMX_ZERO_COMBAT_STATE_SIZE - MMX_ZERO_ANIMATION_STATE_SIZE);
}
static unsigned free_projectile(const uint8_t *r) {
  for (unsigned d = 0x1228; d < 0x1428; d += 64)
    if (!word(r + d)) return d;
  return 0;
}
static void sound(uint8_t *r, unsigned command) {
  /* Same guest-owned ring as $80:88CD. The ordinary SPC driver consumes it. */
  unsigned index = r[0xba3] & 0x1e;
  r[0xb72 + index] = (uint8_t)command; r[0xb73 + index] = 0;
  r[0xba3] = (uint8_t)((index + 2) & 0x1e);
}
static void clear_charge(uint8_t *r) {
  /* $81:9890 stops the looping charge voice before clearing charge state.
   * Bypassing that release path without $17 leaves the SPC voice playing. */
  if (r[0xc2f] & 64) { sound(r,0x17); r[0xc2f] &= (uint8_t)~64; }
  memset(r + 0xbff, 0, 5);
  r[0xc2a] = 0;
  for (unsigned d=0xc98;d<0xe18;d+=32)
    if (MmxZeroNativeChargeObject(d,r[d+10])) memset(r+d,0,32);
}
unsigned MmxZeroSwapPose(const MmxZeroState *s) {
  if (!s || !s->swap_phase || s->swap_phase == 3) return 255;
  if (s->swap_phase == 2 || s->swap_phase == 4) return 0x3c;
  if (s->swap_phase == 6) return 0;
  /* Retail X1 $49/$48 and X3 $7F/$7E: five one-frame body morphs
   * and a two-frame energy ball. The traveling column is pose $3C. */
  if (s->swap_phase == 1) return s->swap_tick < 5 ? 0x3e + s->swap_tick : 0x3d;
  return s->swap_tick < 2 ? 0x3d : 0x44 - s->swap_tick;
}
const uint8_t *MmxZeroTeleportPose(unsigned pose) {
  return poses && (pose == 0 || (pose >= 0x3c && pose <= 0x42)) ?
      poses + (size_t)pose * MMX_ZERO_WIDTH * MMX_ZERO_HEIGHT : NULL;
}
bool MmxZeroSwapTick(uint8_t r[0x20000]) {
  if (!poses || !r) return false;
  MmxZeroHealthSync(r);
  if (!state.swap_phase) {
    /* Native NMI edge latch: Select=$2000. A grounded idle player owns
     * control; transitions, menus, hurt, ladders and ride armor do not. */
    if (!(r[0xac] & 0x20) || r[0xd1] != 2 || r[0xd2] != 4 ||
        r[0xba9] != 2 || r[0xbaa] != 0 || r[0xbab] != 2 ||
        !(r[0xbd3] & 4) || !r[0xbb6] || !(r[0xbcf] & 127) ||
        r[0x1f0c] || r[0x1f10] || r[0x1f23] || r[0x1f48] ||
        word(r + 0xbc2) || state.burst || state.slash) return false;
    MmxZeroCancel(r); clear_charge(r);
    state.swap_phase = 1; state.swap_tick = state.swap_fraction = 0; state.swap_y = 0;
    sound(r,0x0f); /* X1's original teleport-out sound ($81:8D05). */
  } else switch (state.swap_phase) {
    case 1:
      if (++state.swap_tick == 7) { state.swap_phase = 2; state.swap_tick = 0; }
      break;
    case 2: {
      /* Original X1 outgoing speed $0AA6, incoming speed $0800. */
      int fixed = state.swap_y * 256 + state.swap_fraction - 0x0aa6;
      state.swap_y = (int16_t)((fixed - 255) / 256);
      state.swap_fraction = (uint8_t)(fixed - state.swap_y * 256);
      int screen_y = (int16_t)(word(r + 0xbb0) - word(r + 0x1e50));
      if (screen_y + state.swap_y < -40) {
        state.active_x ^= 1; state.anim_valid = 0;
        if (state.hp_valid) r[0xbcf] = state.hp[state.active_x] | 128;
        state.swap_phase = 3; state.swap_tick = 0;
      }
      break;
    }
    case 3:
      if (++state.swap_tick == 30) { /* Original X3 exchange delay $1E. */
        state.swap_phase = 4; state.swap_tick = state.swap_fraction = 0;
        sound(r,0x0e); /* Native X1 arrival ($81:8A18). */
      }
      break;
    case 4:
      state.swap_y += 8;
      if (state.swap_y >= 0) { state.swap_y = 0; state.swap_phase = 5; }
      break;
    case 5:
      if (++state.swap_tick == 7) { state.swap_phase = 6; state.swap_tick = 0; }
      break;
    case 6:
      state.swap_phase = state.swap_tick = state.swap_fraction = 0; state.swap_y = 0;
      /* Resume idle through its native initializer, with no buffered presses. */
      r[0xbab] = 0; r[0xab] = r[0xac] = r[0xbe2] = r[0xbe3] = 0;
      return false;
  }
  /* The scheduler normally acknowledges NMI. Keep DMA/input/SPC alive while
   * every task (including stage scripts and enemy animation) is suspended. */
  r[0xb9d] = r[0xba0] = 0;
  return true;
}
static unsigned burst_sequence(void) {
  return state.burst == 1 ? (state.air ? 0x43 : 0x30) : (state.air ? 0x49 : 0x36);
}
static void burst_record(unsigned sequence) {
  state.burst_offset = (uint16_t)word(animation + sequence * 2);
  state.burst_timer = animation[state.burst_offset];
}
static void start_burst(uint8_t *r, unsigned which) {
  state.burst = (uint8_t)which; state.burst_end = 0;
  state.burst_transition = state.burst_fired = 0;
  state.air = !(r[0xbd3] & 4); state.facing = r[0xc11] & 64;
  burst_record(burst_sequence());
  if (which == 2 && state.air) ++state.burst_timer; /* X3's separate setup frame. */
  clear_charge(r);
}
static void emit_burst(uint8_t *r) {
  unsigned d = free_projectile(r);
  if (!d) return;
  memset(r + d,0,64); r[d] = 1; r[d + 10] = 3;
  ++r[0xbdd]; r[0x1f0d] = 4;
  sound(r,2); /* Native full-buster release sound ($81:A015), once per shot. */
  state.shot_mask |= (uint8_t)(1u << ((d - 0x1228) / 64));
}
static void advance_burst(uint8_t *r) {
  unsigned flags = animation[state.burst_offset + 1];
  if (state.burst_transition) {
    burst_record(burst_sequence() + (flags & 7));
    state.burst_transition = 0;
    flags = animation[state.burst_offset + 1];
  }
  if (flags & 128) { state.burst_end = 1; return; }
  if ((flags & 64) && !state.burst_fired) {
    emit_burst(r); state.burst_fired = 1;
  }
  bool air = !(r[0xbd3] & 4);
  if (air != !!state.air) {
    /* X3 changes action first, then resumes the equivalent numbered phase
     * on the following update. The transition itself retains the old pose. */
    state.air = air;
    state.burst_transition = 1;
    return;
  }
  if (!--state.burst_timer) {
    unsigned next = state.burst_offset + 3;
    if (next + 3 > sizeof(animation) || !animation[next] || animation[next + 2] >= 117) {
      MmxZeroCancel(r); return;
    }
    state.burst_offset = (uint16_t)next;
    state.burst_timer = animation[state.burst_offset];
  }
}
static void track_burst_shots(const uint8_t *r) {
  unsigned alive = 0;
  for (unsigned i = 0; i < 8; ++i) {
    unsigned d = 0x1228 + i * 64;
    if ((state.shot_mask & (1u << i)) && r[d] && r[d + 10] == 3 &&
        !own_projectile(r,d)) alive |= 1u << i;
  }
  /* X3 requires both the charged projectile and its effects to retire. The
   * retained X1 beam executes its disappearance animation in the SAME slot
   * ($81:A3CE..A40D); C25 is decremented only afterwards. Wait for that real
   * lifetime, including impact/offscreen recovery, with no guessed delay. */
  state.shot_mask = (uint8_t)alive;
}
static bool burst_holds_air(void) {
  return state.burst && state.air && (state.burst_end ||
      (state.burst == 2 && state.burst_offset == word(animation + 0x49 * 2) && state.burst_timer == 2));
}
void MmxZeroPlayerTick(uint8_t r[0x20000]) {
  if (!MmxZeroActive() || !r) return;
  unsigned action = r[0xbaa];
  bool playable = r[0xd1] == 2 && r[0xd2] == 4 && r[0xba9] == 2 &&
      (r[0xbcf] & 127) && !r[0x1f0c] && !r[0xbdb] &&
      (action <= 8 || action == 0x10 || action == 0x12 || action == 0x14 || action == 0x20);
  if (!playable) { MmxZeroCancel(r); return; }
  bool held = (r[0xbdf] & 64) != 0, pressed = (r[0xbe3] & 64) != 0;
  if (state.charge >= 21 || (state.combo && state.saber_ready))
    state.charge_phase = (uint8_t)((state.charge_phase + 1) % 88);
  else state.charge_phase = 0;
  track_burst_shots(r);
  if (state.burst_end) {
    state.burst = state.burst_end = 0;
    if (r[0xbd3] & 4) r[0xbab] = 0; /* Restart the ordinary idle action. */
    else if (r[0xbaa] == 8) MmxZeroAnimationStart(0xba8,5 + r[0xc17]);
  }
  if (state.slash) {
    if (++state.slash > 44) {
      MmxZeroCancel(r);
      if (r[0xbd3] & 4) r[0xbab] = 0;
      return;
    }
  } else if (!state.burst && state.combo && pressed) {
    if (state.combo == 1) {
      if (free_projectile(r) && !r[0x1f0d]) {
        state.combo = state.saber_ready ? 2 : 0;
        start_burst(r,2);
      }
    } else if (state.saber_ready && !state.shot_mask && !r[0xc25]) {
      unsigned d = free_projectile(r);
      if (d) {
        clear_charge(r); state.combo = state.saber_ready = 0; state.slash = 1;
        state.air = !(r[0xbd3] & 4); state.facing = r[0xc11] & 64;
        state.hit_slots = 0; state.projectile = (uint16_t)d;
        memset(r + d, 0, 64); r[d] = 1; r[d + 1] = 2;
        r[d + 10] = 3; r[d + 0x11] = r[0xbb9];
        putword(r + d + 0x3e, 0x5a53); ++r[0xbdd];
      }
    }
  } else if (!state.burst && !state.combo) {
    if (held) {
      if (state.charge < 201) ++state.charge;
      /* Feed X1's ordinary charging visuals at Zero's measured thresholds.
       * Its release command is selected explicitly below; X1 special weapons
       * never enter this path and retain their own charge/upgrade rules. */
      if (state.charge == 21) r[0xbff] = 0x97;
      else if (state.charge > 21 && state.charge < 81) r[0xbff] = 0x90;
      else if (state.charge == 81) r[0xbff] = 0x51;
      else if (state.charge > 81) r[0xbff] = 0x40;
      if (state.charge >= 141) { r[0xc03] = 1; r[0xc2a] = 4; }
    } else {
      unsigned tier = MmxZeroChargeTier(&state);
      if (tier >= 8 && free_projectile(r) && !r[0x1f0d]) {
        state.combo = 1; state.saber_ready = tier == 10;
        start_burst(r,1);
      } else if (tier >= 4) {
        /* X1 $81:94BF indexes $86:BA76 by command/2: command 2
         * is class 1 (green half-shot); command 6 is class 2. */
        clear_charge(r); r[0xc01] = tier >= 6 ? 8 : 2;
      }
      state.charge = 0;
    }
  }
  if (state.burst) {
    state.held_vy = (uint16_t)word(r + 0xbc4);
    advance_burst(r);
    clear_charge(r);
    /* The ground firing sequence plants Zero's feet; jumping still transfers
     * to its airborne counterpart. Native air control/gravity/collision run. */
    if (!state.air) {
      r[0xbde] = r[0xbe2] = 0;
      if (action != 0) { r[0xbaa] = 0; r[0xbab] = 0; }
    } else {
      /* X3's firing action keeps integrating through the apex, instead of
       * entering X1's ordinary fall initializer (which resets VY to zero). */
      r[0xbdf] |= 128;
      if (r[0xbaa] == 6 && (int16_t)word(r + 0xbc4) < 0) {
        r[0xbaa] = 8; r[0xbab] = 2;
        MmxZeroAnimationStart(0xba8,4 + r[0xc17]);
      }
      /* Second-shot initialization and the terminal action-return frame do
       * not integrate motion in X3.
       * Keep native terrain probes at the unchanged position, then restore
       * the velocity after the native update for next frame's continuation. */
      if (burst_holds_air()) {
        state.held_vy = (uint16_t)word(r + 0xbc4); state.held_gravity = r[0xbc6];
        putword(r + 0xbc4,0); r[0xbc6] = 0; r[0xbde] = r[0xbe2] = 0;
      }
    }
    r[0xc11] = state.facing;
  }
  if (state.combo || state.burst) {
    r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64;
  }
  if (state.slash) {
    r[0xbde] = 0; r[0xbdf] &= 128; r[0xbe2] = r[0xbe3] = 0;
    r[0xc11] = state.facing; clear_charge(r);
    r[0xbc2] = r[0xbc3] = 0;
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
void MmxZeroPlayerEnd(uint8_t r[0x20000]) {
  if (poses && r && state.burst) {
    if (!state.air || burst_holds_air()) putword(r + 0xbc4,state.held_vy);
    if (burst_holds_air()) r[0xbc6] = state.held_gravity;
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

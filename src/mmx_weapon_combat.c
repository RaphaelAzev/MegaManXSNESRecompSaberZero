#include "mmx_weapon_combat.h"
#include "mmx_weapons.h"
#include "mmx_zero.h"
#include <string.h>

static MmxWeaponCombatState combat;
static uint8_t *collision_rom;
static size_t collision_rom_size;
static bool collision_patch;
static uint8_t previous_boxes[32];
_Static_assert(sizeof(MmxWeaponShot) == 40 && sizeof(MmxWeaponCombatState) == 328, "Weapon combat save ABI");
static unsigned word(const uint8_t *p) { return p[0] | (p[1] << 8); }
static void putword(uint8_t *p, unsigned value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
static bool slot_valid(unsigned d) { return d >= 0x1228 && d < 0x1428 && (d & 63) == 0x28; }
static bool owned(const uint8_t *r, unsigned d) { return slot_valid(d) && r[d] && word(r + d + 0x3e) == 0x5758; }
static unsigned slot_index(unsigned d) { return (d - 0x1228) / 64; }
static unsigned weapon_group(unsigned page, unsigned weapon) {
  return page == 2 ? (weapon == 1 ? 5 : weapon == 4 ? 12 : 0) : 0;
}
static void sound(uint8_t *r, unsigned command) {
  unsigned i = r[0xba3] & 30; r[0xb72 + i] = (uint8_t)command; r[0xb73 + i] = 0; r[0xba3] = (uint8_t)((i + 2) & 30);
}
static void stop_charge(uint8_t *r) {
  if (r[0xc2f] & 64) { sound(r, 0x17); r[0xc2f] &= (uint8_t)~64; }
  memset(r + 0xbff, 0, 5);
}
MmxWeaponCombatState MmxWeaponsGetCombatState(void) { return combat; }
bool MmxWeaponsValidCombatState(const MmxWeaponCombatState *s) {
  if (!s || s->valid > 1 || s->held > 1 || s->pressed > 1 || s->reserved) return false;
  for (unsigned i = 0; i < 8; ++i) {
    const MmxWeaponShot *p = s->shots + i;
    if (p->active > 1 || p->reserved || p->charged > 1 || p->pose >= 128 || p->animation > 8192 ||
        p->x < -0x1000000 || p->x > 0x1000000 || p->y < -0x1000000 || p->y > 0x1000000 ||
        (p->active && (!weapon_group(p->page,p->weapon) || p->group != weapon_group(p->page,p->weapon)))) return false;
    if (p->active && p->page == 2 && p->weapon == 1 &&
        (p->muzzle_pose > 2 || p->radius > 10 ||
         (p->variant > 2 && p->variant != 128 && p->variant != 129 && p->variant != 130))) return false;
  }
  return true;
}
void MmxWeaponsSetCombatState(MmxWeaponCombatState s) {
  memset(&combat, 0, sizeof(combat));
  if (MmxWeaponsValidCombatState(&s)) combat = s;
}
bool MmxWeaponsCombatActive(void) {
  MmxWeaponsState s = MmxWeaponsGetState();
  return MmxWeaponsEnabled() && weapon_group(s.page,s.weapon) != 0;
}
static void retire(uint8_t *r, unsigned d) {
  if (owned(r,d)) { memset(r+d,0,64); if (r[0xbdd]) --r[0xbdd]; }
  memset(combat.shots + slot_index(d),0,sizeof(MmxWeaponShot));
}
void MmxWeaponsCancelShots(uint8_t r[0x20000]) {
  for (unsigned d = 0x1228; d < 0x1428; d += 64) if (r) retire(r,d);
  memset(&combat,0,sizeof(combat));
}
static void animation_record(MmxWeaponShot *s, unsigned offset) {
  unsigned size = 0;
  const uint8_t *data = MmxWeaponsAnimation(s->page,s->weapon,s->group,&size);
  if (!data || offset + 3 > size || !data[offset] || data[offset+2] >= 128) { s->active = 0; return; }
  s->animation = (uint16_t)offset; s->timer = data[offset]; s->flags = data[offset+1]; s->pose = data[offset+2];
}
static void animation_start(MmxWeaponShot *s, unsigned sequence) {
  unsigned size = 0;
  const uint8_t *data = MmxWeaponsAnimation(s->page,s->weapon,s->group,&size);
  if (!data || size < 2 || sequence * 2 + 2 > word(data)) { s->active = 0; return; }
  animation_record(s,word(data + sequence * 2));
}
static void animation_step(MmxWeaponShot *s) {
  if (!s->timer || --s->timer) return;
  unsigned next = s->animation + 3, size = 0;
  const uint8_t *data = MmxWeaponsAnimation(s->page,s->weapon,s->group,&size);
  if (s->flags & 128) {
    if (!data || next + 2 > size) { s->active = 0; return; }
    next = (unsigned)((int)next + (int16_t)word(data + next));
  }
  animation_record(s,next);
}
void MmxWeaponsPlayerTick(uint8_t r[0x20000]) {
  if (!MmxWeaponsEnabled()) return;
  if (!combat.valid || combat.stage != r[0x1f7a] || r[0xd1] != 2 || !(r[0xbcf] & 127)) {
    MmxWeaponsCancelShots(r); combat.valid = 1; combat.stage = r[0x1f7a];
  }
  ++combat.tick;
  combat.held = (r[0xbdf] & 64) != 0; combat.pressed = (r[0xbe3] & 64) != 0;
  combat.direction = r[0xbe3] & 12;
  if (!MmxWeaponsCombatActive()) return;
  /* Use X1's established firing poses, charge effects and release sound
   * cleanup. The actual arm upgrade is bit $02; Zero's innate dash is $08. */
  MmxZeroCancel(r); r[0xc0f] = 2;
  if (!(r[0x1f99] & 2)) stop_charge(r);
  MmxWeaponsState s = MmxWeaponsGetState();
  if (!MmxWeaponsEnergyAmount(s.page,s.weapon)) { stop_charge(r); r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64; }
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].charged &&
      combat.shots[i].page == 2 && combat.shots[i].weapon == 4) {
    /* A second press controls the extended blade, not a second buster shot. */
    stop_charge(r); r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64;
    if (r[0xc25] < 2) r[0xc25] = 2;
  }
  /* Pause owns extended-page selection. Do not let native shoulder cycling
   * replace the underlying buster with an unrelated X1 special mid-shot. */
  r[0xbde] &= (uint8_t)~0x30; r[0xbe2] &= (uint8_t)~0x30;
}
void MmxWeaponsMarkShot(uint8_t r[0x20000], unsigned d) {
  if (!MmxWeaponsCombatActive() || !slot_valid(d) || !r[d]) return;
  MmxWeaponsState w = MmxWeaponsGetState();
  MmxWeaponShot *s = combat.shots + slot_index(d);
  memset(s,0,sizeof(*s)); s->active = 1; s->page = w.page; s->weapon = w.weapon;
  s->group = (uint8_t)weapon_group(w.page,w.weapon);
  s->charged = r[d + 10] == 3 && (r[0x1f99] & 2);
  s->facing = r[0xbb9] & 64;
  r[d + 10] = 0; putword(r+d+0x3e,0x5758);
}
static void shot_bounds(const MmxWeaponShot *s, unsigned *rx, unsigned *ry) {
  *rx = 13; *ry = 10;
  if (s->page == 2 && s->weapon == 1) {
    *rx = *ry = s->charged ? 4 : s->variant == 2 ? 4 : 6;
    if (s->variant >= 128) *rx = *ry = s->charged ? 8 : 4;
  }
}
static void collision_box(unsigned index, const MmxWeaponShot *s) {
  if (!collision_patch) return;
  unsigned rx,ry; shot_bounds(s,&rx,&ry);
  uint8_t *box = previous_boxes + index*4;
  box[0] = box[1] = 0; box[2] = (uint8_t)rx; box[3] = (uint8_t)ry;
  memcpy(collision_rom + 0x37f80 + index*4,box,4);
}
static void native_object(uint8_t *r, unsigned d, const MmxWeaponShot *s) {
  r[d] = 1; r[d+1] = 2; r[d+10] = 0; r[d+14] = 0;
  r[d+17] = (uint8_t)(0x22 | s->facing); r[d+0x28] = 1;
  putword(r+d+5,(unsigned)(s->x >> 8)); putword(r+d+8,(unsigned)(s->y >> 8));
  putword(r+d+0x20,0xff80 + slot_index(d) * 4); putword(r+d+0x3e,0x5758);
  r[d+0x30] = 0;
  collision_box(slot_index(d),s);
}
static unsigned free_slot(const uint8_t *r) {
  for (unsigned d=0x1228;d<0x1428;d+=64) if (!word(r+d)) return d;
  return 0;
}
static void muzzle_origin(const uint8_t *r, unsigned d, MmxWeaponShot *s) {
  int dx = (int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],0,16);
  int dy = (int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],1,253);
  s->x = ((int)word(r+0xbad) + (s->facing ? dx : -dx)) * 256;
  s->y = ((int)word(r+0xbb0) + dy) * 256;
  s->origin_x = (int16_t)(s->x >> 8); s->origin_y = (int16_t)(s->y >> 8);
}
/* X1 $84:90B3/$916A: live screen layout -> metatile -> collision class.
 * Read the current RAM map so destroyed terrain is immediately reflected. */
static unsigned terrain_type(const uint8_t *r, int x, int y) {
  if (!collision_rom || x < 0 || x >= 8192 || y < 0 || y >= 8192) return 0;
  unsigned screen = r[0xe800 + (y >> 8)*32 + (x >> 8)];
  unsigned cell = 0x2000 + ((screen*512 + ((y & 240) << 1) + ((x & 240) >> 3)) & 65535);
  unsigned address = word(r+0xb92) | (r[0xb94] << 16);
  address += word(r+cell);
  if ((address & 65535) < 0x8000) return 0;
  size_t offset = ((address >> 16) & 127)*0x8000 + (address & 0x7fff);
  return offset < collision_rom_size ? collision_rom[offset] & 63 : 0;
}
static int terrain_surface(unsigned type, int x) {
  unsigned u = (x & 15) + 1;
  /* X1's original half/quarter slopes, $84:96D3..9815. */
  if (type == 1) return 16 - (int)(u/2);
  if (type == 2) return 8 - (int)(u/2);
  if (type == 3) return 8 + (int)(u/2);
  if (type == 4) return (int)(u/2);
  if (type >= 5 && type <= 8) return 16 - (int)(type-5)*4 - (int)(u/4);
  if (type >= 9 && type <= 12) return 12 - (int)(type-9)*4 + (int)(u/4);
  if (type == 0x13 || type >= 0x33) return (type == 0x33 || type >= 0x3e) ? 2 : 0;
  return 17; /* Empty, water, ladders and decorative classes. */
}
static bool terrain_solid(const uint8_t *r, int x, int y, bool floor, int *surface) {
  unsigned type = terrain_type(r,x,y);
  if (!floor && ((type >= 1 && type <= 12) || type == 0x39 || type == 0x3a)) return false;
  int height = terrain_surface(type,x);
  if (height > (y & 15)) return false;
  if (surface) *surface = (y & ~15) + height;
  return true;
}
/* Sweep at one-pixel intervals, preserving the subpixel endpoint. Return
 * native-style contact bits: side 1, floor 4, ceiling 8. */
static unsigned terrain_move(const uint8_t *r, MmxWeaponShot *s, unsigned rx, unsigned ry) {
  unsigned hit = 0;
  int32_t target = s->x + s->vx;
  int direction = s->vx >= 0 ? 1 : -1;
  while (s->x != target) {
    int32_t next = s->x + direction*256;
    if ((direction > 0 && next > target) || (direction < 0 && next < target)) next = target;
    if (terrain_solid(r,(next >> 8)+direction*(int)rx,s->y >> 8,false,NULL)) { hit |= 1; break; }
    s->x = next;
  }
  target = s->y + s->vy; direction = s->vy >= 0 ? 1 : -1;
  while (s->y != target) {
    int32_t next = s->y + direction*256;
    if ((direction > 0 && next > target) || (direction < 0 && next < target)) next = target;
    int surface = 0;
    if (terrain_solid(r,s->x >> 8,(next >> 8)+direction*(int)ry,direction > 0,&surface)) {
      if (direction > 0) { s->y = (surface-1-(int)ry)*256; hit |= 4; }
      else hit |= 8;
      break;
    }
    s->y = next;
  }
  return hit;
}
static MmxWeaponShot *spawn_child(uint8_t *r, const MmxWeaponShot *parent) {
  unsigned d = free_slot(r); if (!d) return NULL;
  MmxWeaponShot *s = combat.shots + slot_index(d); *s = *parent;
  s->age = 1; s->born = combat.tick; s->hit_slots = 0;
  memset(r+d,0,64); native_object(r,d,s); ++r[0xbdd]; return s;
}
static void acid_splash(MmxWeaponShot *s, unsigned contact) {
  s->variant = 128; s->radius = 0; s->hit_slots = 0;
  unsigned sequence = (contact & 1) ? 11 : (contact & 8) ? 10 : 7;
  s->vx = sequence == 11 ? 24 : sequence == 10 ? 42 : 40; s->vy = 0;
  if (contact & 1) s->facing ^= 64;
  animation_start(s,sequence);
}
static void acid_fragments(uint8_t *r, const MmxWeaponShot *parent) {
  /* X3 $06:B8AD..B90B: four droplets, oriented for the struck surface. */
  static const int16_t dx[3][4] = {{0,-6,-10,-12},{0,0,0,0},{0,0,0,0}};
  static const int16_t dy[3][4] = {{-8,-6,-4,0},{0,0,0,0},{0,0,0,0}};
  static const int16_t vx[3][4] = {{64,128,256,448},{512,192,-192,-512},{576,256,-256,-576}};
  static const int16_t vy[3][4] = {{-768,-512,-384,-256},{64,64,64,64},{-512,-768,-576,-384}};
  unsigned kind = parent->muzzle_pose;
  for (unsigned i=0;i<4;++i) {
    MmxWeaponShot *s = spawn_child(r,parent); if (!s) break;
    s->charged = 0; s->variant = 2; s->radius = 10;
    s->x += dx[kind][i]*256; s->y += dy[kind][i]*256;
    s->vx = vx[kind][i]*(parent->facing ? 1 : -1); s->vy = vy[kind][i];
    s->facing &= 64; s->muzzle_pose = 0; animation_start(s,4);
  }
}
static void acid_tick(uint8_t *r, unsigned d, MmxWeaponShot *s) {
  /* Acid variants: 0/1 primary blobs, 2 droplets, 128 surface splash,
   * 129 droplet splash, 130 enemy impact. Radius holds bounces/grace frames;
   * muzzle_pose holds the surface orientation for the four-droplet emission. */
  if (!s->age) {
    if (!MmxWeaponsSpend(s->page,s->weapon,(s->charged ? 2 : 1)*256)) { retire(r,d); return; }
    muzzle_origin(r,d,s); s->born = combat.tick; s->age = 1;
    s->radius = s->charged ? 5 : 0;
    s->vx = (s->facing ? 1 : -1) * (s->charged ? 192 : 288);
    s->vy = s->charged ? -1280 : -640;
    if (!s->charged && (r[0xbdf] & 12)) {
      s->vx = 0; s->vy = (r[0xbdf] & 8) ? -1280 : 1280;
    }
    animation_start(s,s->charged ? 12 : 2);
    if (s->charged) {
      MmxWeaponShot *t = spawn_child(r,s);
      if (t) { t->variant = 1; t->vx = (t->facing ? 1 : -1)*512; t->vy = -768; }
    }
    native_object(r,d,s); return;
  }
  if (s->age == 1 && s->born == combat.tick) return;
  if (s->variant < 128 && r[d+1] >= 8) {
    s->variant = 130; s->vx = 30; s->vy = 0; animation_start(s,6);
  }
  if (s->variant >= 128) {
    if (!s->vx) { retire(r,d); return; }
    --s->vx;
    if (s->variant == 128 && (s->flags & 1) && !s->radius) {
      s->radius = 1;
      if (!s->charged) acid_fragments(r,s);
    }
  } else {
    s->vy += s->charged ? 56 : s->variant == 2 ? 48 : 32;
    if (s->vy > (s->charged ? 1280 : 1536)) s->vy = s->charged ? 1280 : 1536;
    unsigned rx = s->charged || s->variant == 2 ? 4 : 6, hit;
    if (s->variant == 2 && s->radius) { --s->radius; s->x += s->vx; s->y += s->vy; hit = 0; }
    else hit = terrain_move(r,s,rx,rx);
    if (hit) {
      if (s->charged && s->radius) {
        MmxWeaponShot *p = spawn_child(r,s);
        if (p) acid_splash(p,hit);
        if (!--s->radius) { retire(r,d); return; }
        if (hit & 1) { s->vx = -s->vx; s->facing ^= 64; }
        else { s->vx = (s->facing ? 1 : -1)*256; s->vy = (hit & 8) ? 1280 : -1280; }
      } else if (s->variant == 2) {
        s->variant = 129; s->vx = 24; s->vy = 0; animation_start(s,5);
      } else {
        s->muzzle_pose = (hit & 1) ? 0 : (hit & 8) ? 1 : 2;
        acid_splash(s,hit);
      }
    }
  }
  if (++s->age > 360 || s->x/256 < (int)word(r+0x1e4d)-128 ||
      s->x/256 > (int)word(r+0x1e4d)+384 || s->y/256 < (int)word(r+0x1e50)-192 ||
      s->y/256 > (int)word(r+0x1e50)+416) { retire(r,d); return; }
  /* The growth sequence marks its final size with bit 1. */
  if (s->charged && s->variant < 128 && (s->flags & 2)) animation_start(s,0);
  else animation_step(s);
  if (!s->active) { retire(r,d); return; }
  native_object(r,d,s);
  if (s->variant >= 129 || (!s->charged && s->variant == 128)) putword(r+d+0x20,0);
}
unsigned MmxWeaponsProjectileTick(uint8_t r[0x20000], unsigned d, unsigned active) {
  if (!owned(r,d)) {
    if (slot_valid(d)) memset(combat.shots+slot_index(d),0,sizeof(MmxWeaponShot));
    return active;
  }
  MmxWeaponShot *s = combat.shots + slot_index(d);
  if (!MmxWeaponsEnabled() || !s->active) { retire(r,d); return 0; }
  if (s->page == 2 && s->weapon == 1) { acid_tick(r,d,s); return 0; }
  if (!s->charged && s->age && (r[d+1] >= 8 || s->variant == 128)) {
    if (s->variant != 128) { s->variant = 128; s->vy = 26; animation_start(s,1); }
    if (!s->vy || !s->active) { retire(r,d); return 0; }
    --s->vy; animation_step(s); native_object(r,d,s); putword(r+d+0x20,0);
    return 0;
  }
  int direction = s->facing ? 1 : -1;
  if (!s->age) {
    if (!MmxWeaponsSpend(s->page,s->weapon,(s->charged ? 3 : 1)*256)) { retire(r,d); return 0; }
    muzzle_origin(r,d,s); s->vx = (int16_t)(direction * 1024); s->vy = 0;
    animation_start(s,s->charged ? 4 : 0); s->born = combat.tick;
    if (!s->charged) {
      unsigned other = free_slot(r);
      if (other) {
        MmxWeaponShot *t = combat.shots + slot_index(other); *t = *s;
        t->variant = 1; t->age = 1;
        memset(r+other,0,64); native_object(r,other,t); ++r[0xbdd];
      }
      s->age = 1; native_object(r,d,s); return 0;
    }
  }
  if (!s->charged && s->age == 1 && s->born == combat.tick) return 0;
  if (s->charged) {
    /* Native charged Blade extends to 80px. Up/down initiates a complete
     * 64-step turn; after that turn the blade retracts to the arm. */
    static const int8_t circle[64] = {0,7,15,23,30,37,44,50,56,61,66,70,73,76,78,79,
      80,79,78,76,73,70,66,61,56,50,44,37,30,23,15,7,0,-7,-15,-23,-30,-37,-44,-50,-56,-61,-66,-70,-73,-76,-78,-79,
      -80,-79,-78,-76,-73,-70,-66,-61,-56,-50,-44,-37,-30,-23,-15,-7};
    muzzle_origin(r,d,s);
    static const uint8_t muzzle[4] = {39,40,41,40};
    s->muzzle_pose = muzzle[(s->age / 2) & 3];
    if (!s->variant && s->radius >= 80 && (combat.direction || combat.pressed)) {
      s->variant = (combat.direction & 4) ? 2 : 1; s->vx = 0;
    }
    int dx, dy;
    if (s->variant == 1 || s->variant == 2) {
      unsigned angle = (unsigned)s->vx & 63;
      dx = circle[(angle + 16) & 63]; dy = circle[angle] * (s->variant == 1 ? -1 : 1);
      /* Pose 66 points left in the source art; the actor facing flip makes
       * that right. Advance through the original 32 orientations accordingly. */
      s->tether_pose = (uint8_t)(42 + ((24 + (s->variant == 1 ? (int)(angle / 2) : -(int)(angle / 2))) & 31));
      if (++s->vx >= 64) s->variant = 3;
    } else {
      if (s->variant == 3 || s->age > 140) {
        if (s->radius <= 4) { retire(r,d); return 0; }
        s->radius -= 4;
      } else if (s->radius < 80) s->radius += 4;
      s->tether_pose = (uint8_t)(30 + (s->radius >= 72 ? 8 : s->radius / 8));
      dx = s->radius; dy = 0;
    }
    s->x += direction * dx * 256; s->y += dy * 256;
  } else {
    s->vx -= (int16_t)(direction * 32);
    s->vy += s->variant ? -16 : 16;
    if (s->vy > 192) s->vy = 192;
    if (s->vy < -192) s->vy = -192;
    s->x += s->vx; s->y += s->vy;
  }
  if (++s->age > 240 || s->x / 256 < (int)word(r+0x1e4d)-256 ||
      s->x / 256 > (int)word(r+0x1e4d)+512 || s->y / 256 < (int)word(r+0x1e50)-256 ||
      s->y / 256 > (int)word(r+0x1e50)+480) { retire(r,d); return 0; }
  if (!(s->age % 16)) s->hit_slots = 0;
  animation_step(s);
  if (!s->active) { retire(r,d); return 0; }
  native_object(r,d,s);
  return 0;
}
void MmxWeaponsCollisionRom(uint8_t *rom, size_t size) {
  collision_rom = rom; collision_rom_size = size; collision_patch = false;
  uint8_t empty[32]; memset(empty,255,32);
  if (!rom || size < 0x37fa0 || (memcmp(rom+0x37f80,empty,32) && memcmp(rom+0x37f80,previous_boxes,32))) return;
  collision_patch = MmxWeaponsEnabled();
  memcpy(previous_boxes,empty,32); memcpy(rom+0x37f80,empty,32);
  if (collision_patch) for (unsigned i=0;i<8;++i) collision_box(i,combat.shots+i);
}
static unsigned enemy_bit(unsigned enemy) {
  return enemy >= 0xe68 && enemy < 0x1228 && (enemy & 63) == 0x28 ? 1u << ((enemy-0xe68)/64) : 0;
}
unsigned MmxWeaponsDamage(uint8_t r[0x20000], unsigned enemy, unsigned d, unsigned original) {
  if (!owned(r,d) || !original || (original & 128)) return original;
  MmxWeaponShot *s = combat.shots + slot_index(d);
  unsigned bit = enemy_bit(enemy);
  if (s->hit_slots & bit) return 0;
  s->hit_slots |= (uint16_t)bit;
  return original; /* Native buster damage/immunity; no new weakness table. */
}
unsigned MmxWeaponsHitbox(const uint8_t r[0x20000], unsigned enemy, unsigned d, unsigned original) {
  if (owned(r,d) && (combat.shots[slot_index(d)].hit_slots & enemy_bit(enemy))) return 0;
  return original;
}

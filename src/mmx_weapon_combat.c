#include "mmx_weapon_combat.h"
#include "mmx_weapons.h"
#include "mmx_zero.h"
#include <string.h>

static MmxWeaponCombatState combat;
_Static_assert(sizeof(MmxWeaponShot) == 40 && sizeof(MmxWeaponCombatState) == 328, "Weapon combat save ABI");
static unsigned word(const uint8_t *p) { return p[0] | (p[1] << 8); }
static void putword(uint8_t *p, unsigned value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
static bool slot_valid(unsigned d) { return d >= 0x1228 && d < 0x1428 && (d & 63) == 0x28; }
static bool owned(const uint8_t *r, unsigned d) { return slot_valid(d) && r[d] && word(r + d + 0x3e) == 0x5758; }
static unsigned slot_index(unsigned d) { return (d - 0x1228) / 64; }
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
        (p->active && (p->page != 2 || p->weapon != 4 || p->group != 12))) return false;
  }
  return true;
}
void MmxWeaponsSetCombatState(MmxWeaponCombatState s) {
  memset(&combat, 0, sizeof(combat));
  if (MmxWeaponsValidCombatState(&s)) combat = s;
}
bool MmxWeaponsCombatActive(void) {
  MmxWeaponsState s = MmxWeaponsGetState();
  return MmxWeaponsEnabled() && s.page == 2 && s.weapon == 4;
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
  if (!s.energy[11]) { stop_charge(r); r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64; }
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].charged) {
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
  MmxWeaponShot *s = combat.shots + slot_index(d);
  memset(s,0,sizeof(*s)); s->active = 1; s->page = 2; s->weapon = 4; s->group = 12;
  s->charged = r[d + 10] == 3 && (r[0x1f99] & 2);
  s->facing = r[0xbb9] & 64;
  r[d + 10] = 0; putword(r+d+0x3e,0x5758);
}
static void native_object(uint8_t *r, unsigned d, const MmxWeaponShot *s) {
  r[d] = 1; r[d+1] = 2; r[d+10] = 0; r[d+14] = 0;
  r[d+17] = (uint8_t)(0x22 | s->facing); r[d+0x28] = 1;
  putword(r+d+5,(unsigned)(s->x >> 8)); putword(r+d+8,(unsigned)(s->y >> 8));
  putword(r+d+0x20,0xff80 + slot_index(d) * 4); putword(r+d+0x3e,0x5758);
  r[d+0x30] = 0;
}
static unsigned free_slot(const uint8_t *r) {
  for (unsigned d=0x1228;d<0x1428;d+=64) if (!word(r+d)) return d;
  return 0;
}
static void blade_origin(const uint8_t *r, unsigned d, MmxWeaponShot *s) {
  int dx = (int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],0,16);
  int dy = (int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],1,253);
  s->x = ((int)word(r+0xbad) + (s->facing ? dx : -dx)) * 256;
  s->y = ((int)word(r+0xbb0) + dy) * 256;
  s->origin_x = (int16_t)(s->x >> 8); s->origin_y = (int16_t)(s->y >> 8);
}
unsigned MmxWeaponsProjectileTick(uint8_t r[0x20000], unsigned d, unsigned active) {
  if (!owned(r,d)) {
    if (slot_valid(d)) memset(combat.shots+slot_index(d),0,sizeof(MmxWeaponShot));
    return active;
  }
  MmxWeaponShot *s = combat.shots + slot_index(d);
  if (!MmxWeaponsEnabled() || !s->active) { retire(r,d); return 0; }
  if (!s->charged && s->age && (r[d+1] >= 8 || s->variant == 128)) {
    if (s->variant != 128) { s->variant = 128; s->vy = 26; animation_start(s,1); }
    if (!s->vy || !s->active) { retire(r,d); return 0; }
    --s->vy; animation_step(s); native_object(r,d,s); putword(r+d+0x20,0);
    return 0;
  }
  int direction = s->facing ? 1 : -1;
  if (!s->age) {
    MmxWeaponsState weapons = MmxWeaponsGetState();
    unsigned cost = s->charged ? 3 : 1;
    if (weapons.energy[11] < cost) { retire(r,d); return 0; }
    weapons.energy[11] -= (uint8_t)cost; MmxWeaponsSetState(weapons);
    blade_origin(r,d,s); s->vx = (int16_t)(direction * 1024); s->vy = 0;
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
    blade_origin(r,d,s);
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
  static uint8_t previous[32];
  uint8_t empty[32]; memset(empty,255,32);
  if (!rom || size < 0x37fa0 || (memcmp(rom+0x37f80,empty,32) && memcmp(rom+0x37f80,previous,32))) return;
  memcpy(previous,empty,32);
  if (MmxWeaponsEnabled()) for (unsigned i=0;i<8;++i) {
    previous[i*4] = previous[i*4+1] = 0;
    previous[i*4+2] = 13; previous[i*4+3] = 10;
  }
  memcpy(rom+0x37f80,previous,32);
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

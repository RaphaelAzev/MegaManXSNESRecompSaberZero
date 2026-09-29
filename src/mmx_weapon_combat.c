#include "mmx_weapon_combat.h"
#include "mmx_weapons.h"
#include "mmx_zero.h"
#include <string.h>

static MmxWeaponCombatState combat;
static uint8_t *collision_rom;
static size_t collision_rom_size;
static bool collision_patch;
static uint8_t previous_boxes[32];
static void fang_player(uint8_t *r);
_Static_assert(sizeof(MmxWeaponShot) == 40 && offsetof(MmxWeaponCombatState,enemies) == MMX_WEAPON_COMBAT_LEGACY_SIZE &&
    offsetof(MmxWeaponCombatState,effects)==MMX_WEAPON_COMBAT_DAMAGE_SIZE &&
    sizeof(MmxWeaponCombatState) == 1028, "Weapon combat save ABI");
static unsigned word(const uint8_t *p) { return p[0] | (p[1] << 8); }
static void putword(uint8_t *p, unsigned value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8); }
static bool slot_valid(unsigned d) { return d >= 0x1228 && d < 0x1428 && (d & 63) == 0x28; }
static bool owned(const uint8_t *r, unsigned d) { return slot_valid(d) && r[d] && word(r + d + 0x3e) == 0x5758; }
static unsigned slot_index(unsigned d) { return (d - 0x1228) / 64; }
static unsigned weapon_group(unsigned page, unsigned weapon) {
  return page == 1 ? (weapon == 2 ? 68 : weapon == 4 ? 70 : weapon == 5 ? 65 : weapon == 7 ? 15 : weapon == 8 ? 37 : 0) :
    page == 2 ? (weapon == 1 ? 5 : weapon == 4 ? 12 : weapon == 5 ? 13 : weapon == 7 ? 16 : weapon == 8 ? 19 : 0) : 0;
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
  for (unsigned i=0;i<15;++i) if (s->enemies[i].remainder>2 ||
      s->enemies[i].hp>127 || s->enemies[i].active>1) return false;
  for (unsigned i = 0; i < 24; ++i) {
    const MmxWeaponShot *p = i<8 ? s->shots+i : s->effects+i-8;
    if (i>=8 && p->active && (p->page!=1 || p->weapon!=8 || p->variant!=4 || p->charged)) return false;
    if (p->active > 1 || p->reserved || p->charged > 1 || p->pose >= 128 || p->animation > 8192 ||
        p->x < -0x1000000 || p->x > 0x1000000 || p->y < -0x1000000 || p->y > 0x1000000 ||
        (p->active && (!weapon_group(p->page,p->weapon) || p->group !=
          (p->page == 1 && p->weapon == 5 && p->charged ? 135 :
           p->page == 1 && p->weapon == 7 ? (p->charged ? 19 : p->muzzle_pose==3 ? 8 : 15) :
           p->page == 1 && p->weapon == 8 && p->charged ? 38 :
           weapon_group(p->page,p->weapon))))) return false;
    if (p->active && p->page == 1 && p->weapon == 5 &&
        (p->variant > 4 || p->radius > 2 || p->muzzle_pose > 3)) return false;
    if (p->active && p->page==1 && p->weapon==2 &&
        (p->variant>(p->charged ? 7 : 10) || p->muzzle_pose>3 || p->radius>63 ||
         p->tether_pose>(p->charged ? 2 : 7))) return false;
    if (p->active && p->page==1 && p->weapon==7 &&
        (p->variant>2 || p->muzzle_pose>3 || p->radius>(p->charged ? 32 : 60))) return false;
    if (p->active && p->page==1 && p->weapon==8 &&
        (p->variant>(i<8 ? 3 : 4) || p->muzzle_pose>3 || p->radius>60 || p->tether_pose>1)) return false;
    if (p->active && p->page == 1 && p->weapon == 4 &&
        (p->variant > (p->charged ? 7 : 0) || p->radius > (p->charged ? 6 : 30) ||
         p->muzzle_pose > (p->charged ? 1 : 6) || p->tether_pose > (p->charged ? 1 : 10))) return false;
    if (p->active && p->page == 2 && p->weapon == 1 &&
        (p->muzzle_pose > 2 || p->radius > 10 ||
         (p->variant > 2 && p->variant != 128 && p->variant != 129 && p->variant != 130))) return false;
    if (p->active && p->page == 2 && p->weapon == 5 && (p->variant > 3 || p->radius > 16)) return false;
    if (p->active && p->page == 2 && p->weapon == 7 &&
        (p->variant > 2 || p->muzzle_pose > 7 ||
         p->radius > 8 || p->tether_pose > 1 || p->origin_x < 0 || p->origin_x > 360)) return false;
    if (p->active && p->page==2 && p->weapon==8 &&
        (p->variant>(p->charged?5:2) || p->muzzle_pose>4 || p->radius>64 || p->tether_pose>7)) return false;
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
  const MmxWeaponShot *s=combat.shots+slot_index(d);
  if (s->active && s->page==1 && s->weapon==8 && s->charged && s->age) {
    r[0xbd8]=0;
    r[0xc26]&=(uint8_t)~64;
    if (r[0xbaa]==0x14) { r[0xbaa]=(r[0xbd3]&4) ? 0 : 8;r[0xbab]=0; }
    if (s->variant) r[0xc06]&=(uint8_t)~4;
  }
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
static void cycle_weapon(uint8_t *r) {
  MmxWeaponsState s=MmxWeaponsGetState();
  if (!s.page) return; /* X1 keeps its native ownership-aware cycle. */
  unsigned held=r[0xbde]&0x30,pressed=r[0xbe2]&0x30;
  /* Consume shoulders even for the buster and unfinished weapons. Native
   * $81:99D3 must never select an X1 weapon behind an extended selection. */
  /* Leave held input intact: native edge detection uses it next frame.
   * Both held buttons may still run native buster cleanup, which is correct. */
  r[0xbe2]&=(uint8_t)~0x30;
  bool bubble_shield=false;
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].page==1 &&
      combat.shots[i].weapon==2 && combat.shots[i].charged) bubble_shield=true;
  if (!pressed || r[0xd1]!=2 || r[0xd2]!=4 || r[0xd3]!=4 || r[0xba9]!=2 ||
      r[0x1f23] || (r[0xbdd] && !bubble_shield) || r[0x1f31] ||
      r[0xbaa]==0x18 || r[0xbaa]==0x42) return;
  unsigned weapon=held==0x30 ? 0 : (s.weapon+(pressed&0x20 ? 8 : 1))%9;
  if (weapon==s.weapon) return;
  MmxZeroCancel(r); stop_charge(r); MmxWeaponsCancelShots(r);
  s.weapon=(uint8_t)weapon; s.charge=s.cooldown=0; MmxWeaponsSetState(s);
  /* Native $81:9A49 restores +$67 from $86:BAB8 on a weapon change.
   * Buster entry is three; leaving zero makes $81:93B5 reject all fire. */
  r[0xbdb]=0; r[0xc0f]=3; r[0x1f12]=weapon ? 0 : 4;
}
static MmxWeaponShot *speed_dash(void) {
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].age &&
      combat.shots[i].page==1 && combat.shots[i].weapon==8 && combat.shots[i].charged)
    return combat.shots+i;
  return NULL;
}
/* Keep X1's native dash, wall/ground collision, animation and camera. X2's
 * charged dash changes its speed to $0475 and timer to $30/$18. This hook
 * runs before native horizontal integration, including the first tick. */
void MmxWeaponsPlayerMotion(uint8_t r[0x20000],unsigned d) {
  if (!MmxWeaponsEnabled() || d!=0xba8) return;
  MmxWeaponShot *s=speed_dash();if (!s || r[0xbaa]!=0x14) return;
  putword(r+0xc04,1141);putword(r+0xbc2,s->facing ? 1141 : -1141);
  r[0xbfa]=s->radius;
}
void MmxWeaponsPlayerTick(uint8_t r[0x20000]) {
  if (!MmxWeaponsEnabled()) return;
  cycle_weapon(r);
  if (!combat.valid || combat.stage != r[0x1f7a] || r[0xd1] != 2 || !(r[0xbcf] & 127)) {
    MmxWeaponsCancelShots(r); combat.valid = 1; combat.stage = r[0x1f7a];
  }
  ++combat.tick;
  for (unsigned i=0;i<16;++i) {
    MmxWeaponShot *p=combat.effects+i;if (!p->active) continue;
    if (++p->age>=32) { memset(p,0,sizeof(*p));continue; }
    p->vy+=(int16_t)(p->tether_pose ? -(int)p->radius : (int)p->radius);
    p->x+=p->vx;p->y+=p->vy;
  }
  for (unsigned i=0;i<15;++i) {
    unsigned d=0xe68+i*64,hp=r[d+0x27]&127;
    MmxWeaponDamageState *e=combat.enemies+i;
    if (!r[d] || !r[d+1] || !hp || e->kind!=r[d+10] || hp>e->hp)
      memset(e,0,sizeof(*e));
    if (e->active) e->hp=(uint8_t)hp;
  }
  combat.held = (r[0xbdf] & 64) != 0; combat.pressed = (r[0xbe3] & 64) != 0;
  combat.direction = r[0xbe3] & 12;
  if (!MmxWeaponsCombatActive()) return;
  /* Use X1's established firing poses, charge effects and release sound
   * cleanup. The actual arm upgrade is bit $02; Zero's innate dash is $08. */
  MmxZeroCancel(r); r[0xc0f] = 2;
  if (!(r[0x1f99] & 2)) stop_charge(r);
  MmxWeaponsState s = MmxWeaponsGetState();
  if (s.page==1 && s.weapon==8) {
    r[0xc0f]=1;
    MmxWeaponShot *p=speed_dash();
    if (p) {
      stop_charge(r);r[0xbdf]&=(uint8_t)~64;r[0xbe3]&=(uint8_t)~64;
      r[0xc11]=p->facing;r[0xbb9]=(r[0xbb9]&~64)|p->facing;
      if (p->variant) { r[0xc06]|=4;r[0xbe3]&=(uint8_t)~128;putword(r+0xbc4,0); }
      /* Source dash can be cancelled by the opposite direction. Keep
       * actual direction input; synthesize only the native dash button. */
      r[0xbde]|=128;
      r[0xc26]|=64; /* Native dash-button mode ($81:9D24). */
    }
  }
  if (s.page==1 && s.weapon==7) r[0xc0f]=8; /* Planted mines release the source firing limit. */
  if (s.page==2 && s.weapon==8) fang_player(r);
  if (s.page==1 && s.weapon==2) {
    bool shield=false;
    for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].page==1 &&
        combat.shots[i].weapon==2 && combat.shots[i].charged) shield=true;
    r[0xc0f]=8; /* Seven bubbles, with room for the charged controller. */
    if (shield) { stop_charge(r);r[0xbdf]&=(uint8_t)~64;r[0xbe3]&=(uint8_t)~64; }
    else if (combat.held && r[0xc03]!=1) {
      /* X2 $88:C452: synthesize another fire edge every two ticks until
       * fully charged. Native X1 still owns pose, charge and allocation. */
      if (combat.pressed || !s.cooldown || !--s.cooldown) { r[0xbe3]|=64;s.cooldown=2; }
    } else s.cooldown=0;
    MmxWeaponsSetState(s);
  }
  if (!MmxWeaponsEnergyAmount(s.page,s.weapon)) { stop_charge(r); r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64; }
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].charged &&
      combat.shots[i].page == 2 && (combat.shots[i].weapon == 4 ||
        (combat.shots[i].weapon == 7 && combat.shots[i].muzzle_pose < 3))) {
    /* A second press controls the extended blade, not a second buster shot. */
    stop_charge(r); r[0xbdf] &= (uint8_t)~64; r[0xbe3] &= (uint8_t)~64;
    if (combat.shots[i].weapon==7 && r[0xbaa]!=14 && r[0xbaa]!=12 && !r[0x1f0c]) r[0xbf8]=2;
  }
  for (unsigned i=0;i<8;++i) {
    const MmxWeaponShot *p = combat.shots+i;
    if (p->active && p->page == 2 && p->weapon == 5 && p->variant < 2) {
      /* The normal burst owns one muzzle; holding can still build a charge. */
      if (!p->charged) {
        r[0xbe3] &= (uint8_t)~64;
        /* X3 keeps body +$50 active for the whole 60-frame burst. X1's
         * ordinary shot overlay expires much sooner; retain its movement
         * animation and use the actual firing timer, never beam count C25. */
        if (p->age && p->age<60 && r[0xbaa]!=14 && r[0xbaa]!=12 && !r[0x1f0c])
          r[0xbf8]=(uint8_t)(60-p->age);
      }
    }
  }
}
void MmxWeaponsSelectShot(const uint8_t r[0x20000], unsigned d) {
  if (!MmxWeaponsCombatActive() || !slot_valid(d)) return;
  /* Moving/air firing ($81:93EA) clears +$59 before it calls $9D47.
   * Capture the command at $94AF, shared by every firing pose. */
  combat.shots[slot_index(d)].charged=r[0xc01]==4 && (r[0x1f99]&2);
}
void MmxWeaponsMarkShot(uint8_t r[0x20000], unsigned d) {
  if (!MmxWeaponsCombatActive() || !slot_valid(d) || !r[d]) return;
  MmxWeaponsState w = MmxWeaponsGetState();
  MmxWeaponShot *s = combat.shots + slot_index(d);
  bool charged=s->charged!=0;
  memset(s,0,sizeof(*s)); s->active = 1; s->page = w.page; s->weapon = w.weapon;
  s->group = (uint8_t)weapon_group(w.page,w.weapon);
  /* X1 $81:94FD..951F identifies full special-weapon release by player
   * +$59 == 4. Buster class 3 is the intermediate arm-upgraded beam;
   * using that class inverted medium/full releases after a longer hold. */
  s->charged = charged;
  if (s->page == 1 && s->weapon == 5 && s->charged) s->group = 135;
  if (s->page == 1 && s->weapon == 7 && s->charged) s->group = 19;
  if (s->page == 1 && s->weapon == 8 && s->charged) s->group = 38;
  s->facing = r[0xbb9] & 64;
  r[d + 10] = 0; putword(r+d+0x3e,0x5758);
}
static void shot_bounds(const MmxWeaponShot *s, unsigned *rx, unsigned *ry) {
  *rx = 13; *ry = 10;
  if (s->page==1 && s->weapon==2) *rx=*ry=7;
  if (s->page==1 && s->weapon==8) {
    static const uint8_t boxes[4][2]={{6,7},{9,8},{10,12},{12,16}};
    unsigned phase=s->flags&3;*rx=boxes[phase][0];*ry=boxes[phase][1];
    if (s->variant==1) { *rx=9;*ry=7; }
    if (s->charged) *rx=*ry=20;
  }
  if (s->page==1 && s->weapon==7) {
    *rx=*ry=s->charged ? 8*(s->variant+1) : 4;
    if (!s->charged && s->muzzle_pose==3) { *rx=17;*ry=18; }
  }
  if (s->page == 1 && s->weapon == 4) *rx = *ry = s->charged ? (s->muzzle_pose ? 6 : 7) : 9;
  if (s->page == 1 && s->weapon == 5) {
    *rx = s->charged ? 10 : 12; *ry = s->charged ? 11 : 8;
  }
  if (s->page == 2 && s->weapon == 1) {
    *rx = *ry = s->charged ? 4 : s->variant == 2 ? 4 : 6;
    if (s->variant >= 128) *rx = *ry = s->charged ? 8 : 4;
  }
  if (s->page == 2 && s->weapon == 5) {
    *rx = s->charged && s->variant < 2 ? 16 : 8; *ry = 8;
  }
  if (s->page == 2 && s->weapon == 7) {
    *rx=5;*ry=7;
    if (s->charged) {
      *rx=s->muzzle_pose>=5 ? 16 : s->muzzle_pose==3 ? 11 : 10;
      *ry=s->muzzle_pose>=5 ? 24 : s->muzzle_pose==3 ? 7 : 17;
    }
    else if (s->muzzle_pose==1) { *rx=s->variant ? 17 : 14;*ry=s->variant ? 9 : 7; }
    else if (s->muzzle_pose==5) { *rx=s->variant ? 23 : 15;*ry=s->variant ? 14 : 9; }
  }
  if (s->page==2 && s->weapon==8) {
    *rx=s->charged?4:9;*ry=s->charged?(s->variant==2?16:6):7;
    if (s->charged && s->variant==3) *rx=*ry=0;
  }
}
static void collision_box(unsigned index, const MmxWeaponShot *s) {
  if (!collision_patch) return;
  unsigned rx,ry; shot_bounds(s,&rx,&ry);
  uint8_t *box = previous_boxes + index*4;
  box[0] = box[1] = 0; box[2] = (uint8_t)rx; box[3] = (uint8_t)ry;
  if (s->page == 1 && s->weapon == 5 && s->charged) box[1] = 254;
  if (s->page==1 && s->weapon==7 && !s->charged && s->muzzle_pose==3) box[1]=248;
  if (s->page==1 && s->weapon==8) {
    if (s->charged) box[1]=4;
    else if (s->variant==1) { box[0]=3;box[1]=255; }
  }
  if (s->page == 2 && s->weapon == 7) {
    if (s->charged && s->muzzle_pose<3) box[0]=(uint8_t)-3;
    else if (!s->charged && s->muzzle_pose==1) box[0]=(uint8_t)(s->variant ? -13 : -7);
    else if (!s->charged && s->muzzle_pose==5) box[1]=(uint8_t)(s->variant ? -12 : -7);
  }
  if (s->page==2 && s->weapon==8) {
    if (!s->charged) box[0]=251;
    else if (s->variant==2) { box[0]=252;box[1]=248; }
    else if (s->variant!=3) box[1]=250;
  }
  memcpy(collision_rom + 0x37f80 + index*4,box,4);
}
static void native_object(uint8_t *r, unsigned d, const MmxWeaponShot *s) {
  r[d] = 1; r[d+1] = 2; r[d+10] = 0; r[d+14] = 0;
  r[d+17] = (uint8_t)(0x22 | s->facing); r[d+0x28] = 1;
  putword(r+d+5,(unsigned)(s->x >> 8)); putword(r+d+8,(unsigned)(s->y >> 8));
  putword(r+d+0x20,0xff80 + slot_index(d) * 4); putword(r+d+0x3e,0x5758);
  r[d+0x30] = 0;
  if (s->page==2 && s->weapon==7 && s->variant!=2)
    r[d+0x30]=(uint8_t)(s->charged ? s->muzzle_pose<2 : s->age<3);
  if (s->page==1 && s->weapon==7 && (s->charged || s->muzzle_pose==3)) r[d+0x30]=s->age&1;
  if (s->page==1 && s->weapon==8 && s->charged) r[d+0x30]=s->age&1;
  if (s->page==2 && s->weapon==8 && !s->charged && s->muzzle_pose==3) r[d+0x30]=s->radius;
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
static bool terrain_water(const uint8_t *r,int x,int y) {
  unsigned type=terrain_type(r,x,y);
  return type==13 || type==14; /* X1 $84:987E water probes. */
}
/* Extend the native terrain result, before the player consumes it. Both X
 * and Zero's translated terrain boxes put their feet 16px below the origin.
 * Native +$24 is the preceding Y position. Upward jumps pass through. */
void MmxWeaponsTerrainEnd(uint8_t r[0x20000],unsigned object) {
  if (!MmxWeaponsEnabled() || object!=0xba8) return;
  for (unsigned i=0;i<8;++i) {
    MmxWeaponShot *s=combat.shots+i;
    if (!s->active || s->page!=2 || s->weapon!=7 || !s->charged || s->variant!=1 ||
        s->muzzle_pose<5 || s->muzzle_pose>6) continue;
    s->tether_pose=0;
    int top=(s->y>>8)-24,x=(int)word(r+0xbad)-(s->x>>8);
    int feet=(int)word(r+0xbb0)+16,previous=(int)word(r+0xbcc)+16;
    if ((int16_t)word(r+0xbc4)>0 || x < -22 || x > 22 || previous>top+2 || feet<top ||
        r[0xbaa]==14 || r[0xbaa]==12 || r[0x1f0c]) continue;
    putword(r+0xbb0,top-16);r[0xbaf]=0;r[0xbd3]|=4;r[0xbd7]=0;s->tether_pose=1;
  }
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
/* X2 $86:B85D..B90B: each charged-bubble movement record lasts two
 * ticks. Velocities use the source upward-positive Y convention. */
static const int16_t bubble_route[29][2]={
  {1664,384},{1024,640},{-256,512},{-1152,384},{-1536,128},
  {-1408,-256},{-1024,-512},{128,-256},{1024,-128},{1536,128},
  {1536,384},{1408,768},{-384,512},{-1408,768},{-1408,128},
  {-1280,-128},{-1152,-512},{128,-512},{1152,-128},{1408,256},
  {1280,640},{768,640},{-256,384},{-640,256},{-1408,128},
  {-896,-256},{-896,-128},{256,-256},{1280,128}
};
static unsigned bubble_random(unsigned salt) {
  /* Source RNG arithmetic ($88:CB42), seeded from saved simulation state.
   * Independent of host frame rate; source and X1 consume RNG differently. */
  unsigned seed=(combat.tick*109u+salt*977u+0x731du)&65535;
  unsigned mixed=(seed*3u)&65535;
  return ((seed&255)+(mixed>>8))&255;
}
static unsigned bubble_manager(void) {
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].page==1 &&
      combat.shots[i].weapon==2 && combat.shots[i].charged && !combat.shots[i].variant) return i;
  return 8;
}
static void bubble_pop(MmxWeaponShot *s) {
  static const uint8_t normal[11]={0,0,13,11,12,13,11,12,13,11,12};
  static const uint8_t charged[7]={13,11,12,13,11,12,13};
  s->muzzle_pose=3;s->vx=s->vy=0;
  animation_start(s,s->charged ? charged[s->variant-1] : normal[s->variant]);
}
static void bubble_emit(uint8_t *r,MmxWeaponShot *s) {
  static const uint8_t sequence[7]={2,3,5,6,8,9,8};
  unsigned used=0;
  for (unsigned i=0;i<8;++i) if (combat.shots[i].active && combat.shots[i].page==1 &&
      combat.shots[i].weapon==2) {
    /* Normal bubbles occupy the same source identity bits until retired. */
    unsigned identity=combat.shots[i].charged ? combat.shots[i].variant : combat.shots[i].tether_pose;
    if (identity && identity<8) used|=1u<<identity;
  }
  unsigned id=1;while (id<8 && (used&(1u<<id))) ++id;
  if (id==8 || !free_slot(r)) return;
  if (!MmxWeaponsSpend(1,2,32)) { s->muzzle_pose=3;return; }
  MmxWeaponShot *t=spawn_child(r,s);if (!t) return;
  t->variant=(uint8_t)id;t->muzzle_pose=0;t->radius=0;t->tether_pose=2;
  t->origin_x=(int16_t)word(r+0xbad);t->origin_y=(int16_t)word(r+0xbb0);
  t->x=(t->origin_x+8)*256;t->y=(t->origin_y+16)*256;
  t->vx=bubble_route[0][0];t->vy=-bubble_route[0][1];
  t->muzzle_pose=0;t->radius=(uint8_t)(bubble_random(id)&63);
  animation_start(t,sequence[id-1]);
  unsigned d=0x1228+(unsigned)(t-combat.shots)*64;putword(r+d+0x20,0);
}
static void bubble_tick(uint8_t *r,unsigned d,MmxWeaponShot *s) {
  if (!s->age) {
    unsigned count=0,used=0;
    for (unsigned i=0;i<8;++i) if (combat.shots+i!=s && combat.shots[i].active &&
        combat.shots[i].page==1 && combat.shots[i].weapon==2) {
      if (!combat.shots[i].charged) { ++count;used|=1u<<combat.shots[i].tether_pose; }
      else if (!combat.shots[i].variant) { retire(r,d);return; }
    }
    if ((!s->charged && count>=7) || !MmxWeaponsSpend(1,2,32)) { retire(r,d);return; }
    s->age=1;s->born=combat.tick;muzzle_origin(r,d,s);
    if (s->charged) {
      s->variant=0;s->x=word(r+0xbad)*256;s->y=word(r+0xbb0)*256;
      animation_start(s,0);bubble_emit(r,s);
    } else {
      static const uint8_t sequences[16]={2,3,4,5,6,7,8,9,10,2,3,4,5,6,7,8};
      unsigned motion=word(r+0xbad)>word(r+0xbca) ? word(r+0xbad)-word(r+0xbca) : word(r+0xbca)-word(r+0xbad);
      if (motion>16) motion=0;
      unsigned speed=256+bubble_random(d)+motion*128;
      s->vx=(int16_t)(s->facing ? (int)speed : -(int)speed);s->vy=0;
      s->variant=sequences[bubble_random(d+1)&15];s->radius=(uint8_t)(6+(bubble_random(d+2)&15));
      s->tether_pose=1;while (s->tether_pose<7 && (used&(1u<<s->tether_pose))) ++s->tether_pose;
      animation_start(s,1);
    }
    native_object(r,d,s);if (s->charged) putword(r+d+0x20,0);return;
  }
  if (s->age==1 && s->born==combat.tick) return;
  if (s->charged && !s->variant) {
    if (s->muzzle_pose==3 || !(r[0xbcf]&127) || r[0xbaa]==12) { retire(r,d);return; }
    s->x=word(r+0xbad)*256;s->y=word(r+0xbb0)*256;
    if (terrain_water(r,s->x>>8,s->y>>8) && r[0xbaa]==6 &&
        !terrain_solid(r,s->x>>8,(s->y>>8)-(MmxZeroActive()?29:21),false,NULL))
      putword(r+0xbb0,word(r+0xbb0)-3);
    bubble_emit(r,s);native_object(r,d,s);putword(r+d+0x20,0);
    if (++s->age==0) s->age=2;return;
  }
  if (s->muzzle_pose==3) {
    if (s->flags&128) { retire(r,d);return; }
    animation_step(s);
  } else if (r[d+1]>=6 || (s->charged && bubble_manager()==8)) bubble_pop(s);
  else if (s->charged) {
    int px=word(r+0xbad),py=word(r+0xbb0);
    s->x+=(px-s->origin_x)*256;s->y+=(py-s->origin_y)*256;
    s->origin_x=(int16_t)px;s->origin_y=(int16_t)py;
    if (!s->muzzle_pose) {
      if (s->radius) --s->radius;
      else { s->muzzle_pose=1;s->radius=0; }
    } else {
      if (!--s->tether_pose) {
        if (++s->radius==29) { retire(r,d);return; }
        s->tether_pose=2;s->vx=bubble_route[s->radius][0];s->vy=-bubble_route[s->radius][1];
      }
      s->x+=s->vx;s->y+=s->vy;animation_step(s);
    }
  } else {
    if (!s->muzzle_pose) {
      if (s->flags&128) { s->muzzle_pose=1;animation_start(s,s->variant); }
      else animation_step(s);
    } else {
      s->vy-=terrain_water(r,s->x>>8,s->y>>8) ? 64 : 12;
      if (s->muzzle_pose==1) {
        if (s->flags&128) s->muzzle_pose=2;else animation_step(s);
      } else if (!--s->radius) bubble_pop(s);
    }
    s->x+=s->vx;s->y+=s->vy;
  }
  if (++s->age>400 || !s->active || (s->x>>8)<(int)word(r+0x1e4d)-96 ||
      (s->x>>8)>(int)word(r+0x1e4d)+352 || (s->y>>8)<(int)word(r+0x1e50)-128 ||
      (s->y>>8)>(int)word(r+0x1e50)+320) { retire(r,d);return; }
  native_object(r,d,s);
  if (s->muzzle_pose==3 || (s->charged && !s->muzzle_pose)) putword(r+d+0x20,0);
}
static void magnet_explode(MmxWeaponShot *s) {
  s->muzzle_pose=3;s->group=8;s->vx=s->vy=0;s->hit_slots=0;animation_start(s,5);
}
static bool magnet_projectiles(uint8_t *r,const MmxWeaponShot *s) {
  /* X2 $88:E67A pulls destructible enemy shots by 5/3 pixels, then
   * $88:D81F absorbs at most one overlapping shot on alternating ticks. */
  for (unsigned d=0x1428;d<0x1628;d+=64) if (r[d] && !(r[d]&64) && r[d+0x28]) {
    int x=word(r+d+5),y=word(r+d+8);
    putword(r+d+5,x+((x<(s->x>>8)) ? 5 : -5));
    putword(r+d+8,y+((y<(s->y>>8)) ? 3 : -3));
  }
  if (s->age&1) return false;
  int radius=8*(s->variant+1);
  for (unsigned d=0x1428;d<0x1628;d+=64) {
    unsigned box=word(r+d+0x20);
    if (!r[d] || (r[d]&64) || !r[d+0x28] || box<0x8000 || !collision_rom) continue;
    size_t offset=0x30000+(box&32767);if (offset+4>collision_rom_size) continue;
    const uint8_t *b=collision_rom+offset;
    int x=(int)word(r+d+5)+(int8_t)b[0]*((r[d+17]&64) ? -1 : 1)-(s->x>>8);
    int y=(int)word(r+d+8)+(int8_t)b[1]-(s->y>>8);
    if (x < -radius-b[2] || x > radius+b[2] || y < -radius-b[3] || y > radius+b[3]) continue;
    memset(r+d,0,4);putword(r+d+14,0);putword(r+d+0x2c,0);return true;
  }
  return false;
}
static void magnet_tick(uint8_t *r,unsigned d,MmxWeaponShot *s) {
  if (!s->age) {
    /* Source normal limit is one flying/arming mine. Once planted, +$35
     * is released while its native slot stays live until the explosion. */
    for (unsigned i=0;i<8;++i) if (combat.shots+i!=s && combat.shots[i].active &&
        combat.shots[i].page==1 && combat.shots[i].weapon==7 &&
        (combat.shots[i].charged || combat.shots[i].muzzle_pose<2)) { retire(r,d);return; }
    if (!MmxWeaponsSpend(1,7,s->charged ? 768 : 256)) { retire(r,d);return; }
    muzzle_origin(r,d,s);s->age=1;s->born=combat.tick;s->vy=0;
    s->vx=(int16_t)((s->facing ? 1 : -1)*(s->charged ? 128 : 512));
    animation_start(s,s->charged ? 0 : 1);native_object(r,d,s);return;
  }
  if (s->charged) {
    if (magnet_projectiles(r,s) && s->radius<32) {
      ++s->radius;
      if (!(s->radius&15)) { ++s->variant;animation_start(s,s->variant); }
    }
  } else if (s->muzzle_pose!=3 && r[d+1]>=6) magnet_explode(s);
  if (s->charged || !s->muzzle_pose) {
    /* $88:E6C4 steering has inertia: releasing up/down retains VY. */
    if (r[0xbdf]&8) { s->vy-=128;if (s->vy < -1024) s->vy=-1024; }
    if (r[0xbdf]&4) { s->vy+=128;if (s->vy > 1024) s->vy=1024; }
    unsigned hit=0;
    if (s->charged) { s->x+=s->vx;s->y+=s->vy; }
    else {
      hit=terrain_move(r,s,4,4);
      for (unsigned i=0;i<8 && !hit;++i) {
        const MmxWeaponShot *t=combat.shots+i;
        if (t==s || !t->active || t->page!=1 || t->weapon!=7 || t->charged) continue;
        int dx=(t->x-s->x)>>8,dy=(t->y-s->y)>>8;
        if (dx>=-8 && dx<=8 && dy>=-8 && dy<=8) hit=1;
      }
    }
    if (hit) { s->muzzle_pose=1;s->vx=s->vy=0;animation_start(s,2); }
    else animation_step(s);
  } else if (s->muzzle_pose==1) {
    if (s->flags&128) { s->muzzle_pose=2;s->radius=60;animation_start(s,3); }
    else animation_step(s);
  } else if (s->muzzle_pose==2) {
    if (!--s->radius) magnet_explode(s);
  } else {
    if (s->flags&128) {
      /* Source $82:AD44 detonates intersecting mines at the end of the
       * blast, preserving the visible delay in a chain reaction. */
      for (unsigned i=0;i<8;++i) {
        MmxWeaponShot *t=combat.shots+i;
        if (t==s || !t->active || t->page!=1 || t->weapon!=7 || t->charged || t->muzzle_pose==3) continue;
        int dx=(t->x-s->x)>>8,dy=((t->y-s->y)>>8)+8;
        if (dx>=-21 && dx<=21 && dy>=-22 && dy<=22) magnet_explode(t);
      }
      retire(r,d);return;
    }
    animation_step(s);
  }
  if (++s->age>1600 || !s->active || (s->x>>8)<(int)word(r+0x1e4d)-32 ||
      (s->x>>8)>=(int)word(r+0x1e4d)+288 || (s->y>>8)<(int)word(r+0x1e50)-16 ||
      (s->y>>8)>=(int)word(r+0x1e50)+240) { retire(r,d);return; }
  if (!(s->age&1) && (s->charged || s->muzzle_pose==3)) s->hit_slots=0;
  native_object(r,d,s);
}
static void speed_spark(const MmxWeaponShot *parent) {
  for (unsigned i=0;i<16;++i) if (!combat.effects[i].active) {
    MmxWeaponShot *t=combat.effects+i;*t=*parent;t->variant=4;t->age=1;t->charged=0;
    t->group=37;t->muzzle_pose=0;t->tether_pose=bubble_random(i+2)&1;t->born=combat.tick;
    t->radius=4+(bubble_random(i+3)&7);t->vx=(int16_t)bubble_random(i+4);
    if (parent->facing) t->vx=-t->vx;
    t->vy=0;t->y+=((int)(bubble_random(i+5)&31)-16)*256;
    animation_start(t,3+(bubble_random(i+6)&1));return;
  }
}
static void speed_water(uint8_t *r,MmxWeaponShot *s) {
  s->variant=2;s->muzzle_pose=0;animation_start(s,5);
  for (unsigned i=0;i<2;++i) {
    MmxWeaponShot *t=spawn_child(r,s);if (!t) break;
    t->variant=3;t->radius=(uint8_t)(i*4);t->tether_pose=(uint8_t)i;
    t->origin_x=(int16_t)(s-combat.shots);t->origin_y=(int16_t)(s->x>>8);
    t->y+=(i ? -3 : 3)*256;t->muzzle_pose=0;animation_start(t,5);
    putword(r+0x1228+(unsigned)(t-combat.shots)*64+0x20,0);
  }
}
static void speed_tick(uint8_t *r,unsigned d,MmxWeaponShot *s) {
  if (!s->age) {
    if (!MmxWeaponsSpend(1,8,(s->charged ? 3 : 1)*256)) { retire(r,d);return; }
    muzzle_origin(r,d,s);s->born=combat.tick;s->age=1;
    s->vx=s->facing ? 1280 : -1280;s->vy=0;
    if (s->charged) {
      s->variant=!(r[0xbd3]&4);s->radius=s->variant ? 25 : 49;
      s->x=word(r+0xbad)*256;s->y=word(r+0xbb0)*256;s->origin_y=(int16_t)word(r+0xbb0);
      r[0xbaa]=0x14;r[0xbab]=0;r[0xbf8]=255;r[0xc17]=0;
      s->tether_pose=terrain_water(r,s->x>>8,s->y>>8);
      r[0xbd8]=s->tether_pose ? 0 : 128;stop_charge(r);
      animation_start(s,0);
    } else {
      animation_start(s,6);
      if (terrain_water(r,s->x>>8,s->y>>8)) speed_water(r,s);
    }
    native_object(r,d,s);if (s->charged && s->tether_pose) putword(r+d+0x20,0);return;
  }
  if (s->age==1 && s->born==combat.tick) return;
  if (s->charged) {
    if (r[0xbaa]!=0x14 || r[0x1f0c] || !--s->radius || !(r[0xbcf]&127)) { retire(r,d);return; }
    if (s->variant) { putword(r+0xbb0,s->origin_y);r[0xbaf]=0;r[0xc06]&=(uint8_t)~4; }
    s->x=word(r+0xbad)*256;s->y=word(r+0xbb0)*256;
    s->tether_pose=terrain_water(r,s->x>>8,s->y>>8);
    r[0xbd8]=s->tether_pose ? 0 : 128;animation_step(s);
    if (!(s->age&1)) s->hit_slots=0;
  } else if (s->variant==3) {
    /* X2 $86:B5A7: two opposite eight-record orbits, two ticks each.
     * These water bubbles are visual children; class $23 parent hits. */
    static const int16_t orbit[8][2]={{128,-128},{128,-512},{-128,-512},{-128,-128},
        {-128,128},{-128,512},{128,512},{128,128}};
    unsigned parent=(unsigned)s->origin_x;
    if (parent>=8 || !combat.shots[parent].active || combat.shots[parent].variant!=2) { retire(r,d);return; }
    const MmxWeaponShot *p=combat.shots+parent;
    if (!(s->age&1)) s->radius=(s->radius+1)&7;
    s->x+=p->x-s->origin_y*256+orbit[s->radius][0];s->y-=orbit[s->radius][1];
    s->origin_y=(int16_t)(p->x>>8);
  } else if (s->muzzle_pose==3) {
    if (s->flags&128) { retire(r,d);return; }animation_step(s);
    if (s->variant==1) s->x+=s->vx;
  } else if (r[d+1]>=6) {
    if (s->variant) { retire(r,d);return; }
    s->muzzle_pose=3;s->vx=s->vy=0;animation_start(s,8);
  } else if (s->variant==1) {
    if (s->muzzle_pose==0) {
      unsigned hit=terrain_move(r,s,0,0);
      if (hit&4) { s->muzzle_pose=1;s->radius=60;s->vx=s->facing ? 512 : -512; }
      else if (!--s->radius) { retire(r,d);return; }
    } else {
      unsigned hit=terrain_move(r,s,3,0);
      if (!--s->radius || !(hit&4) || (hit&1)) { s->muzzle_pose=3;animation_start(s,2); }
      else animation_step(s);
    }
  } else {
    if (!s->variant) {
      if (terrain_water(r,s->x>>8,s->y>>8)) speed_water(r,s);
      else {
        if (!s->muzzle_pose && (s->flags&128)) { s->muzzle_pose=1;animation_start(s,7); }
        else animation_step(s);
        if (s->muzzle_pose && !(combat.tick&3)) speed_spark(s);
        if (!(combat.tick&3) && terrain_solid(r,s->x>>8,(s->y>>8)+24,true,NULL)) {
          MmxWeaponShot *t=spawn_child(r,s);
          if (t) {
            t->variant=1;t->muzzle_pose=0;t->radius=60;t->vx=0;t->vy=1024;
            animation_start(t,1);putword(r+0x1228+(unsigned)(t-combat.shots)*64+0x20,0);
          }
        }
      }
    }
    s->x+=s->vx; /* Source fire and water shot pass through solid terrain. */
  }
  if (++s->age>240 || !s->active || (!s->charged && ((s->x>>8)<(int)word(r+0x1e4d)-32 ||
      (s->x>>8)>=(int)word(r+0x1e4d)+288 || (s->y>>8)<(int)word(r+0x1e50)-16 ||
      (s->y>>8)>=(int)word(r+0x1e50)+240))) { retire(r,d);return; }
  native_object(r,d,s);
  if (s->charged ? s->tether_pose : s->variant==3 || s->muzzle_pose==3 || (s->variant==1 && !s->muzzle_pose))
    putword(r+d+0x20,0);
}
static void frost_shards(uint8_t *r,const MmxWeaponShot *parent) {
  /* X3 $81:A93A/$BD9E selects four class-$37 particles. Its shared
   * initializer $82:FD66 uses these $86:DAE6/$DAF6 velocity rows and gravity
   * 48. Sample the source rows deterministically from the saved frame/slot.
   * Cosmetic particles share X1's finite projectile pool. */
  static const int16_t vx[8]={-512,-256,128,384,-640,-384,256,512};
  static const int16_t vy[8]={768,1024,1408,640,896,1152,1280,512};
  for (unsigned i=0;i<4;++i) {
    MmxWeaponShot *t=spawn_child(r,parent);if (!t) break;
    unsigned index=(combat.tick+i*3+(unsigned)(parent->x>>8))&7;
    t->variant=2;t->muzzle_pose=7;t->origin_x=0;t->radius=0;t->tether_pose=0;
    t->vx=vx[index];t->vy=-vy[(index+i*2)&7];
    animation_start(t,parent->charged ? (parent->variant ? 17 : 14) : 4);
    putword(r+0x1228+(unsigned)(t-combat.shots)*64+0x20,0);
  }
}
static void frost_break(uint8_t *r,MmxWeaponShot *s,bool shards) {
  if (shards) frost_shards(r,s);
  s->muzzle_pose=s->charged ? 4 : 7;s->vx=s->vy=0;s->origin_x=2;
}
static void frost_impact(MmxWeaponShot *s) {
  s->muzzle_pose=2;s->vx=(int16_t)((-s->vx)>>2);
  s->vy=s->variant ? -512 : -768;s->hit_slots=0;
  animation_start(s,5);
}
static void frost_block(uint8_t *r,const MmxWeaponShot *s) {
  /* X3 $84:CC0E clears a colliding, destructible enemy projectile. X1's
   * pool is $1428..1627; its native damage category and hitbox remain the
   * authority. Zero/immune categories are not made destructible. */
  for (unsigned d=0x1428;d<0x1628;d+=64) {
    unsigned box=word(r+d+0x20),type=r[d+0x28];
    if (!r[d] || (r[d]&64) || !type || box<0x8000 || !collision_rom) continue;
    size_t offset=0x30000+(box&0x7fff);
    if (offset+4>collision_rom_size) continue;
    const uint8_t *b=collision_rom+offset;
    int x=(int)word(r+d+5)+(int8_t)b[0]*((r[d+17]&64) ? -1 : 1)-
      ((s->x>>8)+(s->facing ? 3 : -3));
    int y=(int)word(r+d+8)+(int8_t)b[1]-(s->y>>8);
    if (x < -10-(int)b[2] || x > 10+(int)b[2] || y < -17-(int)b[3] || y > 17+(int)b[3]) continue;
    memset(r+d,0,4);putword(r+d+14,0);putword(r+d+0x2c,0);return;
  }
}
static void frost_tick(uint8_t *r,unsigned d,MmxWeaponShot *s) {
  /* X3 $81:A6DF / $81:BAFC, group $10. Normal phases: form, rocket,
   * falling core, landed core, spike growth, planted spike, failed form,
   * shatter. Charged phases: form, shield growth, shield, released chunk,
   * shatter, rising water platform, floating platform. origin_x holds phase
   * lifetime; radius is the steering clock. */
  if (!s->age) {
    for (unsigned i=0;i<8;++i) if (combat.shots+i!=s && combat.shots[i].active &&
        combat.shots[i].page==2 && combat.shots[i].weapon==7 &&
        combat.shots[i].charged==s->charged) { retire(r,d);return; }
    if (!MmxWeaponsSpend(2,7,s->charged ? 0x300 : 0x100)) { retire(r,d);return; }
    muzzle_origin(r,d,s);s->born=combat.tick;s->age=1;
    s->variant=(uint8_t)terrain_water(r,s->x>>8,s->y>>8);
    if (s->charged) {
      s->vx=(int16_t)(((s->x>>8)-(int)word(r+0xbad))*(s->facing ? 1 : -1));
      s->origin_y=(int16_t)((s->y>>8)-(int)word(r+0xbb0));
    } else s->vx=s->facing ? 16 : -16;
    s->vy=0;s->origin_x=s->charged ? 360 : 0;
    animation_start(s,s->charged ? 10 : s->variant*2);
    native_object(r,d,s);return;
  }
  if (s->age==1 && s->born==combat.tick) return;
  unsigned phase=s->muzzle_pose;
  if (s->variant==2) {
    s->vy+=48;s->x+=s->vx;s->y+=s->vy;
    if (++s->age>180 || (s->y>>8)>(int)word(r+0x1e50)+256 ||
        (s->x>>8)<(int)word(r+0x1e4d)-64 || (s->x>>8)>(int)word(r+0x1e4d)+320) { retire(r,d);return; }
    native_object(r,d,s);putword(r+d+0x20,0);return;
  } else if (phase==(s->charged ? 4u : 7u)) {
    if (!s->origin_x || !--s->origin_x) { retire(r,d);return; }
  } else if (s->charged && r[d+1]>=8) {
    /* Source state 8 shatters on a surviving enemy; state 6 retains the
     * shield after killing one. These are deliberately different. */
    frost_break(r,s,false);
  } else if (s->charged && phase>=5) {
    int32_t previous_y=s->y;
    if (!terrain_water(r,s->x>>8,s->y>>8)) frost_break(r,s,true);
    else if (phase==5) {
      if (!terrain_water(r,s->x>>8,(s->y>>8)-16)) {
        s->muzzle_pose=6;s->origin_x=240;s->radius=0;s->vy=0;
      } else {
        s->vy-=240;if (s->vy < -256) s->vy=-256;
        unsigned hit=terrain_move(r,s,16,24);
        if (hit&8) frost_break(r,s,true);
      }
      if (!(s->flags&128)) animation_step(s);
    } else if (!--s->origin_x) frost_break(r,s,true);
    else {
      static const int16_t bob[4]={0,32,0,-32};
      s->vy=bob[((s->age>>5)&3)];s->y+=s->vy;
    }
    if (s->tether_pose && s->muzzle_pose>=5) {
      int32_t target=(int32_t)word(r+0xbb0)*256+s->y-previous_y;
      if (!terrain_solid(r,word(r+0xbad),(target>>8)-(MmxZeroActive()?26:18),false,NULL))
        putword(r+0xbb0,(unsigned)(target>>8));
      else frost_break(r,s,true);
    }
  } else if (s->charged) {
    if (phase<3) {
      if (r[0xbaa]==14 || r[0xbaa]==12 || r[0x1f0c] || r[0xbaa]>=0x18) frost_break(r,s,phase!=0);
      else {
        s->facing=r[0xbb9]&64;
        int dx=(int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],0,(uint8_t)s->vx);
        int dy=(int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],1,(uint8_t)s->origin_y);
        s->x=((int)word(r+0xbad)+(s->facing ? dx : -dx))*256;
        s->y=((int)word(r+0xbb0)+dy)*256;
        if (phase<2) {
          if (s->flags&128) {
            ++s->muzzle_pose;
            if (!phase) {
              s->variant=(uint8_t)terrain_water(r,s->x>>8,s->y>>8);
              if (s->variant) { s->muzzle_pose=5;s->vx=s->vy=0;s->tether_pose=0;animation_start(s,13); }
              else animation_start(s,12);
            }
          } else animation_step(s);
        } else if (!--s->origin_x) {
          /* $81:BC07..BC24 releases class $21 after 360 shield ticks. */
          frost_shards(r,s);
          s->muzzle_pose=3;s->origin_x=180;s->vx=s->facing ? 256 : -256;
          s->vy=-768;s->hit_slots=0;animation_start(s,15);
        }
        if (s->muzzle_pose==2) frost_block(r,s);
      }
    } else {
      if (r[d+1]>=6 || !--s->origin_x) frost_break(r,s,true);
      else {
        s->vy+=32;if (s->vy>2048) s->vy=2048;
        unsigned hit=terrain_move(r,s,11,7);
        if (hit&1) frost_break(r,s,true);
        else if (hit&4) { s->vy=0;if (!(s->age&7)) s->vx=s->facing ? 768 : -768; }
        else if (hit&8) s->vy=0;
      }
    }
  } else if (r[d+1]>=6 && phase<6) {
    if (r[d+1]==6 && phase<2) { frost_shards(r,s);frost_impact(s); }
    else frost_break(r,s,r[d+1]==6);
  } else if (!phase) {
    unsigned hit=terrain_move(r,s,5,7);
    if ((hit&1) || terrain_solid(r,s->x>>8,s->y>>8,false,NULL)) {
      s->muzzle_pose=6;s->origin_x=120;s->vx=s->facing ? 512 : -512;
      animation_start(s,18+s->variant);
    } else if (s->flags&128) { s->muzzle_pose=1;animation_start(s,1+s->variant*2); }
    else animation_step(s);
  } else if (phase==1) {
    unsigned accel=s->variant ? 8 : 16,maximum=s->variant ? 768 : 1024;
    int speed=(s->facing ? s->vx : -s->vx)+(int)accel;
    if (speed>(int)maximum) speed=(int)maximum;
    s->vx=(int16_t)(s->facing ? speed : -speed);s->vy=0;
    /* Source $A7BF clears VY between single-pixel steering pulses. */
    if (++s->radius==8) {
      s->radius=0;s->vy=(r[0xbdf]&8) ? -256 : (r[0xbdf]&4) ? 256 : 0;
    }
    unsigned hit=terrain_move(r,s,5,7);
    if (hit&1) { frost_shards(r,s);frost_impact(s); } else animation_step(s);
  } else if (phase==2) {
    s->vy+=s->variant ? 24 : 32;
    if (s->vy>(s->variant ? 1024 : 1536)) s->vy=s->variant ? 1024 : 1536;
    unsigned hit=terrain_move(r,s,5,7);
    if (hit&4) { s->muzzle_pose=3;s->vx=s->vy=0; }
    else if (hit&8) s->vy=0;
  } else if (phase<6) {
    if (!terrain_solid(r,s->x>>8,(s->y>>8)+8,true,NULL)) frost_break(r,s,true);
    else if (phase==5) { if (!--s->origin_x) frost_break(r,s,true); }
    else if (s->flags&128) {
      ++s->muzzle_pose;
      if (phase==3) animation_start(s,6+s->variant*2);
      else { s->origin_x=240;animation_start(s,7+s->variant*2); }
    } else animation_step(s);
  } else {
    if (!--s->origin_x) { retire(r,d);return; }
    s->vx=-s->vx;s->x+=s->vx;animation_step(s);
  }
  if (++s->age>720 || !s->active || s->x/256<(int)word(r+0x1e4d)-96 ||
      s->x/256>(int)word(r+0x1e4d)+352 || s->y/256<(int)word(r+0x1e50)-160 ||
      s->y/256>(int)word(r+0x1e50)+320) { retire(r,d);return; }
  native_object(r,d,s);
  if ((s->charged && s->muzzle_pose==4) || (!s->charged && s->muzzle_pose>=6)) putword(r+d+0x20,0);
}
static unsigned wheel_spin_sequence(const MmxWeaponShot *s) {
  unsigned speed=s->origin_x>0 ? (unsigned)s->origin_x>>8 : 0;
  return 4-(speed>7 ? 7 : speed)/2; /* X2 $86:B76A. */
}
static void wheel_split(uint8_t *r,unsigned d,MmxWeaponShot *s) {
  /* X2 uses a visual center plus eight projectiles. Reuse the center's
   * native slot for direction 0; keep its brief flash as saved render state. */
  static const int8_t dx[8]={0,6,8,6,0,-6,-8,-6},dy[8]={8,6,0,-6,-8,-6,0,6};
  static const int16_t vx[8]={0,724,1024,724,0,-724,-1024,-724};
  static const int16_t vy[8]={1024,724,0,-724,-1024,-724,0,724};
  MmxWeaponShot center=*s;
  for (unsigned i=0;i<8;++i) {
    MmxWeaponShot *t=i ? spawn_child(r,&center) : s;
    if (!t) break;
    t->variant=(uint8_t)i;t->muzzle_pose=1;t->radius=i ? 0 : 6;
    t->tether_pose=(uint8_t)(center.facing!=0);
    t->origin_x=(int16_t)(center.x>>8);t->origin_y=(int16_t)(center.y>>8);
    t->x=center.x+dx[i]*256;t->y=center.y+dy[i]*256;
    t->vx=vx[i];t->vy=vy[i];t->facing=0;
    t->age=1;t->born=combat.tick;t->hit_slots=0;
    animation_start(t,12+i);
    native_object(r,i ? 0x1228+(unsigned)(t-combat.shots)*64 : d,t);
  }
}
static int wheel_traction(const uint8_t *r,const MmxWeaponShot *s) {
  unsigned type=terrain_type(r,s->x>>8,(s->y>>8)+9);
  bool right=s->facing!=0;
  if (type==1 || type==2) return right ? -20 : 16;
  if (type==3 || type==4) return right ? 16 : -20;
  if (type>=5 && type<=8) return right ? -16 : 0;
  if (type>=9 && type<=12) return right ? 0 : -16;
  return -4;
}
static void wheel_tick(uint8_t *r,unsigned d,MmxWeaponShot *s) {
  /* Normal phases: form, fall, ground delay, roll, airborne roll, wall
   * delay, shrink. origin_x is momentum; origin_y advances the ground FX;
   * tether_pose is the native ten-frame enemy-contact pause. */
  if (!s->age) {
    for (unsigned i=0;i<8;++i) if (combat.shots+i!=s && combat.shots[i].active &&
        combat.shots[i].page==1 && combat.shots[i].weapon==4 &&
        combat.shots[i].charged==s->charged) { retire(r,d); return; }
    if (!MmxWeaponsSpend(1,4,s->charged ? 0x300 : 0x100)) { retire(r,d); return; }
    muzzle_origin(r,d,s);s->age=1;s->born=combat.tick;
    s->vx=s->vy=0;
    if (!s->charged) { s->origin_x=1024;s->origin_y=0; }
    animation_start(s,s->charged ? 11 : 0);native_object(r,d,s);return;
  }
  if (s->age==1 && s->born==combat.tick) return;
  if (s->charged) {
    if (!s->muzzle_pose) {
      if (s->flags&1) { wheel_split(r,d,s); return; }
      animation_step(s);
    } else {
      if (r[d+1]>=8) { retire(r,d); return; }
      s->x+=s->vx;s->y+=s->vy;
      if (s->radius) --s->radius;
    }
  } else {
    unsigned previous_speed=wheel_spin_sequence(s);
    if (s->muzzle_pose && s->muzzle_pose!=6 && !s->tether_pose && r[d+1]>=8)
      s->tether_pose=10;
    if (s->tether_pose) {
      s->origin_x-=8;
      if (!--s->tether_pose) s->hit_slots=0;
    } else if (!s->muzzle_pose) {
      if (s->flags&128) { s->muzzle_pose=1;animation_start(s,1); }
    } else if (s->muzzle_pose==2 || s->muzzle_pose==5) {
      ++s->origin_y;
      if (s->muzzle_pose==5) s->origin_x-=8;
      if (s->radius && !--s->radius) {
        if (s->muzzle_pose==5) { s->vy=-s->origin_x;s->muzzle_pose=4; }
        else s->muzzle_pose=3;
      }
    } else {
      unsigned phase=s->muzzle_pose;
      if (phase==6 && (s->flags&128)) { retire(r,d); return; }
      if (phase==3) {
        s->origin_x=(int16_t)(s->origin_x+wheel_traction(r,s));
        if (s->origin_x>1024) s->origin_x=1024;
        ++s->origin_y;
      }
      s->vx=(phase==3 || phase==4) ? (int16_t)(s->origin_x*(s->facing ? 1 : -1)) : 0;
      s->vy+=64;if (s->vy>1024) s->vy=1024;
      unsigned hit=terrain_move(r,s,8,8);
      if (hit&4) {
        s->vy=0;
        if (phase==1) { s->muzzle_pose=2;s->radius=30; }
        else if (phase==4) s->muzzle_pose=3;
      } else if (phase==3) s->muzzle_pose=4;
      if (hit&8) s->vy=0;
      if (hit&1) {
        if (phase==3) { s->muzzle_pose=5;s->radius=30;s->vx=s->vy=0; }
        else if (phase==4) { s->facing^=64;s->origin_x-=16; }
      } else if (phase==4 && !(hit&4)) s->origin_x-=4;
    }
    if (s->origin_x<0 && s->muzzle_pose!=6) {
      s->muzzle_pose=6;s->vx=0;s->tether_pose=0;animation_start(s,10);
    } else if (s->muzzle_pose>=2 && s->muzzle_pose!=6 && previous_speed!=wheel_spin_sequence(s))
      animation_start(s,wheel_spin_sequence(s));
    animation_step(s);
  }
  if (++s->age>600 || !s->active || s->x/256<(int)word(r+0x1e4d)-96 ||
      s->x/256>(int)word(r+0x1e4d)+352 || s->y/256<(int)word(r+0x1e50)-160 ||
      s->y/256>(int)word(r+0x1e50)+288) { retire(r,d); return; }
  native_object(r,d,s);
  if ((!s->charged && (s->muzzle_pose==6 || s->tether_pose)) ||
      (s->charged && !s->muzzle_pose)) putword(r+d+0x20,0);
}
static void sonic_tick(uint8_t *r, unsigned d, MmxWeaponShot *s) {
  /* X2 $81:9622/$A5D2 launch from the final forming-animation flag.
   * Variant selects the source velocity row; muzzle_pose is the phase:
   * 0 forming, 1 flight, 2 charged descent, 3 enemy impact. */
  static const int16_t vx[5] = {0,400,-400,787,-787};
  static const int16_t vy[5] = {-2304,-2269,-2269,-2165,-2165};
  static const uint8_t gravity[5] = {80,78,78,75,75};
  if (!s->age) {
    for (unsigned i=0;i<8;++i) if (combat.shots+i != s && combat.shots[i].active &&
        combat.shots[i].page == 1 && combat.shots[i].weapon == 5) { retire(r,d); return; }
    if (!MmxWeaponsSpend(1,5,s->charged ? 0x200 : 0x80)) { retire(r,d); return; }
    muzzle_origin(r,d,s); s->age = 1; s->born = combat.tick;
    s->vx = s->charged ? 0 : (s->facing ? 768 : -768);
    s->vy = s->charged ? -2304 : 128;
    animation_start(s,0); native_object(r,d,s); return;
  }
  if (s->age == 1 && s->born == combat.tick) return;
  if (s->muzzle_pose != 3 && r[d+1] >= 8) {
    s->muzzle_pose = 3; s->vx = s->vy = 0;
    animation_start(s,s->charged ? 3 : 4);
  }
  if (s->muzzle_pose == 3) {
    if (s->flags & 128) { retire(r,d); return; }
    animation_step(s);
  } else if (!s->muzzle_pose) {
    if (s->flags & 128) {
      s->muzzle_pose = 1; animation_start(s,1);
      for (unsigned i=1;i<(s->charged ? 5u : 2u);++i) {
        MmxWeaponShot *t = spawn_child(r,s); if (!t) break;
        t->variant = (uint8_t)i;
        t->vx = s->charged ? vx[i] : (s->facing ? 896 : -896);
        t->vy = s->charged ? vy[i] : 128;
      }
    } else animation_step(s);
  } else if (s->charged) {
    s->vy += s->muzzle_pose == 2 ? 96 : gravity[s->variant];
    s->x += s->vx; s->y += s->vy;
    if (s->muzzle_pose == 1) {
      animation_step(s);
      if (s->vy > 0) { s->muzzle_pose = 2; s->vx = 0; animation_start(s,2); }
    } else {
      /* Native $A647 clamps the high byte after movement. */
      int original = -s->vy;
      if (original < -2048) s->vy = (int16_t)(2048 - (original & 255));
    }
  } else {
    s->vy -= s->variant ? 8 : 4;
    unsigned hit = terrain_move(r,s,12,8);
    if (hit & 1) { s->vx = -s->vx; s->facing ^= 64; }
    if (hit & 12) {
      if (++s->radius >= 3) { retire(r,d); return; }
      s->vy = (int16_t)((-s->vy) >> 1);
    }
    int original = -s->vy;
    if (original >= 1024) s->vy = (int16_t)-(768 + (original & 255));
    animation_step(s);
  }
  if (++s->age > 360 || s->x/256 < (int)word(r+0x1e4d)-96 ||
      s->x/256 > (int)word(r+0x1e4d)+352 || s->y/256 < (int)word(r+0x1e50)-160 ||
      s->y/256 > (int)word(r+0x1e50)+288 || !s->active) { retire(r,d); return; }
  native_object(r,d,s);
  if (s->muzzle_pose == 3) putword(r+d+0x20,0);
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
static int ray_vector(unsigned angle) {
  /* Original X3 $06:E18E 32-direction vectors, quarter-wave symmetry. */
  static const int16_t quarter[9] = {0,124,228,307,361,409,457,496,512};
  angle &= 31; int sign = angle >= 16 ? -1 : 1; angle &= 15;
  return sign * quarter[angle > 8 ? 16-angle : angle] * 8;
}
static void ray_emit(uint8_t *r, MmxWeaponShot *s) {
  static const uint8_t forward[8] = {7,9,8,7,9,6,10,0};
  static const uint8_t radial[16] = {2,20,12,24,8,10,18,30,28,14,4,16,0,26,6,22};
  MmxWeaponShot *t = spawn_child(r,s); if (!t) return;
  unsigned angle;
  if (s->charged) { s->radius = (s->radius+1)&15; angle = radial[s->radius]; }
  else angle = forward[(s->radius++)&7] + (s->facing ? 0 : 16);
  t->variant = 2; t->vx = (int16_t)ray_vector(angle); t->vy = (int16_t)-ray_vector(angle+8);
  t->radius = 0; t->origin_x = (int16_t)(t->x>>8); t->origin_y = (int16_t)(t->y>>8);
  animation_start(t,3);
}
static void ray_tick(uint8_t *r, unsigned d, MmxWeaponShot *s) {
  /* Variants 0 muzzle/deploy/rise, 1 deployed turret, 2 ray, 3 draining trail.
   * Turret phase age is stored in origin_x; radius is the emission direction. */
  if (!s->age) {
    for (unsigned i=0;i<8;++i) if (combat.shots+i != s && combat.shots[i].active &&
        combat.shots[i].page == 2 && combat.shots[i].weapon == 5 &&
        combat.shots[i].variant < 2 && combat.shots[i].charged == s->charged) { retire(r,d); return; }
    if (!MmxWeaponsSpend(2,5,s->charged ? 0x280 : 0x100)) { retire(r,d); return; }
    s->age = 1; s->born = combat.tick;
    muzzle_origin(r,d,s); animation_start(s,s->charged ? 0 : 10);
    if (s->charged) {
      s->x = ((int)word(r+0xbad) + (s->facing ? -2 : 2))*256;
      s->y = ((int)word(r+0xbb0) - (MmxZeroActive() ? 52 : 44))*256;
      s->vy = -768; s->origin_x = 0;
    } else {
      s->origin_x = (int16_t)(((s->x>>8)-(int)word(r+0xbad))*(s->facing ? 1 : -1));
      s->origin_y = (int16_t)((s->y>>8)-(int)word(r+0xbb0));
    }
    native_object(r,d,s); putword(r+d+0x20,0); return;
  }
  if (s->age == 1 && s->born == combat.tick) return;
  if (s->variant == 3) {
    if (++s->radius >= 3) { retire(r,d); return; }
  } else if (s->variant == 2) {
    if (r[d+1] >= 8) { s->variant = 3; s->radius = 0; }
    else { s->x += s->vx; s->y += s->vy; }
  } else if (!s->charged) {
    if (s->age >= 60 || r[0xbaa] == 14 || r[0xbaa] == 12 || r[0x1f0c]) { retire(r,d); return; }
    s->facing = r[0xbb9] & 64;
    /* Keep the firing origin after the brief native firing overlay finishes;
     * movement-specific Zero poses can still supply updated arm coordinates. */
    int dx = (int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],0,(uint8_t)s->origin_x);
    int dy = (int8_t)MmxZeroMuzzle(r,d,r[d+0x3c],1,(uint8_t)s->origin_y);
    s->x = ((int)word(r+0xbad)+(s->facing ? dx : -dx))*256;
    s->y = ((int)word(r+0xbb0)+dy)*256;
    if (!(s->age & 7)) ray_emit(r,s);
    animation_step(s);
  } else if (!s->variant) {
    if (s->age >= 36) {
      s->vy += 32;
      if (s->vy >= 0) {
        s->vy = 0; s->variant = 1; s->origin_x = 0; animation_start(s,1);
      } else { s->y += s->vy; animation_step(s); }
    }
  } else {
    if (++s->origin_x >= 180 || r[d+1] >= 8) { retire(r,d); return; }
    if (!(s->origin_x & 7)) ray_emit(r,s);
    animation_step(s);
  }
  if (++s->age > 300 || s->x/256 < (int)word(r+0x1e4d)-96 ||
      s->x/256 > (int)word(r+0x1e4d)+352 || s->y/256 < (int)word(r+0x1e50)-160 ||
      s->y/256 > (int)word(r+0x1e50)+320 || !s->active) { retire(r,d); return; }
  native_object(r,d,s);
  if (s->variant == 3 || (s->variant < 2 && (!s->charged || !s->variant))) putword(r+d+0x20,0);
}
#include "mmx_weapon_fang.inc"
unsigned MmxWeaponsProjectileTick(uint8_t r[0x20000], unsigned d, unsigned active) {
  if (!owned(r,d)) {
    if (slot_valid(d)) memset(combat.shots+slot_index(d),0,sizeof(MmxWeaponShot));
    return active;
  }
  MmxWeaponShot *s = combat.shots + slot_index(d);
  if (!MmxWeaponsEnabled() || !s->active) { retire(r,d); return 0; }
  if (s->page == 1 && s->weapon == 2) { bubble_tick(r,d,s); return 0; }
  if (s->page == 1 && s->weapon == 7) { magnet_tick(r,d,s); return 0; }
  if (s->page == 1 && s->weapon == 8) { speed_tick(r,d,s); return 0; }
  if (s->page == 1 && s->weapon == 4) { wheel_tick(r,d,s); return 0; }
  if (s->page == 1 && s->weapon == 5) { sonic_tick(r,d,s); return 0; }
  if (s->page == 2 && s->weapon == 1) { acid_tick(r,d,s); return 0; }
  if (s->page == 2 && s->weapon == 5) { ray_tick(r,d,s); return 0; }
  if (s->page == 2 && s->weapon == 7) { frost_tick(r,d,s); return 0; }
  if (s->page == 2 && s->weapon == 8) { fang_tick(r,d,s); return 0; }
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
static unsigned source_damage(const MmxWeaponShot *s) {
  /* Neutral ordinary-enemy rows: X2 $86:F4C8, X3 $86:E55D.
   * Both use buster=3. Classes are determined by the projectile phase,
   * not by the menu ID (Ray children are $1C, Acid droplets are $18).
   * Special-response weapons are added with their own behavior handlers. */
  if (s->page==1) {
    if (s->weapon==2) return s->charged ? 5 : 2;
    if (s->weapon==7) return 5;
    if (s->weapon==8) return s->charged || s->variant>=2 ? 1 : 5;
    if (s->weapon==4) return s->charged ? 50 : 25;
    if (s->weapon==5) return s->charged ? 1 : 4;
  } else if (s->page==2) {
    if (s->weapon==1) return s->variant==2 ? 5 : 9;
    if (s->weapon==4) return s->charged ? 30 : 9;
    if (s->weapon==5) return s->variant>=2 ? 5 : 9;
    if (s->weapon==7) return s->charged && s->muzzle_pose==3 ? 9 : 15;
    if (s->weapon==8) return s->charged ? 6 : 2;
  }
  return 3;
}
unsigned MmxWeaponsDamage(uint8_t r[0x20000], unsigned enemy, unsigned d, unsigned original) {
  if (!owned(r,d) || !original || (original & 128)) return original;
  MmxWeaponShot *s = combat.shots + slot_index(d);
  unsigned bit = enemy_bit(enemy);
  if (!bit || !s->active) return original;
  if (s->hit_slots & bit) return 0;
  s->hit_slots |= (uint16_t)bit;
  /* X1 categories 0..5 are ordinary enemies; 6..19 contain the eight
   * Maverick and special encounter/armored response rows at $86:EF37.
   * Source bosses mostly take one from these attacks and one from buster.
   * Preserve that neutral ratio, never transplant another game's weakness. */
  if (r[enemy+0x28]>=6) return original;
  MmxWeaponDamageState *e=combat.enemies+(enemy-0xe68)/64;
  unsigned hp=r[enemy+0x27]&127;
  if (!e->active || e->kind!=r[enemy+10] || hp>e->hp) {
    e->remainder=1; /* Round the cumulative total to nearest whole HP. */
    e->kind=r[enemy+10];e->active=1;
  }
  e->hp=(uint8_t)hp;
  unsigned scaled=original*source_damage(s)+e->remainder;
  e->remainder=(uint8_t)(scaled%3);
  unsigned damage=scaled/3;
  return damage>127 ? 127 : damage;
}
unsigned MmxWeaponsHitbox(const uint8_t r[0x20000], unsigned enemy, unsigned d, unsigned original) {
  if (owned(r,d) && (combat.shots[slot_index(d)].hit_slots & enemy_bit(enemy))) return 0;
  return original;
}

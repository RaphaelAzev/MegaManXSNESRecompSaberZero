#include "mmx_coop.h"
#include "cpu_state.h"
#include "snes/interp_bridge.h"
#include "snes/snes.h"
#include "snes/cart.h"
#include <string.h>

extern uint8_t g_ram[0x20000];
extern Snes *g_snes;

static MmxCoopState state;
static bool enabled;
static unsigned starting_character;
_Static_assert(sizeof(MmxCoopPlayer) == 2272, "Co-op player save ABI");
_Static_assert(sizeof(MmxCoopState) == 4600, "Co-op save ABI");
static bool join_tick(uint8_t *r);
static unsigned word(const uint8_t *p) {return p[0]|p[1]<<8;}
static void putword(uint8_t *p,unsigned v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void sound(uint8_t *r,unsigned command) {
  unsigned i=r[0xba3]&30;r[0xb72+i]=(uint8_t)command;r[0xb73+i]=0;r[0xba3]=(uint8_t)((i+2)&30);
}
bool MmxCoopTransitionActive(void) {
  return enabled && state.initialized && state.players[1].zero.swap_phase;
}

bool MmxCoopEnabled(void) { return enabled; }
void MmxCoopReset(void) {
  memset(&state, 0, sizeof(state));
  MmxWeaponsPartnerCombat(NULL);
  state.players[0].character = (uint8_t)starting_character;
  state.players[1].character = (uint8_t)(starting_character ^ 1);
  if (enabled) {
    MmxZeroState z = {0}; z.active_x = starting_character == MMX_COOP_X;
    MmxZeroSetState(z);
  }
}
bool MmxCoopEnable(unsigned character) {
  if (character > MMX_COOP_ZERO || !MmxZeroEnabled()) return false;
  starting_character = character; enabled = true; MmxCoopReset(); return true;
}
void MmxCoopDisable(void) { enabled = false; starting_character = 0; MmxCoopReset(); }
MmxCoopState MmxCoopGetState(void) { return state; }
bool MmxCoopValidState(const MmxCoopState *s) {
  if (!s || s->initialized > 1 || s->current > 1 || s->controller_pass > 2 ||
      s->reserved || s->object_reserved || s->object_pass > 2 ||
      s->contact_reserved || s->contact_pass > 2 || s->enrolled>1 ||
      s->select_hold>180 || s->select_armed>1 || s->stage_pending>2 ||
      s->menu_owner>2 || s->menu_last>1 || s->menu_reserved[0] || s->menu_reserved[1]) return false;
  if (s->object_pass && s->object_entry != 0xd2bd && s->object_entry != 0xd3dd &&
      s->object_entry != 0xd3fa && s->object_entry != 0xd43a && s->object_entry != 0xd457 &&
      s->object_entry != 0x9d67) return false;
  if (s->contact_pass && s->contact_entry != 0x9b03 && s->contact_entry != 0x9b43) return false;
  for (unsigned i = 0; i < 2; ++i) {
    const MmxCoopPlayer *p = &s->players[i];
    if (p->character > MMX_COOP_ZERO || p->status > MMX_COOP_FALLEN ||
        p->input > 4095 || p->pressed > 4095 ||
        !MmxWeaponsValidState(&p->weapons) || !MmxWeaponsValidCombatState(&p->combat) ||
        !MmxZeroValidState(&p->zero)) return false;
    if (s->initialized && p->zero.active_x != (p->character == MMX_COOP_X)) return false;
    for (unsigned n = 0; n < 4; ++n) if (p->subtanks[n] > 14) return false;
    for (unsigned n = 1; n < 16; n += 2) if (p->energy[n] > 28) return false;
  }
  return s->players[0].character != s->players[1].character;
}
void MmxCoopSetState(const MmxCoopState *s) {
  if (enabled && MmxCoopValidState(s)) {
    state = *s;
    MmxWeaponsPartnerCombat(state.initialized ? &state.players[state.current^1].combat : NULL);
  }
  else MmxCoopReset();
}
void MmxCoopCapture(uint8_t *r) {
  if (!enabled || !state.initialized || !r) return;
  MmxCoopPlayer *p = &state.players[state.current];
  memcpy(p->body, r + 0xba8, sizeof(p->body));
  memcpy(p->auxiliaries, r + 0xc38, sizeof(p->auxiliaries));
  memcpy(p->shots, r + 0x1228, sizeof(p->shots));
  memcpy(p->energy, r + 0x1f87, sizeof(p->energy));
  for (unsigned n = 1; n < 16; n += 2) p->energy[n] &= 63;
  for (unsigned n = 0; n < 4; ++n) p->subtanks[n] = r[0x1f83 + n] & 15;
  p->zero = MmxZeroGetState();
  p->weapons = MmxWeaponsGetState();
  p->combat = MmxWeaponsGetCombatState();
  /* Fractional damage belongs to the world enemy, not to the attacker.
   * Keep the two serialized copies synchronized before projecting either. */
  memcpy(state.players[state.current^1].combat.enemies,p->combat.enemies,sizeof(p->combat.enemies));
  p->shot_command = r[0x1f0d]; p->hud_state = r[0x1f12];
}
bool MmxCoopSelect(uint8_t *r, unsigned player) {
  if (!enabled || !state.initialized || !r || player > 1) return false;
  if (player == state.current) return true;
  MmxCoopCapture(r);
  state.current = (uint8_t)player;
  const MmxCoopPlayer *p = &state.players[player];
  memcpy(r + 0xba8, p->body, sizeof(p->body));
  memcpy(r + 0xc38, p->auxiliaries, sizeof(p->auxiliaries));
  memcpy(r + 0x1228, p->shots, sizeof(p->shots));
  for (unsigned n = 0; n < 16; ++n)
    r[0x1f87 + n] = p->energy[n] | ((n & 1) ? r[0x1f87 + n] & 0xc0 : 0);
  for (unsigned n = 0; n < 4; ++n)
    r[0x1f83 + n] = (r[0x1f83 + n] & 0xf0) | p->subtanks[n];
  MmxZeroSetState(p->zero);
  MmxWeaponsSetState(p->weapons);
  MmxWeaponsSetCombatState(p->combat);
  MmxWeaponsPartnerCombat(&state.players[player^1].combat);
  r[0x1f0d] = p->shot_command; r[0x1f12] = p->hud_state;
  if (g_snes && g_snes->cart)
    MmxZeroSetCollisionRom(g_snes->cart->rom, g_snes->cart->romSize);
  return true;
}
void MmxCoopInitialize(uint8_t *r) {
  if (!enabled || state.initialized || !r || r[0xd1] != 2 || r[0xd2] != 4 || r[0xba9] != 2) return;
  state.initialized = 1; state.stage = r[0x1f7a];
  state.players[0].status = MMX_COOP_ALIVE;
  MmxCoopCapture(r);
  MmxCoopPlayer *partner = &state.players[1];
  /* Inventory starts full for a new partner. Shared unlocks remain in WRAM.
   * Reserves are separately empty; joining never copies P1's stored healing. */
  for (unsigned n = 1; n < 16; n += 2) partner->energy[n] = 28;
  partner->weapons.initialized = 1; memset(partner->weapons.energy, 28, 16);
  partner->zero.active_x = partner->character == MMX_COOP_X;
  MmxWeaponsPartnerCombat(&partner->combat);
}
bool MmxCoopFrameTick(uint8_t *r) {
  if (!enabled || !state.initialized) return MmxWeaponsFrameTick(r);
  if (state.menu_owner) {
    if (state.current==1) MmxCoopApplyInput(r);
    state.select_hold=0;return false;
  }
  if (r[0xd1]!=2 || r[0xd2]!=4 || r[0xba9]!=2 || state.stage!=r[0x1f7a]) {
    state.stage_pending=1;state.select_hold=0;
    return false;
  }
  if (state.stage_pending==1 && r[0xd3]==4 && !r[0x1f0c]) {
    /* Native stage entry/reset has rebuilt P1. Preserve enrollment and
     * reserves, refill P2 as agreed, and wait for safe ground to arrive. */
    MmxCoopCapture(r);state.stage=r[0x1f7a];state.players[0].status=MMX_COOP_ALIVE;
    MmxCoopPlayer *p=&state.players[1];p->status=MMX_COOP_ABSENT;
    p->body[0x27]=r[0x1f9a]|128;
    memset(p->energy,0,sizeof(p->energy));
    for(unsigned i=1;i<16;i+=2)p->energy[i]=28;
    memset(p->weapons.energy,28,sizeof(p->weapons.energy));
    memset(p->weapons.fraction,0,sizeof(p->weapons.fraction));p->weapons.charge=p->weapons.cooldown=0;
    memset(&p->combat,0,sizeof(p->combat));memset(p->shots,0,sizeof(p->shots));
    memset(&p->zero,0,sizeof(p->zero));p->zero.active_x=p->character==MMX_COOP_X;
    state.stage_pending=state.enrolled?2:0;
  }
  if (join_tick(r)) return true;
  if (r[0x1f10]) return false;
  unsigned phases[2]={0,0};
  for (unsigned seat=0;seat<2;++seat) if (state.players[seat].status==MMX_COOP_ALIVE) {
    MmxCoopSelect(r,seat);
    MmxWeaponsFrameTick(r);
    MmxCoopCapture(r);
    phases[seat]=MmxWeaponsTimePhase(&state.players[seat].combat);
  }
  MmxCoopSelect(r,0);
  /* Two staggered half-speed effects must not alternate into a permanent
   * freeze. Advance one shared display-frame cadence, while both ages tick. */
  ++state.time_tick;
  bool frozen=phases[0]==1 || phases[1]==1 ||
      ((phases[0]==2 || phases[1]==2) && !(state.time_tick&1));
  if (frozen) r[0xb9d]=r[0xba0]=0;
  return frozen;
}
void MmxCoopPoll(uint16_t p1, uint16_t p2) {
  if (!enabled) return;
  uint16_t inputs[2] = {p1 & 4095, p2 & 4095};
  for (unsigned i = 0; i < 2; ++i) {
    state.players[i].pressed = inputs[i] & ~state.players[i].input;
    state.players[i].input = inputs[i];
  }
}
void MmxCoopApplyInput(uint8_t *r) {
  if (!enabled || !state.initialized || !r) return;
  unsigned input = state.players[state.current].input, native = 0;
  for (unsigned bit = 0; bit < 12; ++bit) if (input & (1u << bit)) native |= 0x8000u >> bit;
  /* $00:E543..E5F6: preserve X1's configurable button masks at $7E:FFC0..5.
   * Both seats use the game's action layout, after independent host bindings. */
  unsigned buttons = ((native & 255) >> 2 & 0x3c) | (native >> 8 & 0xc0) | (native >> 12 & 3);
  static const uint16_t action_bits[6] = {0x4000, 0x8000, 0x0080, 0x0020, 0x0010, 0x1000};
  unsigned actions = native & 0x0f00;
  for (unsigned i = 0; i < 6; ++i) {
    unsigned mask = r[0xffc0 + i];
    if (mask && (buttons & mask) == mask) actions |= action_bits[i];
  }
  /* Native input mapping writes port 1 into the projected body. Seat 2 takes
   * its previous actions from its own preceding frame snapshot. */
  unsigned previous = word(state.players[state.current].body+0x36);
  r[0xbe0] = (uint8_t)previous; r[0xbe1] = (uint8_t)(previous >> 8);
  r[0xbde] = (uint8_t)actions; r[0xbdf] = (uint8_t)(actions >> 8);
  r[0xbe2] = (uint8_t)(actions & ~previous); r[0xbe3] = (uint8_t)((actions & ~previous) >> 8);
}
bool MmxCoopPlacePartner(uint8_t *r, uint16_t x, uint16_t y) {
  if (!enabled || !state.initialized || !r || state.current || state.players[1].status != MMX_COOP_ABSENT) return false;
  MmxCoopCapture(r);
  MmxCoopPlayer *p = &state.players[1];
  unsigned hp=state.enrolled ? p->body[0x27]&127 : r[0x1f9a];
  unsigned weapon=state.enrolled ? p->body[0x33] : 0;
  /* Use native player setup fields, but never inherit P1's hurt, charge,
   * movement or weapon counters. Personal inventory survives withdrawal. */
  memcpy(p->body, state.players[0].body, sizeof(p->body));
  memset(p->auxiliaries,0,sizeof(p->auxiliaries));memset(p->shots,0,sizeof(p->shots));
  memcpy(p->auxiliaries,state.players[0].auxiliaries,0x60);
  memset(&p->combat,0,sizeof(p->combat));memset(&p->zero,0,sizeof(p->zero));
  p->zero.active_x=p->character==MMX_COOP_X;
  memset(p->body+0x28,0,sizeof(p->body)-0x28);
  memset(p->body+0x1a,0,6); /* velocities, ending before the collision pointer */
  p->body[2]=p->body[3]=0;p->body[14]=1;
  p->body[0x2b]=4; /* solid ground; native idle initializer runs on resume */
  /* Constants from $81:819F..825A. The idle initializer does not rebuild
   * these action-table pointers, so clearing them disables firing entirely. */
  putword(p->body+0x31,0xa597);putword(p->body+0x5f,0xfa80);
  p->body[0x2f]=8;p->body[0x66]=255;p->body[0x67]=3;
  p->body[0x69]=r[0x1f7f]?0:64;p->body[0x78]=4;
  p->body[4] = p->body[7] = 0;
  p->body[5] = (uint8_t)x; p->body[6] = (uint8_t)(x >> 8);
  p->body[8] = (uint8_t)y; p->body[9] = (uint8_t)(y >> 8);
  p->body[0x27] = (uint8_t)(hp|128);p->body[0x33]=(uint8_t)weapon;
  putword(p->body+0x22,x);putword(p->body+0x24,y);
  p->shot_command=0;p->hud_state=2;
  p->status = MMX_COOP_ALIVE;
  state.enrolled=1;
  return true;
}

bool MmxCoopFindLanding(const uint8_t *r,uint16_t *out_x,uint16_t *out_y) {
  if (!r || !out_x || !out_y) return false;
  int px=word(r+0xbad),py=word(r+0xbb0),camera_x=word(r+0x1e4d),camera_y=word(r+0x1e50);
  int height=state.players[1].character==MMX_COOP_ZERO ? 44 : 36;
  const int gaps[]={32,-32,48,-48,64,-64};
  for (unsigned i=0;i<sizeof(gaps)/sizeof(gaps[0]);++i) {
    int x=px+gaps[i];if (x-12<camera_x || x+12>=camera_x+256) continue;
    for (int offset=-16;offset<=24;++offset) {
      int floor=py+16+offset,surface=0;
      unsigned type=MmxWeaponsTerrainClass(r,x,floor);
      /* Require ordinary full ground/slopes. Conservative join rejection
       * for spikes, conveyors, disappearing floors and one-way surfaces. */
      /* $84:961C dispatches $34..36/$3B..3D to ordinary $96B5.
       * $33/$3E/$3F are hurt/spike handlers; $37/$38 are conveyors. */
      if (!(type==0x13 || (type>=1 && type<=12) ||
            (type>=0x34 && type<=0x36) || (type>=0x3b && type<=0x3d)) ||
          !MmxWeaponsTerrainSolid(r,x,floor,true,&surface) || surface!=floor ||
          floor-height<camera_y || floor>=camera_y+224) continue;
      bool clear=true;
      for (int y=floor-height;y<floor && clear;++y) for (int dx=-10;dx<=10;dx+=5) {
        unsigned t=MmxWeaponsTerrainClass(r,x+dx,y);
        if ((t>=0x33 && t!=0x39 && t!=0x3a) ||
            MmxWeaponsTerrainSolid(r,x+dx,y,true,NULL)) {clear=false;break;}
      }
      /* Source enemies and ride armor retain their native space; don't
       * drop a newly joined player directly onto an existing actor. */
      for (unsigned d=0xe18;d<0x1228 && clear;d+=(d==0xe18?0x50:64)) if (r[d]) {
        int dx=(int)word(r+d+5)-x,dy=(int)word(r+d+8)-(floor-16);
        if (dx>-28 && dx<28 && dy>-40 && dy<32) clear=false;
      }
      if (clear) {*out_x=(uint16_t)x;*out_y=(uint16_t)(floor-16);return true;}
    }
  }
  return false;
}
static void clear_partner_combat(uint8_t *r) {
  MmxCoopSelect(r,1);MmxWeaponsCancelShots(r);MmxZeroCancel(r);
  if(r[0xc2f]&64) sound(r,0x17);
  memset(r+0xbff,0,5);memset(r+0xc98,0,0x180);r[0xc2f]&=(uint8_t)~64;
  MmxCoopSelect(r,0);
}
static bool join_tick(uint8_t *r) {
  MmxCoopPlayer *p=&state.players[1];MmxZeroState *z=&p->zero;
  if (z->swap_phase) {
    switch (z->swap_phase) {
      case 1: if (++z->swap_tick==7) {z->swap_phase=2;z->swap_tick=0;} break;
      case 2: {
        int fixed=z->swap_y*256+z->swap_fraction-0x0aa6;
        z->swap_y=(int16_t)((fixed-255)/256);z->swap_fraction=(uint8_t)(fixed-z->swap_y*256);
        if ((int)word(p->body+8)-word(r+0x1e50)+z->swap_y < -40) {
          z->swap_phase=z->swap_tick=z->swap_fraction=0;z->swap_y=0;
          p->status=MMX_COOP_ABSENT;state.select_armed=state.select_hold=0;
        }
        break;
      }
      case 4: z->swap_y+=8;if(z->swap_y>=0) {z->swap_y=0;z->swap_phase=5;z->swap_tick=0;} break;
      case 5: if(++z->swap_tick==7) {z->swap_phase=0;state.select_armed=0;} break;
      default: z->swap_phase=0;break;
    }
    r[0xb9d]=r[0xba0]=0;return true;
  }
  bool gameplay=r[0xd1]==2 && r[0xd2]==4 && r[0xd3]==4 && r[0xba9]==2 &&
      (r[0xbcf]&127) && r[0xbaa]!=12 &&
      !r[0x1f0c] && !r[0x1f10] && !r[0x1f23] && !r[0x1f48];
  if (!gameplay) {state.select_hold=0;return false;}
  if (!(p->input&4)) {state.select_armed=1;state.select_hold=0;}
  if (p->status==MMX_COOP_FALLEN) {state.select_hold=0;return false;}
  if (p->status==MMX_COOP_ALIVE) {
    if (!(p->body[0x27]&127) || p->body[2]==12) {state.select_hold=0;return false;}
    if ((p->input&4) && state.select_armed && ++state.select_hold>=180) {
      clear_partner_combat(r);z=&p->zero;z->swap_phase=1;z->swap_tick=z->swap_fraction=0;z->swap_y=0;
      state.select_hold=0;sound(r,0x0f);r[0xb9d]=r[0xba0]=0;return true;
    }
    return false;
  }
  bool automatic=state.stage_pending==2;
  if (!(p->pressed&4) && !automatic) return false;
  uint16_t x,y;
  if (!MmxCoopFindLanding(r,&x,&y)) {
    if(!automatic) sound(r,0x74); /* $00:F1E4 password rejection. */
    return false;
  }
  if (!MmxCoopPlacePartner(r,x,y)) return false;
  state.stage_pending=0;
  z=&p->zero;z->swap_phase=4;z->swap_y=(int16_t)(word(r+0x1e50)-(int)y-40);
  z->swap_tick=z->swap_fraction=0;state.select_armed=state.select_hold=0;
  sound(r,0x0e);r[0xb9d]=r[0xba0]=0;return true;
}

static void contact_hook(CpuState *cpu,uint32_t pc) {
  if (!enabled || !state.initialized || state.menu_owner || state.players[1].status != MMX_COOP_ALIVE) return;
  unsigned at = pc & 65535;
  if (at == 0x9b03 || at == 0x9b43) {
    if (!state.current && !state.contact_pass) {
      state.contact_pass = 1; state.contact_entry = (uint16_t)at;
      state.contact_s = cpu->S; state.contact_d = cpu->D;
    }
    return;
  }
  /* A helper may return through a shared RTL; only the owning guest call's
   * balanced return boundary can complete or restart this pass. */
  if (!state.contact_pass || cpu->S != state.contact_s || cpu->D != state.contact_d) return;
  if (state.contact_pass == 1) {
    /* Retail stops the projectile scan after its first contact, including
     * immune/reflecting hits. Extend the scan to P2 only after a real miss. */
    if (state.contact_entry == 0x9b43 && at != 0x9b7d) { state.contact_pass = 0; return; }
    state.contact_a = cpu->A; state.contact_x = cpu->X; state.contact_y = cpu->Y;
    state.contact_db = cpu->DB; cpu_mirrors_to_p(cpu); state.contact_p = cpu->P;
    MmxCoopSelect(g_ram,1); state.contact_pass = 2;
    interp_bridge_pre_opcode_redirect(0x840000 | state.contact_entry);
  } else {
    bool first_hit = (state.contact_a & 255) != 0;
    bool no_second_hit = !(cpu->A & 255);
    MmxCoopSelect(g_ram,0);
    if (first_hit || no_second_hit) {
      cpu->A = state.contact_a; cpu->X = state.contact_x; cpu->Y = state.contact_y;
      cpu->DB = state.contact_db; cpu->P = state.contact_p; cpu_p_to_mirrors(cpu);
    }
    state.contact_pass = 0;
  }
}
static void object_hook(CpuState *cpu, uint32_t pc) {
  if (!enabled || !state.initialized || state.menu_owner || state.players[1].status != MMX_COOP_ALIVE) return;
  unsigned at = pc & 65535;
  bool entry = at == 0xd2bd || at == 0xd3dd || at == 0xd3fa || at == 0xd43a || at == 0xd457 || at == 0x9d67;
  if (entry) {
    if (!state.object_pass && !state.current) {
      state.object_pass = 1; state.object_entry = (uint16_t)at;
    }
    return;
  }
  if (state.object_pass == 1) {
    state.object_a = cpu->A; state.object_x = cpu->X; state.object_y = cpu->Y;
    state.object_s = cpu->S; state.object_d = cpu->D; state.object_db = cpu->DB;
    cpu_mirrors_to_p(cpu); state.object_p = cpu->P;
    MmxCoopSelect(g_ram,1); state.object_pass = 2;
    interp_bridge_pre_opcode_redirect((pc & 0xff0000) | state.object_entry);
  } else if (state.object_pass == 2) {
    MmxCoopSelect(g_ram,0);
    cpu->A = state.object_a; cpu->X = state.object_x; cpu->Y = state.object_y;
    cpu->D = state.object_d; cpu->DB = state.object_db; cpu->P = state.object_p;
    cpu_p_to_mirrors(cpu); state.object_pass = 0;
  }
}
static bool shared_screen(void) {
  return enabled && state.initialized && !state.menu_owner && state.players[0].status==MMX_COOP_ALIVE &&
      state.players[1].status==MMX_COOP_ALIVE && !MmxCoopTransitionActive() &&
      g_ram[0xd1]==2 && g_ram[0xd2]==4 && g_ram[0xd3]==4 &&
      !g_ram[0x1f0c] && !g_ram[0x1f23] && !g_ram[0x1f48];
}
static void constrain_player(uint8_t *r) {
  if (!shared_screen()) return;
  const MmxCoopPlayer *other=&state.players[state.current^1];
  int x=word(r+0xbad),ox=word(other->body+5);
  int limited=x<ox-224 ? ox-224 : x>ox+224 ? ox+224 : x;
  if (limited!=x) {putword(r+0xbad,(unsigned)limited);r[0xbac]=0;putword(r+0xbc2,0);}
}
static void camera_hook(CpuState *cpu,uint32_t pc) {
  if (!shared_screen()) return;
  unsigned axis=((pc&65535)==0xdebf || (pc&65535)==0xdeca) ? 8 : 5;
  unsigned a=word(state.players[0].body+axis),b=word(state.players[1].body+axis);
  cpu->A=(uint16_t)((a+b)/2);
  cpu->_flag_N=(cpu->A&0x8000)!=0;cpu->_flag_Z=cpu->A==0;
  cpu->P=(cpu->P&~0x82)|(cpu->_flag_N?0x80:0)|(cpu->_flag_Z?2:0);
}
static void menu_hook(CpuState *cpu,uint32_t pc) {
  if (!enabled || !state.initialized || MmxCoopTransitionActive()) return;
  switch (pc&65535) {
    case 0xe57f:
      if (state.menu_owner==2 && state.current==1) {
        MmxCoopApplyInput(g_ram);cpu->A=(uint16_t)word(g_ram+0xbe2);
        cpu->_flag_N=(cpu->A&0x8000)!=0;cpu->_flag_Z=cpu->A==0;
        cpu->P=(cpu->P&~0x82)|(cpu->_flag_N?0x80:0)|(cpu->_flag_Z?2:0);
      }
      break;
    case 0x9e68: {
      if (state.menu_owner) return;
      unsigned seat=(g_ram[0xbe3]&0x10) ? 0 : 1;
      if (state.players[seat].status!=MMX_COOP_ALIVE ||
          (seat && !(state.players[1].pressed&8))) return;
      state.menu_owner=(uint8_t)(seat+1);state.menu_last=(uint8_t)seat;
      MmxCoopSelect(g_ram,seat);
      if (seat) {MmxCoopApplyInput(g_ram);g_ram[0xbe3]|=0x10;}
      break;
    }
    case 0x9eac: /* Native pause guards rejected the request. */
    case 0xc579: /* Equipment and subtank changes are now committed. */
      if(state.menu_owner) {MmxCoopSelect(g_ram,0);state.menu_owner=0;}
      break;
  }
}
static void controller_hook(CpuState *cpu, uint32_t pc) {
  if (!enabled) return;
  if ((pc & 0x7fffff) == 0x048fcb) {
    /* There is exactly one X. Preserve his native CHR allocation; Zero's
     * complete body comes from the original X3 asset compositor. PHP has
     * already run, so the native PLP/RTL remains balanced. */
    if (MmxZeroActive() && cpu->D >= 0xba8 && cpu->D < 0xc98) {
      g_ram[cpu->D + 0x17] &= 127;
      interp_bridge_pre_opcode_redirect(0x848fc8);
    }
    return;
  }
  if ((pc & 0x7fffff) == 0x0280df) {
    /* Keep native visibility/culling, but do not enqueue a second pointer to
     * the projected P1 address. P2 is drawn from its immutable context. */
    if (state.initialized && state.current == 1 &&
        ((cpu->D >= 0xba8 && cpu->D < 0xe18) || (cpu->D >= 0x1228 && cpu->D < 0x1428)))
      interp_bridge_pre_opcode_redirect(0x82810a);
    return;
  }
  if ((pc & 0xffff) == 0x8136) {
    MmxCoopInitialize(g_ram);
    if (state.initialized && !state.controller_pass) state.controller_pass = 1;
    return;
  }
  if (!state.initialized || !state.controller_pass) return;
  constrain_player(g_ram);
  if (state.controller_pass == 1 && !state.menu_owner && state.players[1].status == MMX_COOP_ALIVE &&
      g_ram[0xd1] == 2 && g_ram[0xd2] == 4 && !g_ram[0x1f0c]) {
    state.return_a = cpu->A; state.return_x = cpu->X; state.return_y = cpu->Y;
    state.return_s = cpu->S; state.return_db = cpu->DB;
    cpu_mirrors_to_p(cpu); state.return_p = cpu->P;
    MmxCoopSelect(g_ram, 1); MmxCoopApplyInput(g_ram);
    /* $00:D1F3..D206 prepares these outside the player routine. P2 needs
     * its own previous position and per-frame fire-command reset too. */
    if (!g_ram[0x1f19]) {
      memcpy(g_ram+0xbca,g_ram+0xbad,2); memcpy(g_ram+0xbcc,g_ram+0xbb0,2);
    }
    g_ram[0x1f0d] = 0;
    state.controller_pass = 2;
    cpu->P |= 0x30; cpu_p_to_mirrors(cpu); cpu->X &= 255; cpu->Y &= 255;
    /* Re-enter AFTER PHP/PHD/PLD. Both passes share exactly one prologue and
     * epilogue, so no synthetic JSL or extra stack frame is needed. */
    interp_bridge_pre_opcode_redirect(0x818136);
    return;
  }
  if (state.controller_pass == 2) {
    g_ram[0xbd4] = 0; /* P2's counterpart of $00:D21A. */
    MmxCoopSelect(g_ram, 0);
    cpu->A = state.return_a; cpu->X = state.return_x; cpu->Y = state.return_y;
    cpu->DB = state.return_db; cpu->P = state.return_p; cpu_p_to_mirrors(cpu);
  }
  MmxCoopCapture(g_ram);
  state.controller_pass = 0;
}
void MmxCoopRegisterHooks(void) {
  interp_bridge_set_pre_opcode_hook(0x818136, controller_hook);
  interp_bridge_set_pre_opcode_hook(0x81819c, controller_hook);
  interp_bridge_set_pre_opcode_hook(0x848fcb, controller_hook);
  interp_bridge_set_pre_opcode_hook(0x8280df, controller_hook);
  const unsigned objects[] = {0xd2bd,0xd2dd,0xd3dd,0xd3f9,0xd3fa,0xd422,
      0xd43a,0xd456,0xd457,0xd47f,0x819d67,0x819d79};
  for (unsigned i=0;i<sizeof(objects)/sizeof(objects[0]);++i)
    interp_bridge_set_pre_opcode_hook(objects[i],object_hook);
  const unsigned contacts[] = {0x849b03,0x849b43,0x849b42,0x849b7d,0x849d82,
      0x849dc9,0x849dcc,0x849ee9};
  for (unsigned i=0;i<sizeof(contacts)/sizeof(contacts[0]);++i)
    interp_bridge_set_pre_opcode_hook(contacts[i],contact_hook);
  const unsigned cameras[]={0xdea0,0xdeab,0xdebf,0xdeca};
  for(unsigned i=0;i<sizeof(cameras)/sizeof(cameras[0]);++i)
    interp_bridge_set_pre_opcode_hook(cameras[i],camera_hook);
  interp_bridge_set_pre_opcode_hook(0x9e68,menu_hook);
  interp_bridge_set_pre_opcode_hook(0x9eac,menu_hook);
  interp_bridge_set_pre_opcode_hook(0xc579,menu_hook);
  interp_bridge_set_pre_opcode_hook(0xe57f,menu_hook);
}

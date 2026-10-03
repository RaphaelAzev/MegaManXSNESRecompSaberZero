#include "mmx_coop.h"
_Static_assert(sizeof(MmxCoopState) == MMX_COOP_LEGACY_STATE_SIZE + 2 * sizeof(MmxZeroModernState),
               "Update the legacy co-op importer when its layout changes");
#include "cpu_state.h"
#include "snes/interp_bridge.h"
#include "snes/snes.h"
#include "snes/cart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

extern uint8_t g_ram[0x20000];
extern Snes *g_snes;

static MmxCoopState state = {.players = {{.character = MMX_COOP_X}, {.character = MMX_COOP_ZERO}}};
static bool enabled;
static unsigned starting_character;
_Static_assert(sizeof(MmxCoopPlayer) == 2276, "Co-op player save ABI");
_Static_assert(sizeof(MmxCoopState) == 4664, "Co-op save ABI");
static bool join_tick(uint8_t *r);
static bool scene_tick(uint8_t *r);
static unsigned word(const uint8_t *p) {return p[0]|p[1]<<8;}
static void putword(uint8_t *p,unsigned v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void sound(uint8_t *r,unsigned command) {
  unsigned i=r[0xba3]&30;r[0xb72+i]=(uint8_t)command;r[0xb73+i]=0;r[0xba3]=(uint8_t)((i+2)&30);
}
/* Host-only opt-in trace. Never changes WRAM, scheduling or serialized state.
 * Keep two bounded CSV segments so a long play session cannot fill the disk.
 * Rollback/replayed frames retain their original guest counters and a distinct
 * host sequence number rather than being mistaken for consecutive simulation. */
static FILE *diagnostic_file;
static char diagnostic_path[2048], diagnostic_previous[2064];
static bool diagnostic_checked;
static bool diagnostic_enabled;
static unsigned diagnostic_session;
static unsigned diagnostic_sequence, diagnostic_rows;
static long diagnostic_bytes;
extern int snes_frame_counter;
bool MmxCoopDiagnosticsEnabled(void) {return diagnostic_enabled;}
void MmxCoopSetDiagnosticsEnabled(bool active) {
  if(diagnostic_file) fclose(diagnostic_file);
  diagnostic_file=NULL;diagnostic_checked=false;
  diagnostic_sequence=diagnostic_rows=0;diagnostic_bytes=0;
  diagnostic_enabled=active;
}
static void diagnostic_open(void) {
  if (diagnostic_checked) return;
  diagnostic_checked=true;
  time_t now=time(NULL);struct tm *local=localtime(&now);
  char stamp[32]="unknown-time";
  if(local) strftime(stamp,sizeof(stamp),"%Y%m%d-%H%M%S",local);
#ifdef _WIN32
  _mkdir("logs");unsigned long pid=(unsigned long)_getpid();
#else
  mkdir("logs",0755);unsigned long pid=(unsigned long)getpid();
#endif
  snprintf(diagnostic_path,sizeof(diagnostic_path),"logs/coop-physics-%s-%lu-%u.csv",
      stamp,pid,++diagnostic_session);
  snprintf(diagnostic_previous,sizeof(diagnostic_previous),"%s.previous.csv",diagnostic_path);
  diagnostic_file=fopen(diagnostic_path,"wb");
  if (!diagnostic_file) {fprintf(stderr,"[coop-physics] cannot open %s\n",diagnostic_path);return;}
  fprintf(stderr,"[coop-physics] recording %s (32 MiB per segment, plus previous segment)\n",diagnostic_path);
}
static void diagnostic_header(void) {
  diagnostic_bytes=fprintf(diagnostic_file,
      "sequence,host_frame,world_tick,event,pc,cpu_d,cpu_s,stage,mode,submode,phase,"
      "current,anchor,controller_pass,object_pass,contact_pass,pickup_pass,pickup_d,"
      "menu_owner,scene_owner,camera_x,camera_y,freeze_flags,pickup_owners,seat,"
      "character,status,input,x,y,previous_x,previous_y,vx,vy,hp,ground,"
      "terrain_above_feet,solid_above_feet,terrain_feet,solid_feet,body,scratch\n");
}
static void diagnostic_event(const uint8_t *r,const CpuState *cpu,uint32_t pc,const char *event) {
  if (!diagnostic_enabled || !enabled || !state.initialized) return;
  diagnostic_open();if (!diagnostic_file) return;
  if (!diagnostic_bytes) diagnostic_header();
  if (diagnostic_bytes>=32L*1024*1024) {
    fclose(diagnostic_file);diagnostic_file=NULL;
    /* Both filenames belong exclusively to this explicitly requested trace. */
    remove(diagnostic_previous);
    if (rename(diagnostic_path,diagnostic_previous)) {
      fprintf(stderr,"[coop-physics] rotation failed; recording stopped\n");return;
    }
    diagnostic_file=fopen(diagnostic_path,"wb");
    if (!diagnostic_file) return;
    diagnostic_header();
  }
  char flags[15],owners[33],scratch[129];
  for(unsigned i=0;i<7;++i) snprintf(flags+i*2,3,"%02x",r[0x1f13+i]);
  for(unsigned i=0;i<16;++i) snprintf(owners+i*2,3,"%02x",state.pickup_owner[i]);
  for(unsigned i=0;i<64;++i) snprintf(scratch+i*2,3,"%02x",r[i]);
  unsigned sequence=++diagnostic_sequence;
  for(unsigned seat=0;seat<2;++seat) {
    /* Other-seat history is sampled once at frame end; repeated copies at
     * every interpreter boundary add volume without new observations. */
    bool frame_end=!strcmp(event,"frame-end");
    if(seat!=state.current && !frame_end) continue;
    const MmxCoopPlayer *p=&state.players[seat];
    const uint8_t *b=seat==state.current ? r+0xba8 : p->body;
    char body[0x90*2+1]={0};
    if(frame_end || !strcmp(event,"pickup"))
      for(unsigned i=0;i<0x90;++i) snprintf(body+i*2,3,"%02x",b[i]);
    int x=word(b+5),y=word(b+8);
    int count=fprintf(diagnostic_file,
        "%u,%d,%u,%s,%06x,%04x,%04x,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%04x,"
        "%u,%u,%u,%u,%s,%s,%u,%u,%u,%04x,%d,%d,%u,%u,%d,%d,%u,%u,%u,%u,%u,%u,%s,%s\n",
        sequence,snes_frame_counter,r[0xb9c],event,(unsigned)pc,
        cpu?(unsigned)cpu->D:0,cpu?(unsigned)cpu->S:0,r[0x1f7a],r[0xd1],r[0xd2],r[0xd3],
        state.current,state.anchor,state.controller_pass,state.object_pass,state.contact_pass,
        state.pickup_pass,state.pickup_d,state.menu_owner,state.scene_owner,word(r+0x1e4d),word(r+0x1e50),
        flags,owners,seat+1,p->character,p->status,p->input,x,y,word(b+0x22),word(b+0x24),
        (int16_t)word(b+0x1a),(int16_t)word(b+0x1c),b[0x27]&127,b[0x2b],
        MmxWeaponsTerrainClass(r,x,y+8),MmxWeaponsTerrainSolid(r,x,y+8,true,NULL),
        MmxWeaponsTerrainClass(r,x,y+16),MmxWeaponsTerrainSolid(r,x,y+16,true,NULL),body,scratch);
    if(count<0) {fclose(diagnostic_file);diagnostic_file=NULL;return;}
    diagnostic_bytes+=count;
  }
  if (++diagnostic_rows%60==0) fflush(diagnostic_file);
}
void MmxCoopDiagnosticFrame(const uint8_t *r) {diagnostic_event(r,NULL,0,"frame-end");}
bool MmxCoopTransitionActive(void) {
  return enabled && state.initialized && (state.players[0].zero.swap_phase || state.players[1].zero.swap_phase);
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
      s->platform_riders > 3 || s->contact_pass > 2 || s->enrolled>1 ||
      s->select_hold>180 || s->select_armed>1 || s->stage_pending>2 || /* Accept older 3-second hold saves. */
      s->menu_owner>2 || s->menu_last>1 || s->menu_reserved[0] || s->menu_reserved[1] ||
      s->pickup_pass>2 || s->pickup_reserved[0] || s->pickup_reserved[1] || s->pickup_reserved[2] ||
      s->anchor>1 || s->solo_death[0]>1 || s->solo_death[1]>1 || s->death_reserved ||
      s->scene_owner>2 || s->scene_phase>3 || s->door_pass>2 || s->scene_reserved || s->slime_p2>255 ||
      (s->scene_phase && !s->scene_owner) ||
      (s->door_pass && s->door_entry!=0xe70d && s->door_entry!=0xec98 && s->door_entry!=0xc0ae)) return false;
  for(unsigned i=0;i<16;++i) if(s->pickup_owner[i]>2) return false;
  if(s->pickup_pass && (s->pickup_d<0x1628 || s->pickup_d>=0x1928 || (s->pickup_d-0x1628)%48)) return false;
  if (s->object_pass && s->object_entry != 0xd2bd && s->object_entry != 0xd3dd &&
      s->object_entry != 0xd3fa && s->object_entry != 0xd43a && s->object_entry != 0xd457 &&
      s->object_entry != 0x9d67) return false;
  if (s->contact_pass && s->contact_entry != 0x9b03 && s->contact_entry != 0x9b43 &&
      s->contact_entry != 0xab81 && s->contact_entry != 0xab56) return false;
  for (unsigned i = 0; i < 2; ++i) {
    const MmxCoopPlayer *p = &s->players[i];
    if (p->character > MMX_COOP_ZERO || p->status > MMX_COOP_FALLEN ||
        p->input > 4095 || p->pressed > 4095 ||
        !MmxWeaponsValidState(&p->weapons) || !MmxWeaponsValidCombatState(&p->combat) ||
        !MmxZeroValidState(&p->zero)) return false;
    if (s->initialized && p->zero.active_x != (p->character == MMX_COOP_X)) return false;
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
  diagnostic_event(r,NULL,0,"select-before");
  MmxCoopCapture(r);
  state.current = (uint8_t)player;
  const MmxCoopPlayer *p = &state.players[player];
  memcpy(r + 0xba8, p->body, sizeof(p->body));
  memcpy(r + 0xc38, p->auxiliaries, sizeof(p->auxiliaries));
  memcpy(r + 0x1228, p->shots, sizeof(p->shots));
  for (unsigned n = 0; n < 16; ++n)
    r[0x1f87 + n] = p->energy[n] | ((n & 1) ? r[0x1f87 + n] & 0xc0 : 0);
  MmxZeroSetState(p->zero);
  MmxWeaponsSetState(p->weapons);
  MmxWeaponsSetCombatState(p->combat);
  MmxWeaponsPartnerCombat(&state.players[player^1].combat);
  r[0x1f0d] = p->shot_command; r[0x1f12] = p->hud_state;
  if (g_snes && g_snes->cart)
    MmxZeroSetCollisionRom(g_snes->cart->rom, g_snes->cart->romSize);
  return true;
}
static void select_world_survivor(uint8_t *r) {
  unsigned other=state.anchor^1;
  if(state.current==state.anchor && !(r[0xbcf]&127) &&
      state.players[other].status==MMX_COOP_ALIVE && (state.players[other].body[0x27]&127)) {
    /* World scripts must see the living actor during the other seat's death
     * countdown, not only after its orbs have finished spawning. */
    state.anchor=(uint8_t)other;MmxCoopSelect(r,other);
  }
}
static bool refill_paused(const uint8_t *r) {
  if(!r[0x1f19] || state.menu_owner || state.scene_owner) return false;
  for(unsigned seat=0;seat<2;++seat) {
    const uint8_t *body=seat==state.current ? r+0xba8 : state.players[seat].body;
    if(state.players[seat].status==MMX_COOP_ALIVE && body[2]==0x18) return true;
  }
  return false;
}
void MmxCoopInitialize(uint8_t *r) {
  if (!enabled || state.initialized || !r || r[0xd1] != 2 || r[0xd2] != 4 || r[0xba9] != 2) return;
  state.initialized = 1; state.stage = r[0x1f7a];
  state.players[0].status = MMX_COOP_ALIVE;
  MmxCoopCapture(r);
  MmxCoopPlayer *partner = &state.players[1];
  /* Inventory starts full for a new partner. Shared unlocks and subtanks
   * remain in world WRAM; joining never creates another stored-healing pool. */
  for (unsigned n = 1; n < 16; n += 2) partner->energy[n] = 28;
  partner->weapons.initialized = 1; memset(partner->weapons.energy, 28, 16);
  partner->zero.active_x = partner->character == MMX_COOP_X;
  partner->body[0x27]=r[0x1f9a]|128;
  /* Co-op starts with both players enrolled. The ordinary arrival path waits
   * for gameplay and a safe landing, then uses the original teleport art. */
  state.enrolled=1;state.stage_pending=2;
  MmxWeaponsPartnerCombat(&partner->combat);
}
bool MmxCoopFrameTick(uint8_t *r) {
  if (!enabled || !state.initialized) return MmxWeaponsFrameTick(r);
  if (state.menu_owner) {
    if (state.current==1) MmxCoopApplyInput(r);
    state.select_hold=0;return false;
  }
  /* A changed stage requests initialization; it must not keep returning
   * before the block below can adopt the new stage and revive the roster. */
  if (state.stage!=r[0x1f7a]) state.stage_pending=1;
  if (r[0xd1]!=2 || r[0xd2]!=4 || r[0xba9]!=2) {
    state.stage_pending=1;state.select_hold=0;
    /* A new stage/checkpoint is initialized for the configured P1, even
     * when P2 was the last survivor driving the preceding world tasks. */
    if(state.anchor && (r[0xd1]!=2 || r[0xd2]!=4)) {MmxCoopSelect(r,0);state.anchor=0;}
    return false;
  }
  if (state.stage_pending==1 && r[0xd3]==4 && !r[0x1f0c]) {
    /* Native stage entry/reset has rebuilt P1. Preserve enrollment and
     * reserves, refill P2 as agreed, and wait for safe ground to arrive. */
    MmxWeaponsState weapons=MmxWeaponsGetState();
    weapons.page=weapons.weapon=weapons.menu_page=0;weapons.charge=weapons.cooldown=0;
    MmxWeaponsSetState(weapons);
    MmxCoopCapture(r);state.stage=r[0x1f7a];state.players[0].status=MMX_COOP_ALIVE;
    memset(state.pickup_owner,0,sizeof(state.pickup_owner));state.pickup_pass=0;
    memset(state.solo_death,0,sizeof(state.solo_death));
    state.scene_owner=state.scene_phase=state.door_pass=0;
    MmxCoopPlayer *p=&state.players[1];p->status=MMX_COOP_ABSENT;
    /* Match native P1's buster reset, for X1 and imported selections alike.
     * Voluntary withdrawal and scene transport retain their own selection. */
    p->body[0x33]=0;p->weapons.page=p->weapons.weapon=p->weapons.menu_page=0;
    p->body[0x27]=r[0x1f9a]|128;
    memset(p->energy,0,sizeof(p->energy));
    for(unsigned i=1;i<16;i+=2)p->energy[i]=28;
    memset(p->weapons.energy,28,sizeof(p->weapons.energy));
    memset(p->weapons.fraction,0,sizeof(p->weapons.fraction));p->weapons.charge=p->weapons.cooldown=0;
    memset(&p->combat,0,sizeof(p->combat));memset(p->shots,0,sizeof(p->shots));
    memset(&p->zero,0,sizeof(p->zero));p->zero.active_x=p->character==MMX_COOP_X;
    state.stage_pending=state.enrolled?2:0;
  }
  MmxCoopCapture(r);
  select_world_survivor(r);
  if((r[0xbcf]&127) && !(state.players[state.anchor^1].body[0x27]&127)) {
    /* Repair old co-op saves made after Penguin saw a dead world actor.
     * His combat state writes .30 only at $81:B6ED (player-dead latch).
     * Ordinary hit immunity uses .35 and its damage row; preserve those. */
    for(unsigned d=0xe68;d<0x1228;d+=64)
      if(r[d] && r[d+10]==2 && r[d+1]==4 && (r[d+0x27]&127) && r[d+0x30]==1)
        r[d+0x30]=0;
  }
  /* Simultaneous fatalities must leave only one native death controller
   * responsible for the life decrement/checkpoint transition. */
  if(state.players[0].status==MMX_COOP_ALIVE && state.players[1].status==MMX_COOP_ALIVE &&
      !(state.players[0].body[0x27]&127) && !(state.players[1].body[0x27]&127)) {
    state.players[state.anchor^1].status=MMX_COOP_FALLEN;
    state.solo_death[state.anchor]=0;
  }
  if (scene_tick(r)) return true;
  /* Retail refills park their collector in action $18 while the item task
   * advances HP/energy. $00:D263 skips terrain collision when $1F19 is set.
   * The other actor must park too; running its motion through that pause
   * lets it fall through the floor. Keep running the native refill task. */
  if(refill_paused(r)) return false;
  if (!state.scene_owner && join_tick(r)) return true;
  if (r[0x1f10]>=6) return false;
  unsigned phases[2]={0,0};
  for (unsigned seat=0;seat<2;++seat) if (state.players[seat].status==MMX_COOP_ALIVE &&
      (!state.scene_owner || seat==state.anchor)) {
    MmxCoopSelect(r,seat);
    MmxWeaponsFrameTick(r);
    MmxCoopCapture(r);
    phases[seat]=MmxWeaponsTimePhase(&state.players[seat].combat);
  }
  MmxCoopSelect(r,state.anchor);
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
static void place_other(uint8_t *r,uint16_t x,uint16_t y,bool preserve) {
  MmxCoopCapture(r);
  MmxCoopPlayer *p = &state.players[state.current^1];
  const MmxCoopPlayer *source=&state.players[state.current];
  unsigned hp=preserve ? p->body[0x27]&127 : r[0x1f9a];
  unsigned weapon=preserve ? p->body[0x33] : 0;
  /* Use native player setup fields, but never inherit P1's hurt, charge,
   * movement or weapon counters. Personal inventory survives withdrawal. */
  memcpy(p->body, source->body, sizeof(p->body));
  memset(p->auxiliaries,0,sizeof(p->auxiliaries));memset(p->shots,0,sizeof(p->shots));
  memcpy(p->auxiliaries,source->auxiliaries,0x60);
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
}
bool MmxCoopPlacePartner(uint8_t *r,uint16_t x,uint16_t y) {
  if (!enabled || !state.initialized || !r || state.current || state.players[1].status!=MMX_COOP_ABSENT) return false;
  place_other(r,x,y,state.enrolled!=0);state.enrolled=1;return true;
}

bool MmxCoopFindLanding(const uint8_t *r,uint16_t *out_x,uint16_t *out_y) {
  if (!r || !out_x || !out_y) return false;
  int px=word(r+0xbad),py=word(r+0xbb0),camera_x=word(r+0x1e4d),camera_y=word(r+0x1e50);
  int height=state.players[state.current^1].character==MMX_COOP_ZERO ? 44 : 36;
  const int gaps[]={32,-32,48,-48,64,-64,16,-16,0};
  /* A scene may finish on a narrow ledge just inside a door. The players
   * can overlap, so returning beside/on the driver is preferable to being
   * stranded across the door. Voluntary joins keep the wider clear space. */
  unsigned gap_count=state.scene_owner ? 9 : 6;
  for (unsigned i=0;i<gap_count;++i) {
    int x=px+gaps[i];if (x-12<camera_x || x+12>=camera_x+256) continue;
    for (int offset=-16;offset<=24;++offset) {
      int floor=py+16+offset,surface=0;
      unsigned type=MmxWeaponsTerrainClass(r,x,floor);
      /* $84:961C: ordinary ground, slopes and solid conveyors ($37/$38)
       * support a landing. Mammoth's arena can offer only conveyor tiles.
       * Spikes and transient/one-way surfaces remain ineligible. */
      if (!(type==0x13 || (type>=1 && type<=12) ||
            (type>=0x34 && type<=0x38) || (type>=0x3b && type<=0x3d)) ||
          !MmxWeaponsTerrainSolid(r,x,floor,true,&surface) || surface!=floor ||
          floor-height<camera_y || floor>=camera_y+224) continue;
      bool clear=true;
      /* A clear destination across a shut door is not a usable return.
       * Check the corridor between the actors as well as the landing box. */
      for(int cx=px;cx!=x && clear;cx+=x>px?1:-1)
        if(MmxWeaponsTerrainSolid(r,cx,py-8,true,NULL)) clear=false;
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
/* Both scene transport and voluntary join use the source teleport timing. */
static bool teleport_tick(uint8_t *r,MmxCoopPlayer *p) {
  MmxZeroState *z=&p->zero;
  switch(z->swap_phase) {
    case 1: if(++z->swap_tick==7) {z->swap_phase=2;z->swap_tick=0;} break;
    case 2: {
      int fixed=z->swap_y*256+z->swap_fraction-0x0aa6;
      z->swap_y=(int16_t)((fixed-255)/256);z->swap_fraction=(uint8_t)(fixed-z->swap_y*256);
      if((int)word(p->body+8)-word(r+0x1e50)+z->swap_y < -40) {
        z->swap_phase=z->swap_tick=z->swap_fraction=0;z->swap_y=0;
      }
      break;
    }
    case 4: z->swap_y+=8;if(z->swap_y>=0) {z->swap_y=0;z->swap_phase=5;z->swap_tick=0;} break;
    case 5: if(++z->swap_tick==7) z->swap_phase=0;break;
    default: z->swap_phase=0;break;
  }
  return !z->swap_phase;
}
static void begin_scene(uint8_t *r) {
  unsigned other=state.anchor^1;
  if(state.scene_owner || state.players[other].status!=MMX_COOP_ALIVE ||
      !(state.players[other].body[0x27]&127)) return;
  MmxCoopSelect(r,other);MmxZeroCancel(r);MmxWeaponsCancelShots(r);
  memset(r+0xc98,0,0x180);MmxCoopSelect(r,state.anchor);
  MmxZeroState *z=&state.players[other].zero;
  z->swap_phase=1;z->swap_tick=z->swap_fraction=0;z->swap_y=0;
  state.scene_owner=(uint8_t)(state.anchor+1);state.scene_phase=1;
  sound(r,0x0f);
}
static bool scene_tick(uint8_t *r) {
  /* Capsule acquisition deliberately clears the body lock ($87:CD20).
   * $1F48 stays set through the subsequent recorded-input demonstration,
   * until $87:CE06. $1F3B clears earlier, before that demonstration starts. */
  if(!state.scene_owner && state.players[state.anchor^1].status==MMX_COOP_ALIVE &&
      (r[0xbcf]&127) && r[0xbaa]!=12 &&
      (r[0x1f0c] || r[0x1f23] || r[0x1f48] || (r[0xc16] && (r[0x1f31] || r[0x1f3b])))) begin_scene(r);
  if(!state.scene_owner) return false;
  MmxCoopPlayer *p=&state.players[state.anchor^1];
  if(p->status!=MMX_COOP_ALIVE) {state.scene_owner=state.scene_phase=0;return false;}
  if(state.scene_phase==1 || state.scene_phase==3) {
    if(teleport_tick(r,p)) {
      if(state.scene_phase==1) state.scene_phase=2;
      else {state.scene_phase=state.scene_owner=0;state.select_armed=state.select_hold=0;}
    }
    r[0xb9d]=r[0xba0]=0;return true;
  }
  if(!r[0x1f0c] && !r[0xc16] && !r[0x1f23] && !r[0x1f13] && !r[0x1f48] && r[0x1f10]<6 && r[0xd3]==4) {
    uint16_t x,y;
    if(!MmxCoopFindLanding(r,&x,&y)) return false;
    place_other(r,x,y,true);p=&state.players[state.anchor^1];
    p->zero.swap_phase=4;p->zero.swap_y=(int16_t)(word(r+0x1e50)-(int)y-40);
    state.scene_phase=3;sound(r,0x0e);r[0xb9d]=r[0xba0]=0;return true;
  }
  return false;
}
static void door_hook(CpuState *cpu,uint32_t pc) {
  if(!enabled || !state.initialized || state.menu_owner) return;
  unsigned at=pc&65535;
  if(at==0xe70d || at==0xec98) {
    if(!state.door_pass && !state.scene_owner && !(at==0xec98 && g_ram[0x1f41]) &&
        state.players[state.anchor^1].status==MMX_COOP_ALIVE) {
      state.door_pass=1;state.door_s=cpu->S;state.door_d=cpu->D;state.door_entry=(uint16_t)at;
    }
    return;
  }
  if(!state.door_pass || cpu->S!=state.door_s || cpu->D!=state.door_d) return;
  if(at==0xe725 || at==0xecc7) {
    state.anchor=state.current;state.door_pass=0;begin_scene(g_ram);return;
  }
  if(state.door_pass==1) {
    MmxCoopSelect(g_ram,state.anchor^1);state.door_pass=2;
    interp_bridge_pre_opcode_redirect((pc&0xff0000)|state.door_entry);
  } else {MmxCoopSelect(g_ram,state.anchor);state.door_pass=0;}
}
static void eagle_lift_hook(CpuState *cpu,uint32_t pc) {
  if(!enabled || !state.initialized || state.menu_owner || state.scene_owner) return;
  unsigned d=cpu->D;
  if(g_ram[d+10]!=0x48 || g_ram[d+1]!=2 || g_ram[d+2]) return;
  if((pc&65535)==0xc0ae) {
    if(!state.door_pass && state.players[state.anchor^1].status==MMX_COOP_ALIVE) {
      state.door_pass=1;state.door_entry=0xc0ae;state.door_s=cpu->S;state.door_d=cpu->D;
    }
    return;
  }
  if(!state.door_pass || state.door_entry!=0xc0ae || state.door_s!=cpu->S || state.door_d!=d) return;
  /* $82:D7D7 sets .2C only for a rider. Run just that contact query for the
   * second seat, then let the original lift script own its chosen actor. */
  if(g_ram[d+0x2c]) {
    state.anchor=state.current;state.door_pass=0;begin_scene(g_ram);
  } else if(state.door_pass==1) {
    MmxCoopSelect(g_ram,state.anchor^1);state.door_pass=2;
    interp_bridge_pre_opcode_redirect(0x87c0ae);
  } else {MmxCoopSelect(g_ram,state.anchor);state.door_pass=0;}
}
static bool join_tick(uint8_t *r) {
  MmxCoopPlayer *p=&state.players[1];MmxZeroState *z=&p->zero;
  if (z->swap_phase) {
    unsigned phase=z->swap_phase;
    if (teleport_tick(r,p)) {
      if(phase==2) p->status=MMX_COOP_ABSENT;
      state.select_armed=state.select_hold=0;
    }
    r[0xb9d]=r[0xba0]=0;return true;
  }
  bool gameplay=r[0xd1]==2 && r[0xd2]==4 && r[0xd3]==4 && r[0xba9]==2 &&
      (r[0xbcf]&127) && r[0xbaa]!=12 &&
      !r[0x1f0c] && r[0x1f10]<6 && !r[0x1f23] && !r[0x1f48];
  if (!gameplay) {state.select_hold=0;return false;}
  if (!(p->input&4)) {state.select_armed=1;state.select_hold=0;}
  if (p->status==MMX_COOP_FALLEN) {state.select_hold=0;return false;}
  if (p->status==MMX_COOP_ALIVE) {
    if (!(p->body[0x27]&127) || p->body[2]==12) {state.select_hold=0;return false;}
    if(state.players[0].status==MMX_COOP_FALLEN) {state.select_hold=0;return false;}
    if ((p->input&4) && state.select_armed && ++state.select_hold>=90) {
      MmxCoopSelect(r,0);state.anchor=0;
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

static unsigned slime_bit(unsigned d) {
  return d>=0x1428 && d<0x1628 && (d-0x1428)%64==0 && g_ram[d+10]==0x19 ?
      1u<<((d-0x1428)/64) : 0;
}
static void slime_owner(unsigned bit,unsigned player) {
  if(player) state.slime_p2|=bit;else state.slime_p2&=~bit;
}
static void slime_hook(CpuState *cpu,uint32_t pc) {
  if(!enabled || !state.initialized) return;
  unsigned d=cpu->D,bit=slime_bit(d);
  if(!bit) return;
  if((pc&65535)==0xa939) {MmxCoopSelect(g_ram,state.anchor);return;}
  if(g_ram[d+2]==12 || g_ram[d+2]==14) {
    unsigned owner=(state.slime_p2&bit)!=0;
    if(state.players[owner].status==MMX_COOP_ALIVE && (state.players[owner].body[0x27]&127))
      MmxCoopSelect(g_ram,owner);
    else {g_ram[d+2]=16;g_ram[d+3]=0;} /* Native release/pop animation. */
  }
}
static void contact_hook(CpuState *cpu,uint32_t pc) {
  if (!enabled || !state.initialized) return;
  unsigned at = pc & 65535;
  /* A lone survivor still uses the native contact path without a second pass. */
  if(at==0x9b03 && !state.contact_pass) {
    unsigned bit=slime_bit(cpu->D);
    if(bit && g_ram[cpu->D+2]!=12 && g_ram[cpu->D+2]!=14) slime_owner(bit,state.current);
  }
  if(state.menu_owner || state.scene_owner || state.players[state.anchor^1].status!=MMX_COOP_ALIVE) return;
  if (at == 0x9b03 || at == 0x9b43) {
    if (state.current==state.anchor && !state.contact_pass) {
      state.contact_pass = 1; state.contact_entry = (uint16_t)at;
      state.contact_s = cpu->S; state.contact_d = cpu->D;
    }
    return;
  }
  /* A helper may return through a shared RTL; only the owning guest call's
   * balanced return boundary can complete or restart this pass. */
  if (!state.contact_pass || cpu->S != state.contact_s || cpu->D != state.contact_d) return;
  unsigned slime=state.contact_entry==0x9b03 ? slime_bit(cpu->D) : 0;
  if (state.contact_pass == 1) {
    /* Slimer's puddle can capture one actor. Keep the actual contact seat
     * for its later pin/escape states, instead of reusing the world anchor. */
    if(slime && (cpu->A&255)) {
      slime_owner(slime,state.current);state.contact_pass=0;return;
    }
    /* Retail stops the projectile scan after its first contact, including
     * immune/reflecting hits. Extend the scan to P2 only after a real miss. */
    if (state.contact_entry == 0x9b43 && at != 0x9b7d) { state.contact_pass = 0; return; }
    state.contact_a = cpu->A; state.contact_x = cpu->X; state.contact_y = cpu->Y;
    state.contact_db = cpu->DB; cpu_mirrors_to_p(cpu); state.contact_p = cpu->P;
    MmxCoopSelect(g_ram,state.anchor^1); state.contact_pass = 2;
    interp_bridge_pre_opcode_redirect(0x840000 | state.contact_entry);
  } else {
    if(slime && (cpu->A&255)) slime_owner(slime,state.current);
    bool first_hit = (state.contact_a & 255) != 0;
    bool no_second_hit = !(cpu->A & 255);
    MmxCoopSelect(g_ram,state.anchor);
    if (first_hit || no_second_hit) {
      cpu->A = state.contact_a; cpu->X = state.contact_x; cpu->Y = state.contact_y;
      cpu->DB = state.contact_db; cpu->P = state.contact_p; cpu_p_to_mirrors(cpu);
    }
    state.contact_pass = 0;
    /* A fatal contact can happen immediately before a boss tests player HP
     * and permanently disables its own damage receiver. Finish both contact
     * passes, then expose the living world actor to that native continuation. */
    select_world_survivor(g_ram);
  }
}
static bool pickup_slot(unsigned d) {
  if(d<0x1628 || d>=0x1928 || (d-0x1628)%48) return false;
  unsigned kind=g_ram[d+10];
  return kind==1 || kind==2 || kind==4 || kind==5 || kind==11;
}
static void pickup_hook(CpuState *cpu,uint32_t pc) {
  if(!enabled || !state.initialized || state.menu_owner) return;
  unsigned at=pc&65535,d=cpu->D;
  if ((pickup_slot(d) && g_ram[d]) || (state.pickup_pass && d==state.pickup_d))
    diagnostic_event(g_ram,cpu,pc,"pickup");
  if(at==0xd2ed || at==0xd31b) {MmxCoopSelect(g_ram,state.anchor);return;}
  if(at==0xd2e6 || at==0xd308) {
    if(d<0x1628 || d>=0x1928 || (d-0x1628)%48) return;
    unsigned slot=(d-0x1628)/48;
    if(!g_ram[d] || !g_ram[d+1] || !pickup_slot(d)) state.pickup_owner[slot]=0;
    if(state.pickup_owner[slot]) MmxCoopSelect(g_ram,state.pickup_owner[slot]-1);
    return;
  }
  if(at==0x9c0e) {
    if(!state.pickup_pass && !state.scene_owner && pickup_slot(d) && cpu->X==0xba8 &&
        !state.pickup_owner[(d-0x1628)/48] && state.players[state.anchor^1].status==MMX_COOP_ALIVE) {
      state.pickup_s=cpu->S;state.pickup_d=(uint16_t)d;state.pickup_pass=1;
    }
    return;
  }
  if(!state.pickup_pass || cpu->S!=state.pickup_s || d!=state.pickup_d) return;
  if(cpu->_flag_C) {
    state.pickup_owner[(d-0x1628)/48]=(uint8_t)(state.current+1);state.pickup_pass=0;
  } else if(state.pickup_pass==1 && state.current==state.anchor) {
    /* Retry only native contact, never item movement or its refill task.
     * Keep a successful collector projected through the rest of this item. */
    MmxCoopSelect(g_ram,state.anchor^1);state.pickup_pass=2;
    interp_bridge_pre_opcode_redirect(0x849c0e);
  } else {MmxCoopSelect(g_ram,state.anchor);state.pickup_pass=0;}
}
static void object_hook(CpuState *cpu, uint32_t pc) {
  if (!enabled || !state.initialized || state.menu_owner || state.scene_owner || state.players[state.anchor^1].status != MMX_COOP_ALIVE) return;
  unsigned at = pc & 65535;
  bool entry = at == 0xd2bd || at == 0xd3dd || at == 0xd3fa || at == 0xd43a || at == 0xd457 || at == 0x9d67;
  if (entry) {
    if (!state.object_pass && state.current==state.anchor) {
      state.object_pass = 1; state.object_entry = (uint16_t)at;
    }
    return;
  }
  if (state.object_pass == 1) {
    state.object_a = cpu->A; state.object_x = cpu->X; state.object_y = cpu->Y;
    state.object_s = cpu->S; state.object_d = cpu->D; state.object_db = cpu->DB;
    cpu_mirrors_to_p(cpu); state.object_p = cpu->P;
    MmxCoopSelect(g_ram,state.anchor^1); state.object_pass = 2;
    interp_bridge_pre_opcode_redirect((pc & 0xff0000) | state.object_entry);
  } else if (state.object_pass == 2) {
    MmxCoopSelect(g_ram,state.anchor);
    cpu->A = state.object_a; cpu->X = state.object_x; cpu->Y = state.object_y;
    cpu->D = state.object_d; cpu->DB = state.object_db; cpu->P = state.object_p;
    cpu_p_to_mirrors(cpu); state.object_pass = 0;
  }
}
static void platform_hook(CpuState *cpu,uint32_t pc) {
  if(!enabled || !state.initialized || state.menu_owner || state.scene_owner) return;
  unsigned at=pc&65535,d=cpu->D;
  /* Item $0E uses only .2C's boolean rider latch. In co-op retain one bit
   * per seat there, projecting a boolean while the native helper executes.
   * Slot initialization still clears it, and snapshots retain both riders. */
  if(d<0x1628 || d>=0x1928 || (d-0x1628)%48 || g_ram[d+10]!=0x0e) return;
  if(at==0xab81 || at==0xab56) {
    if(state.contact_pass) return;
    state.contact_entry=(uint16_t)at;state.contact_pass=1;
    state.contact_d=(uint16_t)d;state.contact_s=cpu->S;
    state.platform_riders=g_ram[d+0x2c]&3;
    if(at==0xab81) g_ram[d+0x2c]=(state.platform_riders>>state.current)&1;
    return;
  }
  if(!state.contact_pass || state.contact_d!=d || state.contact_s!=cpu->S ||
      (state.contact_entry!=0xab81 && state.contact_entry!=0xab56)) return;
  if(state.contact_entry==0xab81) {
    unsigned bit=1u<<state.current;
    state.platform_riders=(state.platform_riders&~bit)|(g_ram[d+0x2c]?bit:0);
  }
  if(state.contact_pass==1 && state.players[state.anchor^1].status==MMX_COOP_ALIVE &&
      (state.players[state.anchor^1].body[0x27]&127)) {
    state.contact_a=cpu->A;state.contact_x=cpu->X;state.contact_y=cpu->Y;
    state.contact_db=cpu->DB;cpu_mirrors_to_p(cpu);state.contact_p=cpu->P;
    MmxCoopSelect(g_ram,state.anchor^1);state.contact_pass=2;
    if(state.contact_entry==0xab81) g_ram[d+0x2c]=(state.platform_riders>>state.current)&1;
    interp_bridge_pre_opcode_redirect(0x840000|state.contact_entry);
    return;
  }
  g_ram[d+0x2c]=state.platform_riders;
  if(state.contact_pass==2) {
    MmxCoopSelect(g_ram,state.anchor);
    cpu->A=state.contact_a;cpu->X=state.contact_x;cpu->Y=state.contact_y;
    cpu->DB=state.contact_db;cpu->P=state.contact_p;cpu_p_to_mirrors(cpu);
  }
  state.contact_pass=state.platform_riders=0;
}
static bool shared_screen(void) {
  return enabled && state.initialized && !state.menu_owner && !state.scene_owner && state.players[0].status==MMX_COOP_ALIVE &&
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
  if ((pc&65535)==0xe12d) {
    /* The native bottom-camera clamp checks only $0BB0. Apply that same
     * signed feet threshold to the other actor before the original check.
     * $84:9F2F..9F79's fatal-hit state keeps the ordinary death controller,
     * sound and orb objects; the shared camera still runs only once. */
    if (!enabled || !state.initialized || state.menu_owner || state.scene_owner || MmxCoopTransitionActive()) return;
    MmxCoopPlayer *p=&state.players[state.anchor^1];
    if (p->status==MMX_COOP_ALIVE && (p->body[0x27]&127) &&
        (int16_t)(word(p->body+8)-32-word(g_ram))>=0) {
      ++p->body[0x30];p->body[0x2f]=8;p->body[0x26]=127;
      p->body[2]=p->body[0x6a]=12;p->body[3]=0;p->body[0x27]=128;
    }
    return;
  }
  if (!shared_screen()) return;
  unsigned axis=((pc&65535)==0xdebf || (pc&65535)==0xdeca) ? 8 : 5;
  unsigned a=word(state.players[0].body+axis),b=word(state.players[1].body+axis);
  cpu->A=(uint16_t)((a+b)/2);
  cpu->_flag_N=(cpu->A&0x8000)!=0;cpu->_flag_Z=cpu->A==0;
  cpu->P=(cpu->P&~0x82)|(cpu->_flag_N?0x80:0)|(cpu->_flag_Z?2:0);
}
static void menu_hook(CpuState *cpu,uint32_t pc) {
  if (!enabled || !state.initialized || state.scene_owner || MmxCoopTransitionActive()) return;
  switch (pc&65535) {
    case 0xe57f:
      if (state.current==1) {
        MmxCoopApplyInput(g_ram);cpu->A=(uint16_t)word(g_ram+0xbe2);
        cpu->_flag_N=(cpu->A&0x8000)!=0;cpu->_flag_Z=cpu->A==0;
        cpu->P=(cpu->P&~0x82)|(cpu->_flag_N?0x80:0)|(cpu->_flag_Z?2:0);
      }
      break;
    case 0x9e68: {
      if (state.menu_owner) return;
      unsigned seat=(state.players[state.anchor].pressed&8) ? state.anchor : state.anchor^1;
      if (state.players[seat].status!=MMX_COOP_ALIVE || !(state.players[seat].pressed&8)) return;
      state.menu_owner=(uint8_t)(seat+1);state.menu_last=(uint8_t)seat;
      MmxCoopSelect(g_ram,seat);
      MmxCoopApplyInput(g_ram);g_ram[0xbe3]|=0x10;
      break;
    }
    case 0x9eac: /* Native pause guards rejected the request. */
    case 0xc579: /* Equipment and subtank changes are now committed. */
      if(state.menu_owner) {MmxCoopSelect(g_ram,state.anchor);state.menu_owner=0;}
      break;
  }
}
static void death_hook(CpuState *cpu,uint32_t pc) {
  if(!enabled || !state.initialized || state.menu_owner) return;
  unsigned seat=state.current,other=seat^1;
  if((pc&65535)==0x9d9e) {
    /* Native reset has already cleared the actor pools. Adopt that new body
     * for P1 instead of projecting a stored corpse back over the cleared RAM. */
    state.current=state.anchor=0;state.stage_pending=1;
    state.scene_owner=state.scene_phase=state.door_pass=0;
    state.players[1].status=MMX_COOP_ABSENT;
    MmxZeroState z={0};z.active_x=state.players[0].character==MMX_COOP_X;
    MmxZeroSetState(z);MmxWeaponsSetState(state.players[0].weapons);
    MmxWeaponsCancelShots(g_ram);MmxWeaponsPartnerCombat(&state.players[1].combat);
    if(g_snes && g_snes->cart) MmxZeroSetCollisionRom(g_snes->cart->rom,g_snes->cart->romSize);
    return;
  }
  if((pc&65535)==0x9ac7) {
    /* Main stage mode 6 assumes the whole team is dead and stops input/pause
     * polling. Keep mode 4 when another player still has HP. */
    if(state.players[other].status==MMX_COOP_ALIVE && (state.players[other].body[0x27]&127))
      interp_bridge_pre_opcode_redirect((pc&0xff0000)|0x9ad9);
    return;
  }
  if(cpu->D!=0xba8) return;
  switch(pc&65535) {
    case 0x8a5c:
      state.solo_death[seat]=state.players[other].status==MMX_COOP_ALIVE &&
          (state.players[other].body[0x27]&127)!=0;
      if(state.solo_death[seat]) memcpy(state.death_flags[seat],g_ram+0x1f13,7);
      break;
    case 0x8ab7:
      if(state.solo_death[seat]) memcpy(state.death_flags[seat],g_ram+0x1f13,7);
      break;
    case 0x8a78:
    case 0x8acc:
      /* Keep the native death pose, countdown, sound and expanding orbs,
       * but do not freeze the partner or erase another task's freeze flags. */
      if(state.solo_death[seat]) memcpy(g_ram+0x1f13,state.death_flags[seat],7);
      break;
    case 0x8b0b:
      if(state.solo_death[seat]) {
        state.solo_death[seat]=0;state.players[seat].status=MMX_COOP_FALLEN;
        MmxZeroCancel(g_ram);MmxWeaponsCancelShots(g_ram);
        memset(g_ram+0xc38,0,0x1e0);g_ram[0xbb6]=0;g_ram[0xbcf]=128;
        MmxCoopCapture(g_ram);
      }
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
    if (state.initialized && state.current != state.anchor &&
        ((cpu->D >= 0xba8 && cpu->D < 0xe18) || (cpu->D >= 0x1228 && cpu->D < 0x1428)))
      interp_bridge_pre_opcode_redirect(0x82810a);
    return;
  }
  if ((pc & 0xffff) == 0x8136) {
    MmxCoopInitialize(g_ram);
    diagnostic_event(g_ram,cpu,pc,"controller-enter");
    if (state.initialized && !state.controller_pass) {
      if(state.current==1) MmxCoopApplyInput(g_ram);
      state.controller_pass=1;
    }
    if(state.initialized && refill_paused(g_ram))
      interp_bridge_pre_opcode_redirect(0x81819c);
    return;
  }
  if (!state.initialized || !state.controller_pass) return;
  diagnostic_event(g_ram,cpu,pc,"controller-return");
  constrain_player(g_ram);
  unsigned other=state.current^1;
  if (state.controller_pass == 1 && !state.menu_owner && !state.scene_owner && state.players[other].status == MMX_COOP_ALIVE &&
      g_ram[0xd1] == 2 && g_ram[0xd2] == 4 && !g_ram[0x1f0c]) {
    state.return_a = cpu->A; state.return_x = cpu->X; state.return_y = cpu->Y;
    state.return_s = cpu->S; state.return_db = cpu->DB;
    cpu_mirrors_to_p(cpu); state.return_p = cpu->P;
    if(!(g_ram[0xbcf]&127) && (state.players[other].body[0x27]&127)) state.anchor=(uint8_t)other;
    MmxCoopSelect(g_ram,other); MmxCoopApplyInput(g_ram);
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
    MmxCoopSelect(g_ram,state.anchor);
    cpu->A = state.return_a; cpu->X = state.return_x; cpu->Y = state.return_y;
    cpu->DB = state.return_db; cpu->P = state.return_p; cpu_p_to_mirrors(cpu);
  }
  MmxCoopCapture(g_ram);
  state.controller_pass = 0;
}
void MmxCoopRegisterHooks(void) {
  interp_bridge_set_pre_opcode_hook(0x87c0ae,eagle_lift_hook);
  interp_bridge_set_pre_opcode_hook(0x87c0b4,eagle_lift_hook);
  const unsigned platforms[]={0x84ab81,0x84ac34,0x84ab56,0x84ab80};
  for(unsigned i=0;i<sizeof(platforms)/sizeof(platforms[0]);++i)
    interp_bridge_set_pre_opcode_hook(platforms[i],platform_hook);
  interp_bridge_set_pre_opcode_hook(0x818136, controller_hook);
  interp_bridge_set_pre_opcode_hook(0x81819c, controller_hook);
  interp_bridge_set_pre_opcode_hook(0x848fcb, controller_hook);
  interp_bridge_set_pre_opcode_hook(0x8280df, controller_hook);
  const unsigned objects[] = {0xd2bd,0xd2dd,0xd3dd,0xd3f9,0xd3fa,0xd422,
      0xd43a,0xd456,0xd457,0xd47f,0x819d67,0x819d79};
  for (unsigned i=0;i<sizeof(objects)/sizeof(objects[0]);++i)
    interp_bridge_set_pre_opcode_hook(objects[i],object_hook);
  interp_bridge_set_pre_opcode_hook(0x83a934,slime_hook);
  interp_bridge_set_pre_opcode_hook(0x83a939,slime_hook);
  const unsigned contacts[] = {0x849b03,0x849b43,0x849b42,0x849b7d,0x849d82,
      0x849dc9,0x849dcc,0x849ee9};
  for (unsigned i=0;i<sizeof(contacts)/sizeof(contacts[0]);++i)
    interp_bridge_set_pre_opcode_hook(contacts[i],contact_hook);
  const unsigned cameras[]={0xdea0,0xdeab,0xdebf,0xdeca,0xe12d};
  for(unsigned i=0;i<sizeof(cameras)/sizeof(cameras[0]);++i)
    interp_bridge_set_pre_opcode_hook(cameras[i],camera_hook);
  interp_bridge_set_pre_opcode_hook(0x9e68,menu_hook);
  interp_bridge_set_pre_opcode_hook(0x9eac,menu_hook);
  interp_bridge_set_pre_opcode_hook(0xc579,menu_hook);
  interp_bridge_set_pre_opcode_hook(0xe57f,menu_hook);
  const unsigned pickups[]={0xd2e6,0xd2ed,0xd308,0xd31b,0x849c0e,0x849c15,0x849c1d,0x849d06};
  for(unsigned i=0;i<sizeof(pickups)/sizeof(pickups[0]);++i)
    interp_bridge_set_pre_opcode_hook(pickups[i],pickup_hook);
  const unsigned doors[]={0x81e70d,0x81e724,0x81e725,0x81ec98,0x81ecc6,0x81ecc7};
  for(unsigned i=0;i<sizeof(doors)/sizeof(doors[0]);++i)
    interp_bridge_set_pre_opcode_hook(doors[i],door_hook);
  const unsigned deaths[]={0x9d9e,0x9ac7,0x818a5c,0x818a78,0x818ab7,0x818acc,0x818b0b};
  for(unsigned i=0;i<sizeof(deaths)/sizeof(deaths[0]);++i)
    interp_bridge_set_pre_opcode_hook(deaths[i],death_hook);
}

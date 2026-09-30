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
_Static_assert(sizeof(MmxCoopState) == 4592, "Co-op save ABI");

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
      s->contact_reserved || s->contact_pass > 2) return false;
  if (s->object_pass && s->object_entry != 0xd2bd && s->object_entry != 0xd3dd &&
      s->object_entry != 0xd3fa && s->object_entry != 0xd43a && s->object_entry != 0xd457) return false;
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
  unsigned previous = r[0xbde] | r[0xbdf] << 8;
  r[0xbe0] = (uint8_t)previous; r[0xbe1] = (uint8_t)(previous >> 8);
  r[0xbde] = (uint8_t)actions; r[0xbdf] = (uint8_t)(actions >> 8);
  r[0xbe2] = (uint8_t)(actions & ~previous); r[0xbe3] = (uint8_t)((actions & ~previous) >> 8);
}
bool MmxCoopPlacePartner(uint8_t *r, uint16_t x, uint16_t y) {
  if (!enabled || !state.initialized || !r || state.current || state.players[1].status != MMX_COOP_ABSENT) return false;
  MmxCoopCapture(r);
  MmxCoopPlayer *p = &state.players[1];
  memcpy(p->body, state.players[0].body, sizeof(p->body));
  memcpy(p->auxiliaries, state.players[0].auxiliaries, 0x60); /* armor only */
  p->body[4] = p->body[7] = 0;
  p->body[5] = (uint8_t)x; p->body[6] = (uint8_t)(x >> 8);
  p->body[8] = (uint8_t)y; p->body[9] = (uint8_t)(y >> 8);
  p->body[0x27] = r[0x1f9a] | 128;
  p->body[0x33] = p->body[0x35] = p->body[0x36] = p->body[0x37] = 0;
  memset(p->body + 0x38, 0, 4);
  p->status = MMX_COOP_ALIVE;
  return true;
}

static void contact_hook(CpuState *cpu,uint32_t pc) {
  if (!enabled || !state.initialized || state.players[1].status != MMX_COOP_ALIVE) return;
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
  if (!enabled || !state.initialized || state.players[1].status != MMX_COOP_ALIVE) return;
  unsigned at = pc & 65535;
  bool entry = at == 0xd2bd || at == 0xd3dd || at == 0xd3fa || at == 0xd43a || at == 0xd457;
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
  if (state.controller_pass == 1 && state.players[1].status == MMX_COOP_ALIVE &&
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
      0xd43a,0xd456,0xd457,0xd47f};
  for (unsigned i=0;i<sizeof(objects)/sizeof(objects[0]);++i)
    interp_bridge_set_pre_opcode_hook(objects[i],object_hook);
  const unsigned contacts[] = {0x849b03,0x849b43,0x849b42,0x849b7d,0x849d82,
      0x849dc9,0x849dcc,0x849ee9};
  for (unsigned i=0;i<sizeof(contacts)/sizeof(contacts[0]);++i)
    interp_bridge_set_pre_opcode_hook(contacts[i],contact_hook);
}

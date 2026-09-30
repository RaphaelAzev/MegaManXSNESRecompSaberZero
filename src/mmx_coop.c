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
_Static_assert(sizeof(MmxCoopState) == 4560, "Co-op save ABI");

bool MmxCoopEnabled(void) { return enabled; }
void MmxCoopReset(void) {
  memset(&state, 0, sizeof(state));
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
      s->reserved[0] || s->reserved[1]) return false;
  for (unsigned i = 0; i < 2; ++i) {
    const MmxCoopPlayer *p = &s->players[i];
    if (p->character > MMX_COOP_ZERO || p->status > MMX_COOP_FALLEN ||
        p->input > 4095 || p->pressed > 4095 || p->reserved[0] || p->reserved[1] ||
        !MmxWeaponsValidState(&p->weapons) || !MmxWeaponsValidCombatState(&p->combat) ||
        !MmxZeroValidState(&p->zero)) return false;
    if (s->initialized && p->zero.active_x != (p->character == MMX_COOP_X)) return false;
    for (unsigned n = 0; n < 4; ++n) if (p->subtanks[n] > 14) return false;
    for (unsigned n = 1; n < 16; n += 2) if (p->energy[n] > 28) return false;
  }
  return s->players[0].character != s->players[1].character;
}
void MmxCoopSetState(const MmxCoopState *s) {
  if (enabled && MmxCoopValidState(s)) state = *s;
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

static void controller_hook(CpuState *cpu, uint32_t pc) {
  if (!enabled) return;
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
    state.controller_pass = 2;
    cpu->P |= 0x30; cpu_p_to_mirrors(cpu); cpu->X &= 255; cpu->Y &= 255;
    /* Re-enter AFTER PHP/PHD/PLD. Both passes share exactly one prologue and
     * epilogue, so no synthetic JSL or extra stack frame is needed. */
    interp_bridge_pre_opcode_redirect(0x818136);
    return;
  }
  if (state.controller_pass == 2) {
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
}

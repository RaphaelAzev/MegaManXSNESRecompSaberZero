/* Private ROM-backed controller/context checks. No owner save is overwritten. */
#include "mmx_coop.h"
static unsigned coop_x(const MmxCoopPlayer *p) { return p->body[5] | p->body[6] << 8; }
static unsigned coop_y(const MmxCoopPlayer *p) { return p->body[8] | p->body[9] << 8; }
static void coop_frames(unsigned count, unsigned p1, unsigned p2) {
  while (count--) frame(p1 | p2 << 12);
}
static void coop_checks(const char *assets, const char *fixture,
                        uint8_t *start, uint8_t *expected, uint8_t *actual, size_t cap) {
  check(assets && fixture, "co-op check requires private source assets and a grounded fixture");
  check(MmxZeroLoad(assets), "co-op uses original X3 Zero assets");
  MmxZeroRegisterHooks(); MmxCoopRegisterHooks();
  for (unsigned character = 0; character < 2; ++character) {
    MmxCoopDisable();
    check(RtlLoadSnapshot(fixture), "private co-op fixture loads");
    check(MmxCoopEnable(character), "either starting character is accepted");
    coop_frames(8,0,0);
    MmxCoopState s = MmxCoopGetState();
    check(s.initialized && s.players[0].character == character && s.players[1].character == (character ^ 1),
          "P1 choice assigns the counterpart to P2");
    check(s.players[0].zero.active_x == (character == MMX_COOP_X), "native context uses chosen P1");
    unsigned x0 = coop_x(&s.players[0]), y0 = coop_y(&s.players[0]);
    check(MmxCoopPlacePartner(g_ram, x0+32, y0), "test places the second actor on known clear ground");
    unsigned world_tick = g_ram[0xb9c];
    coop_frames(20,0,SNES_PAD_RIGHT);
    s = MmxCoopGetState();
    fprintf(stderr,"co-op roster=%u initial=%u p1=%u p2=%u pass=%u input=%04x native=%02x/%02x\n",
      character,x0,coop_x(&s.players[0]),coop_x(&s.players[1]),s.controller_pass,s.players[1].input,
      s.players[1].body[0x36],s.players[1].body[0x37]);
    check(coop_x(&s.players[0]) == x0 && coop_x(&s.players[1]) > x0+50,
          "P2 moves using port 2 while P1 stays still");
    check((uint8_t)(g_ram[0xb9c]-world_tick) == 20, "world frame counter advances once per two-player frame");
    check(s.current == 0 && !s.controller_pass, "second controller restores native P1 context and stack path");
    unsigned p2x = coop_x(&s.players[1]);
    coop_frames(8,SNES_PAD_LEFT,SNES_PAD_B);
    s = MmxCoopGetState();
    check(coop_x(&s.players[0]) < x0 && coop_x(&s.players[1]) == p2x && coop_y(&s.players[1]) < y0,
          "P1 walks independently while P2 jumps with original character physics");
    check(MmxCoopValidState(&s), "both native player contexts validate");
    size_t n = RtlSaveSnapshotToMemory(start,cap);
    check(n != 0, "both players save in the native snapshot");
    coop_frames(12,SNES_PAD_RIGHT,SNES_PAD_LEFT);
    size_t en = RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n), "co-op snapshot restores");
    coop_frames(12,SNES_PAD_RIGHT,SNES_PAD_LEFT);
    size_t an = RtlSaveSnapshotToMemory(actual,cap);
    same(expected,en,actual,an,"two-port replay preserves both actors and world state exactly");
    s = MmxCoopGetState(); s.players[1].character = s.players[0].character;
    check(!MmxCoopValidState(&s), "malformed duplicate-character roster is rejected");
  }
  MmxCoopDisable();
  puts("MMX CO-OP CONTROLLER FOUNDATION CHECKS PASSED");
}

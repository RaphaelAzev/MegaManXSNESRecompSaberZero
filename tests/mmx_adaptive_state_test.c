/* ROM-backed checks for the MMX adapters and the real shared desktop host.
 * The caller supplies an empty working directory; only slot 12 is used. */
#define MMX_DESKTOP_ENTRY MmxDesktopMain
#include "desktop/host_main.c"
#include MMX_GAME_MAIN
#include "common/launcher_binds.h"
#include "mmx_weapons.h"

static void check(int ok, const char *what) {
  if (!ok) { fprintf(stderr, "FAIL: %s\n", what); exit(1); }
  printf("ok: %s\n", what);
}
static void frame(unsigned input) {
  MmxBeforeFrame();
  RtlRunFrame(input | (1u << 30));
  CaptureSimulationFrame(1);
}
static void replay(int count) {
  for (int i = 0; i < count; ++i) frame(i < count / 2 ? SNES_PAD_RIGHT : 0);
}
static void same(const void *a, size_t an, const void *b, size_t bn, const char *what) {
  if (an != bn || memcmp(a, b, an)) {
    size_t i = 0;
    while (i < an && i < bn && ((const uint8 *)a)[i] == ((const uint8 *)b)[i]) ++i;
    fprintf(stderr, "first difference at %zu / %zu / %zu\n", i, an, bn);

  }
  check(an && an == bn && !memcmp(a, b, an), what);
}
static void zero_replay(int n) { for (int i=0;i<n;++i) frame(0); }
static void rewind_six_frames(void) {
  /* The framework harness requests interval 1; the game's default is 6. */
  for (int i=0;i<6 && snes_rewind_selected_seconds() < 0.0999f;++i)
    snes_rewind_step(-1);
}
static unsigned zero_projectiles(unsigned kind) {
  unsigned count = 0;
  for (unsigned d=0x1228;d<0x1428;d+=64) count += g_ram[d] && g_ram[d+10] == kind;
  return count;
}
static void zero_capture(const char *base, const char *suffix) {
  if (!base) return;
  char path[4096]; snprintf(path, sizeof(path), "%s%s", base, suffix);
  check(MmxRendererSaveCapture(path), "Zero renderer capture saved");
}
static void zero_motion_checks(const char *fixture) {
  const char *path = getenv("MMX_ZERO_MOTION_REFERENCE");
  if (!path) path = MMX_ZERO_MOTION_REFERENCE_DEFAULT;
  FILE *f = fopen(path,"r"); check(f != NULL,"X3 movement reference opens");
  char line[256], previous[32] = ""; unsigned x0 = 0, y0 = 0, checked = 0;
  check(fgets(line,sizeof(line),f) != NULL,"X3 movement reference header");
  while (fgets(line,sizeof(line),f)) {
    char name[32], keys[32]; int tick,x,y,vx,vy,action,sub,pose,group,ground,visibility;
    check(sscanf(line,"%31[^,],%d,%31[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
          name,&tick,keys,&x,&y,&vx,&vy,&action,&sub,&pose,&group,&ground,&visibility) == 13,
          "X3 movement reference row");
    if (strcmp(name,previous)) {
      check(RtlLoadSnapshot(fixture),"restore movement fixture");
      zero_replay(10); g_ram[0xbac] = g_ram[0xbaf] = 0;
      x0 = g_ram[0xbad] << 8 | g_ram[0xbae] << 16;
      y0 = g_ram[0xbb0] << 8 | g_ram[0xbb1] << 16;
      snprintf(previous,sizeof(previous),"%s",name);
    }
    unsigned input = strstr(keys,"Right") ? SNES_PAD_RIGHT : 0;
    if (strchr(keys,'A')) input |= SNES_PAD_A;
    if (strchr(keys,'B')) input |= SNES_PAD_B;
    frame(input);
    int dx = (int)(g_ram[0xbac] | g_ram[0xbad] << 8 | g_ram[0xbae] << 16) - (int)x0;
    int dy = (int)(g_ram[0xbaf] | g_ram[0xbb0] << 8 | g_ram[0xbb1] << 16) - (int)y0;
    MmxZeroState z = MmxZeroGetState();
    if (dx != x || dy != y || (int16_t)(g_ram[0xbc2] | g_ram[0xbc3] << 8) != vx ||
        (int16_t)(g_ram[0xbc4] | g_ram[0xbc5] << 8) != vy ||
        (tick > 1 && (!z.anim_valid || z.anim_pose != pose))) {
      fprintf(stderr,"X3 reference mismatch %s/%d: xy %d,%d vs %d,%d pose %u vs %d valid %u\n",
              name,tick,dx,dy,x,y,z.anim_pose,pose,z.anim_valid); exit(1);
    }
    ++checked;
  }
  fclose(f); printf("ok: %u original-X3 movement/animation reference frames\n",checked);
}
static void zero_combat_checks(const char *fixture) {
  FILE *f=fopen(MMX_ZERO_COMBAT_REFERENCE_DEFAULT,"r"); check(f!=NULL,"X3 combat reference opens");
  char line[160],previous[16]=""; unsigned count=0; int y0=0;
  check(fgets(line,sizeof(line),f)!=NULL,"X3 combat reference header");
  while(fgets(line,sizeof(line),f)) {
    char mode[16],phase[24],keys[16]; int tick,pose,dy,vy,emitted;
    check(sscanf(line,"%15[^,],%23[^,],%d,%15[^,],%d,%d,%d,%d",mode,phase,&tick,keys,&pose,&dy,&vy,&emitted)==8,"X3 combat reference row");
    if(strcmp(mode,previous)) {
      check(RtlLoadSnapshot(fixture),"restore combat fixture");
      g_ram[0xbaf]=0xed; /* Original fixture's fractional Y, including landing snap. */
      for(unsigned i=0;i<205;++i) frame(SNES_PAD_Y);
      y0=g_ram[0xbaf] | g_ram[0xbb0]<<8 | g_ram[0xbb1]<<16;
      snprintf(previous,sizeof(previous),"%s",mode);
    }
    unsigned input=strchr(keys,'Y')?SNES_PAD_Y:0;
    if(strchr(keys,'B')) input|=SNES_PAD_B;
    unsigned before=zero_projectiles(3);
    frame(input);
    MmxZeroState z=MmxZeroGetState();
    int actual_pose=(int)((MmxZeroPose(g_ram,&z)-MmxZeroMenuPose())/(128*128));
    int actual_y=(int)(g_ram[0xbaf] | g_ram[0xbb0]<<8 | g_ram[0xbb1]<<16)-y0;
    int actual_vy=(int16_t)(g_ram[0xbc4]|g_ram[0xbc5]<<8);
    if(actual_pose!=pose || actual_y!=dy || actual_vy!=vy ||
       (emitted && zero_projectiles(3)<=before)) {
      fprintf(stderr,"combat mismatch %s/%s/%d pose %d/%d dy %d/%d vy %d/%d emitted %u/%d burst %u/%u\n",
        mode,phase,tick,actual_pose,pose,actual_y,dy,actual_vy,vy,zero_projectiles(3)>before,emitted,z.burst,z.burst_end);
      fprintf(stderr,"native action=%u sub=%u pose=%u seqbase=%u mirror=%u/%u/%u\n",g_ram[0xbaa],g_ram[0xbab],g_ram[0xbbf],g_ram[0xc17],z.anim_offset,z.anim_pose,z.anim_timer);
      exit(1);
    }
    ++count;
  }
  fclose(f); printf("ok: %u original-X3 combat animation/physics reference frames\n",count);
}
static void zero_menu_pixels(void) {
  static uint32_t pixels[256 * 224]; unsigned body = 0, icon = 0;
  static uint32_t baseline[48 * 56]; static bool have_baseline;
  check(MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false),"Zero menu renders");
  const uint32_t *stock = MmxRendererStockFrame(); check(stock != NULL,"menu stock reference exists");
  for (int y = 0; y < 224; ++y) for (int x = 0; x < 256; ++x) if (pixels[y * 256 + x] != stock[y * 256 + x]) {
    if (x >= 104 && x < 152 && y >= 128 && y < 184) ++body;
    else if (x >= 192 && x < 216 && y >= 139 && y < 163) ++icon;
    else { fprintf(stderr,"Unexpected Zero menu change at %d,%d\n",x,y); exit(1); }
  }
  check(body > 100 && icon == 0,"menu replaces body and preserves X's original life head exactly");
  bool same_body = true;
  for (int y = 128; y < 184; ++y) for (int x = 104; x < 152; ++x) {
    unsigned at = (y - 128) * 48 + x - 104;
    if (have_baseline) same_body &= pixels[y * 256 + x] == baseline[at];
    else baseline[at] = pixels[y * 256 + x];
  }
  check(same_body,"armor ownership leaves the same Zero menu body");
  have_baseline = true;
}
static void zero_weapon_checks(const char *fixture, const char *capture) {
  for (unsigned weapon = 0; weapon <= 8; ++weapon) for (unsigned charged = 0; charged < 2; ++charged) {
    check(RtlLoadSnapshot(fixture),"restore weapon fixture");
    if (weapon) {
      g_ram[0x1f85 + weapon * 2] = 0; g_ram[0x1f86 + weapon * 2] = 0xdc;
      for (int i = 0; i < 6; ++i) frame(SNES_PAD_R);
      zero_replay(1); check(g_ram[0xbdb] == weapon * 2,"native switch selects requested X1 weapon");
    }
    if (charged) {
      g_ram[0x1f99] = 2;
      for (int i = 0; i < 181; ++i) frame(SNES_PAD_Y);
      if (!weapon) zero_capture(capture,".charge.cap");
    }
    unsigned seen = 0;
    for (int i = 0; i < (charged && weapon == 3 ? 95 : 35); ++i) {
      frame(!charged && i == 0 ? SNES_PAD_Y : 0);
      for (unsigned d = 0x1228; d < 0x1428; d += 64) if (g_ram[d] && g_ram[d + 10] < 32)
        seen |= 1u << g_ram[d + 10];
      if (i == 0 && !charged && !weapon) {
        unsigned d = 0x1228;
        check((int)(g_ram[d+5] | g_ram[d+6]<<8) - (int)(g_ram[0xbad] | g_ram[0xbae]<<8) == 27 &&
              (int)(g_ram[d+8] | g_ram[d+9]<<8) - (int)(g_ram[0xbb0] | g_ram[0xbb1]<<8) == -7,
              "standing shot uses original Zero pose 31 muzzle, translated to X1 feet");
      }
      if (i == 2) { char suffix[64]; snprintf(suffix,sizeof(suffix),".weapon%u-%u.cap",weapon,charged); zero_capture(capture,suffix); }
      if (i == 26 && charged && (weapon == 2 || weapon == 3)) {
        char suffix[64]; snprintf(suffix,sizeof(suffix),".effect%u.cap",weapon); zero_capture(capture,suffix);
      }
      if (i == 32 && charged && weapon == 2) zero_capture(capture,".chameleon-next.cap");
      if (i == 84 && charged && weapon == 3) zero_capture(capture,".shield.cap");
      if (i == 0 && !charged && weapon == 5)
        check((int)(g_ram[0x122d] | g_ram[0x122e]<<8) - (int)(g_ram[0xbad] | g_ram[0xbae]<<8) == 22,
              "Tornado starts six pixels farther along Zero's facing direction");
      if (i == 0 && !charged && weapon == 7)
        check((int)(g_ram[0x1230] | g_ram[0x1231]<<8) - (int)(g_ram[0xbb0] | g_ram[0xbb1]<<8) == -8,
              "Boomerang launches above X's original origin");
      if (i == 20 && charged && weapon == 3)
        check((int)(g_ram[0x1230] | g_ram[0x1231]<<8) - (int)(g_ram[0xbb0] | g_ram[0xbb1]<<8) == -6,
              "Rolling Shield collision and art follow Zero's raised center");
    }
    unsigned expected_kind = weapon ? weapon + (charged ? 15 : 6) : charged ? 3 : 0;
    check(seen & (1u << expected_kind),"X1 weapon produces its expected normal/charged projectile class");
    if (weapon) check((g_ram[0x1f85 + weapon*2] | (g_ram[0x1f86 + weapon*2] & 63)<<8) < 0x1c00,
                      "X1 weapon spends native energy");
  }
  check(RtlLoadSnapshot(fixture),"restore for left-facing muzzle");
  frame(SNES_PAD_LEFT); frame(0); frame(SNES_PAD_Y);
  check((int)(g_ram[0x122d] | g_ram[0x122e]<<8) - (int)(g_ram[0xbad] | g_ram[0xbae]<<8) == -27 &&
        (int)(g_ram[0x1230] | g_ram[0x1231]<<8) - (int)(g_ram[0xbb0] | g_ram[0xbb1]<<8) == -7,
        "native facing mirrors Zero muzzle without changing its height");
}
static void zero_menu_transition_checks(const char *fixture, const char *capture, unsigned armor) {
  check(RtlLoadSnapshot(fixture),"restore for menu transition probe");
  g_ram[0x1f99] = (uint8_t)armor;
  bool repairs = g_mmx_render_asset_repairs; g_mmx_render_asset_repairs = false;
  static uint32_t pixels[256 * 224]; unsigned previous = ~0u;
  for (unsigned direction = 0; direction < 2; ++direction) for (unsigned i = 0; i < 120; ++i) {
    frame(i == 0 ? SNES_PAD_START : 0);
    check(MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false),"menu transition renders");
    MmxRenderStats stats = MmxRendererGetStats();
    unsigned changed = 0, lit = 0;
    const uint32_t *stock = MmxRendererStockFrame();
    /* Keep background repair out of the comparison. Only the character can
     * change this rectangle; the HUD badge and life head are outside it. */
    for (int y = 80; y < 184; ++y) for (int x = 104; x < 152; ++x) {
      unsigned p = y * 256 + x;
      changed += pixels[p] != stock[p]; lit += stock[p] != 0;
    }
    if (lit && changed < 20) {
      fprintf(stderr,"menu missing Zero %u/%u: changed %u lit %u\n",direction,i,changed,lit);
      char suffix[80]; snprintf(suffix,sizeof(suffix),".missing%u-%u.cap",direction,i);zero_capture(capture,suffix);
      check(false,"visible menu-transition body remains Zero");
    }
    unsigned signature = g_ram[0xd3] | g_ram[0xd4] << 8 | g_ram[0x1f10] << 16 | g_ram[0xc3] << 24;
    if (signature != previous) {
      fprintf(stderr,"menu transition %u/%u state %08x custom %u fallback %u\n",direction,i,signature,stats.custom_lines,stats.fallback_lines);
      char suffix[80]; snprintf(suffix,sizeof(suffix),".transition%u-%u.cap",direction,i);zero_capture(capture,suffix);
      previous = signature;
    }
  }
  g_mmx_render_asset_repairs = repairs;
}
static void zero_swap_checks(const char *fixture, uint8 *start, uint8 *expected, uint8 *actual, size_t cap) {
  check(RtlLoadSnapshot(fixture),"restore for grounded Select swap");
  zero_replay(10);
  frame(SNES_PAD_Y); frame(0);
  check(zero_projectiles(0)!=0,"live native projectile participates in frozen-world check");
  uint8_t objects[0x1d08-0xe18], camera[6], position[6];
  memcpy(camera,g_ram+0x1e4d,6); memcpy(position,g_ram+0xbac,6);
  unsigned hp=g_ram[0xbcf], weapon=g_ram[0xbdb], upgrades=g_ram[0x1f99];
  const char *capture=getenv("MMX_ZERO_TEST_CAPTURE");
  for(unsigned direction=0;direction<2;++direction) {
    frame(SNES_PAD_SELECT);
    if(!MmxZeroSwapping()) fprintf(stderr,"swap gates: %02x %02x %02x %02x %02x %02x; %02x %02x %02x %02x; vx %02x%02x\n",
        g_ram[0xd1],g_ram[0xd2],g_ram[0xba9],g_ram[0xbaa],g_ram[0xbab],g_ram[0xbb6],
        g_ram[0x1f0c],g_ram[0x1f10],g_ram[0x1f23],g_ram[0x1f48],g_ram[0xbc3],g_ram[0xbc2]);
    check(MmxZeroSwapping(),"grounded Select starts exchange");
    memcpy(objects,g_ram+0xe18,sizeof(objects));
    unsigned phases=0, ticks=0;
    while(MmxZeroSwapping() && ticks++<160) {
      MmxZeroState s=MmxZeroGetState();
      if(!(phases&(1u<<s.swap_phase))) {
        char suffix[64]; snprintf(suffix,sizeof(suffix),".swap%u-phase%u.cap",direction,s.swap_phase);
        zero_capture(capture,suffix); phases|=1u<<s.swap_phase;
      }
      if(ticks==12) {
        size_t n=RtlSaveSnapshotToMemory(start,cap);
        zero_replay(5); size_t en=RtlSaveSnapshotToMemory(expected,cap);
        check(RtlLoadSnapshotFromMemory(start,n),"mid-teleport state loads");
        zero_replay(5); size_t an=RtlSaveSnapshotToMemory(actual,cap);
        same(expected,en,actual,an,"mid-teleport replay preserves phase and frozen world");
      }
      check(!memcmp(objects,g_ram+0xe18,sizeof(objects)) && !memcmp(camera,g_ram+0x1e4d,6),
            "enemies items scripts and camera remain frozen");
      frame(direction ? SNES_PAD_SELECT : 0);
    }
    check(!MmxZeroSwapping() && phases==0x7e,"all six teleport phases finish");
    check(MmxZeroActive()==(direction!=0),"exchange changes playable character");
    check(!memcmp(position,g_ram+0xbac,6) && g_ram[0xbcf]==hp &&
        g_ram[0xbdb]==weapon && g_ram[0x1f99]==upgrades,"swap preserves feet health weapon and equipment");
    check(MmxZeroUpgradeBits(0x81971c,0)==(direction?8:0),"X and Zero retain separate dash/charge capabilities");
    check(g_snes->cart->rom[0x32555]==(direction?18:14),"terrain/damage bounds follow active character");
    if(direction) {
      frame(SNES_PAD_SELECT); frame(SNES_PAD_SELECT);
      check(!MmxZeroSwapping() && MmxZeroActive(),"holding Select cannot retrigger an exchange");
    }
    zero_replay(3);
    char suffix[48]; snprintf(suffix,sizeof(suffix),".swap%u-done.cap",direction); zero_capture(capture,suffix);
  }
  frame(SNES_PAD_B); frame(SNES_PAD_SELECT);
  check(!MmxZeroSwapping(),"Select cannot swap in midair");
  check(RtlLoadSnapshot(fixture),"restore after Select checks");
}
static void zero_health_swap(void) {
  frame(SNES_PAD_SELECT);
  check(MmxZeroSwapping(),"HP exchange begins");
  for(unsigned i=0;i<160 && MmxZeroSwapping();++i) frame(0);
  check(!MmxZeroSwapping(),"HP exchange completes");
  zero_replay(3);
}
static void zero_health_pickup(unsigned small) {
  /* Retail health actor (item kind 2), collected through native collision. */
  memset(g_ram+0x1628,0,48); g_ram[0x1628]=1;
  g_ram[0x1632]=2; g_ram[0x1633]=(uint8_t)(128|small);
  memcpy(g_ram+0x162d,g_ram+0xbad,2);
  unsigned y=(g_ram[0xbb0] | g_ram[0xbb1]<<8)-16;
  g_ram[0x1630]=(uint8_t)y; g_ram[0x1631]=(uint8_t)(y>>8);
  zero_replay(55);
  check(!g_ram[0x1628],"native health pickup finishes collection");
}
static void zero_health_checks(const char *fixture, uint8 *start, uint8 *expected, uint8 *actual, size_t cap) {
  check(RtlLoadSnapshot(fixture),"restore for separate HP");
  zero_replay(10);
  g_ram[0xbcf]=7|128; frame(0);
  check(MmxZeroGetState().hp[0]==7 && MmxZeroGetState().hp[1]==16,"damage affects only active Zero");
  zero_health_swap();
  check(!MmxZeroActive() && (g_ram[0xbcf]&127)==16,"X arrives with his independent HP");
  g_ram[0xbcf]=9|128; frame(0);
  zero_health_pickup(1);
  check((g_ram[0xbcf]&127)==11 && MmxZeroGetState().hp[0]==7,"small health pickup heals X only");
  zero_health_pickup(0);
  check((g_ram[0xbcf]&127)==16 && MmxZeroGetState().hp[0]==7,"large health pickup clamps X without healing Zero");
  zero_health_pickup(1);
  check(MmxZeroGetState().hp[0]==7,"full active pool never redirects pickup healing to reserve");
  zero_health_swap();
  check(MmxZeroActive() && (g_ram[0xbcf]&127)==7,"Zero returns with his previous HP");
  g_ram[0x1f9a]=18; g_ram[0xbcf]=9|128; frame(0);
  check(MmxZeroGetState().hp_max==18 && MmxZeroGetState().hp[1]==16,"heart-tank maximum is shared without refilling reserve HP");
  size_t n=RtlSaveSnapshotToMemory(start,cap);
  zero_health_swap(); size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"separate HP save loads");
  zero_health_swap(); size_t an=RtlSaveSnapshotToMemory(actual,cap);
  same(expected,en,actual,an,"separate HP and exchange replay deterministically");
  unsigned lives=g_ram[0x1f80];
  /* State produced by native lethal damage at $84:9D60. Let the real death
   * animation, life decrement and checkpoint initializer perform the rest. */
  g_ram[0xbcf]=128; g_ram[0xbaa]=0x0c; g_ram[0xbab]=0;
  bool respawned=false;
  for(unsigned i=0;i<1000;++i) {
    frame(0); MmxZeroState s=MmxZeroGetState();
    if(g_ram[0x1f80]+1==lives && s.hp_valid && s.hp[0]==18 && s.hp[1]==18 && g_ram[0xba9]==2) {
      respawned=true; break;
    }
  }
  check(respawned,"native death loses one life and respawn refills both HP pools");
  check(RtlLoadSnapshot(fixture),"restore after separate HP checks");
}
static void weapon_menu_checks(const char *assets, const char *fixture, uint8 *start,
                               uint8 *expected, uint8 *actual, size_t cap) {
  check(MmxWeaponsLoad(assets), "original X2/X3 weapon cache loads");
  check(RtlLoadSnapshot(fixture), "legacy gameplay fixture initializes new weapons");
  for (unsigned i=0;i<16;++i) check(MmxWeaponsGetState().energy[i]==28,"new weapons unlocked with full energy");
  uint8 inventory[16]; memcpy(inventory,g_ram+0x1f88,16);
  frame(SNES_PAD_START); zero_replay(75);
  check(MmxWeaponsMenuVisible(g_ram),"native pause opens for weapon pages");
  for (unsigned page=1;page<=2;++page) {
    frame(SNES_PAD_R); zero_replay(3);
    check(MmxWeaponsGetState().menu_page==page,"R advances extended weapon page");
    for (unsigned i=1;i<=4;++i) {
      frame(SNES_PAD_DOWN); zero_replay(3);
      check(g_ram[0x1ed2]==i,"left-column extended weapon selectable while X1 is locked");
    }
    frame(SNES_PAD_UP); zero_replay(2); frame(SNES_PAD_RIGHT); zero_replay(2);
    check(g_ram[0x1ed2]==8,"native horizontal cursor selects extended right column");
    for (unsigned i=7;i>=5;--i) {
      frame(SNES_PAD_UP); zero_replay(3);
      check(g_ram[0x1ed2]==i,"right-column extended weapon selectable while X1 is locked");
    }
    char suffix[40]; snprintf(suffix,sizeof(suffix),".x%u-menu.cap",page+1);
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),suffix);
    frame(SNES_PAD_START); zero_replay(75);
    MmxWeaponsState s=MmxWeaponsGetState();
    check(s.page==page && s.weapon==5 && g_ram[0xbdb]==0,"pause confirms extended selection with safe native buster resources");
    check(!memcmp(inventory,g_ram+0x1f88,16),"extended selection preserves native X1 progression and energy");
    if (page==1) {
      frame(SNES_PAD_START); zero_replay(75);
      check(MmxWeaponsGetState().menu_page==1 && g_ram[0x1ed2]==5,"pause reopens at extended selection");
      frame(SNES_PAD_LEFT); zero_replay(3); check(!g_ram[0x1ed2],"native cursor returns to buster");
    }
  }
  MmxWeaponsState s=MmxWeaponsGetState(); s.energy[0]=7; s.energy[15]=13; MmxWeaponsSetState(s);
  size_t n=RtlSaveSnapshotToMemory(start,cap);
  frame(SNES_PAD_START); zero_replay(75); frame(SNES_PAD_R); zero_replay(3);
  check(!MmxWeaponsGetState().menu_page,"R wraps to original X1 page");
  check(!g_ram[0x1ed2],"return to locked X1 page selects available buster");
  size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"weapon selection and partial energy restore");
  s=MmxWeaponsGetState(); check(s.page==2 && s.weapon==5 && s.energy[0]==7 && s.energy[15]==13,"independent weapon state survives save/load");
  frame(SNES_PAD_START); zero_replay(75); frame(SNES_PAD_R); zero_replay(3);
  size_t an=RtlSaveSnapshotToMemory(actual,cap);
  same(expected,en,actual,an,"weapon-menu deterministic replay");
  frame(SNES_PAD_START); zero_replay(75);
  check(!MmxWeaponsActive() && !g_ram[0xbdb],"X1 buster selection exits extended weapon mode");
  check(!memcmp(inventory,g_ram+0x1f88,16),"page cycle and save/load leave original inventory intact");
  zero_health_swap(); check(MmxZeroGetState().active_x,"weapon-page test switches to X");
  frame(SNES_PAD_START); zero_replay(75); frame(SNES_PAD_L); zero_replay(3);
  check(MmxWeaponsGetState().menu_page==2,"L wraps backward from X1 to X3 for X");
  frame(SNES_PAD_DOWN); zero_replay(3);
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".x-menu.cap");
  frame(SNES_PAD_START); zero_replay(75);
  check(MmxZeroGetState().active_x && MmxWeaponsGetState().page==2 &&
        MmxWeaponsGetState().weapon==1 && !g_ram[0xbdb],"X selects an extended weapon through native pause");
  puts("MMX X2/X3 WEAPON MENU CHECKS PASSED");
}
static void zero_state_checks(const char *assets, const char *fixture, uint8 *start,
                              uint8 *expected, uint8 *actual, size_t cap) {
  check(fixture != NULL && MmxZeroLoad(assets), "Zero local assets load");
  MmxZeroRegisterHooks();
  check(RtlLoadSnapshot(fixture), "Zero gameplay fixture loads");
  MmxZeroCancel(g_ram);
  check(g_ram[0xba9] == 2 && g_ram[0xbaa] == 0 && g_ram[0xbdb] == 0 &&
        !g_ram[0x1f99] && (g_ram[0xbd3] & 4), "fixture is standing, unupgraded buster");
  g_config.widescreen = false;
  int w,h; MmxPrepareFrame(1280,720,&w,&h);
  check(w == 256 && g_mmx_custom_renderer, "Zero activates native-width compositor");
  const char *weapons=getenv("MMX_WEAPONS_TEST_ASSETS");
  if (weapons) { weapon_menu_checks(weapons,fixture,start,expected,actual,cap); return; }
  zero_swap_checks(fixture,start,expected,actual,cap);
  if(getenv("MMX_ZERO_SWAP_ONLY")) { puts("MMX SELECT SWAP CHECKS PASSED"); return; }
  zero_health_checks(fixture,start,expected,actual,cap);
  if(getenv("MMX_ZERO_HEALTH_ONLY")) { puts("MMX CHARACTER HP CHECKS PASSED"); return; }
  zero_motion_checks(fixture);
  zero_combat_checks(fixture);
  check(RtlLoadSnapshot(fixture), "restore for burst state replay");
  for(unsigned i=0;i<201;++i) frame(SNES_PAD_Y);
  zero_replay(4);
  size_t burst_n=RtlSaveSnapshotToMemory(start,cap);
  zero_replay(8); size_t burst_en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,burst_n),"load during first windup");
  zero_replay(8); size_t burst_an=RtlSaveSnapshotToMemory(actual,cap);
  same(expected,burst_en,actual,burst_an,"windup replay emits exactly the same shot");
  size_t burst_rn=RtlRollbackSaveToMemory(start,cap);
  zero_replay(14); size_t burst_ren=RtlRollbackSaveToMemory(expected,cap);
  check(RtlRollbackLoadFromMemory(start,burst_rn),"load live beam and stored follow-ups");
  zero_replay(14); burst_an=RtlRollbackSaveToMemory(actual,cap);
  same(expected,burst_ren,actual,burst_an,"live beam and stored combo rollback replay");
  /* v4/v5 had only the 12/18-byte prefix. Both must still load, with the
   * former full-charge combo interpreted as saber-capable. */
  burst_n=RtlSaveSnapshotToMemory(start,cap);
  size_t chunk=0;
  for(size_t i=burst_n-8;i>8;--i) {
    uint32_t magic; memcpy(&magic,start+i,4);
    if(magic==0x4d4d5854u) { chunk=i; break; }
  }
  check(chunk!=0,"Zero game chunk located");
  for(uint32_t v=4;v<=5;++v) {
    memcpy(start+chunk+4,&v,4);
    size_t prefix=v==4?MMX_ZERO_LEGACY_STATE_SIZE:MMX_ZERO_ANIMATION_STATE_SIZE;
    check(RtlLoadSnapshotFromMemory(start,burst_n-sizeof(MmxZeroState)+prefix),"legacy Zero state prefix loads");
    check(MmxZeroGetState().combo==1 && MmxZeroGetState().saber_ready && !MmxZeroGetState().burst,"legacy stored combo migrates");
  }
  uint32_t v6=6; memcpy(start+chunk+4,&v6,4);
  check(RtlLoadSnapshotFromMemory(start,burst_n-sizeof(MmxZeroState)+MMX_ZERO_COMBAT_STATE_SIZE),
        "pre-swap v6 save retains combat prefix");
  check(MmxZeroActive() && !MmxZeroSwapping(),"pre-swap save defaults to Zero without an exchange");
  uint32_t v7=7; memcpy(start+chunk+4,&v7,4);
  check(RtlLoadSnapshotFromMemory(start,burst_n-sizeof(MmxZeroState)+MMX_ZERO_SWAP_STATE_SIZE),
        "pre-HP v7 save retains character/swap prefix");
  check(!MmxZeroGetState().hp_valid,"pre-HP save seeds separate pools from native HP on first update");
  check(RtlLoadSnapshot(fixture), "restore for combo probe");
  for (int i=0;i<201;++i) frame(SNES_PAD_Y);
  frame(0);
  check(MmxZeroGetState().combo == 1 && MmxZeroGetState().saber_ready &&
        !zero_projectiles(3), "full charge begins original windup and stores two follow-ups");
  zero_replay(17); frame(SNES_PAD_Y); zero_replay(9);
  check(zero_projectiles(3) == 2 && MmxZeroGetState().combo == 2, "two native charged shots coexist after second windup");
  for (unsigned i=0;i<160 && (MmxZeroGetState().burst || MmxZeroGetState().shot_mask || g_ram[0xc25]);++i) frame(0);
  check(!MmxZeroGetState().burst && !MmxZeroGetState().shot_mask && !g_ram[0xc25], "beams and their disappearance effects retire");
  frame(SNES_PAD_Y); zero_replay(12);
  MmxZeroState state = MmxZeroGetState();
  check(state.slash > 7 && state.projectile && !state.combo, "saber reaches active frames");
  check(g_ram[0x1f99] == 0, "combo does not grant X1 upgrades");
  const char *capture = getenv("MMX_ZERO_TEST_CAPTURE");
  zero_capture(capture, "");
  size_t n = RtlSaveSnapshotToMemory(start,cap);
  zero_replay(10); size_t en = RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n), "mid-saber memory load");
  size_t an = RtlSaveSnapshotToMemory(actual,cap);
  same(start,n,actual,an,"mid-saber complete immediate restore");
  zero_replay(10); an = RtlSaveSnapshotToMemory(actual,cap);
  same(expected,en,actual,an,"mid-saber deterministic replay");
  check(RtlLoadSnapshotFromMemory(start,n), "restore before rollback probe");
  size_t rn = RtlRollbackSaveToMemory(start,cap);
  zero_replay(8); en = RtlRollbackSaveToMemory(expected,cap);
  check(RtlRollbackLoadFromMemory(start,rn), "mid-saber rollback load");
  zero_replay(8); an = RtlRollbackSaveToMemory(actual,cap);
  same(expected,en,actual,an,"mid-saber rollback replay");
  check(RtlRollbackLoadFromMemory(start,rn), "restore before rewind probe");
  snes_rewind_configure();
  for (int i=0;i<12;++i) { frame(0); snes_rewind_note_frame(); }
  en = RtlSaveSnapshotToMemory(expected,cap);
  for (int i=0;i<6;++i) { frame(0); snes_rewind_note_frame(); }
  check(snes_rewind_open(), "mid-saber rewind opens");
  rewind_six_frames(); snes_rewind_commit();
  an = RtlSaveSnapshotToMemory(actual,cap);
  same(expected,en,actual,an,"rewind restores complete mid-saber state");
  snes_rewind_shutdown();
  zero_replay(60);
  check(!MmxZeroGetState().slash && !MmxZeroGetState().projectile && !g_ram[0xbdd], "saber and projectile counts retire");
  check(RtlLoadSnapshot(fixture), "restore for dash probe");
  unsigned x = g_ram[0xbad] | g_ram[0xbae] << 8;
  for (int i=0;i<8;++i) frame(SNES_PAD_A | SNES_PAD_RIGHT);
  unsigned dx = (g_ram[0xbad] | g_ram[0xbae] << 8) - x;
  check(dx >= 24 && !g_ram[0x1f99], "unupgraded dash moves at dash speed");
  zero_capture(capture, ".dash.cap");
  check(RtlLoadSnapshot(fixture), "restore for aerial saber probe");
  unsigned floor = g_ram[0xbb0] | g_ram[0xbb1] << 8;
  MmxZeroSetState((MmxZeroState){.combo=2,.saber_ready=1});
  for (int i=0;i<6;++i) frame(SNES_PAD_B);
  frame(SNES_PAD_B | SNES_PAD_Y); zero_replay(12);
  check(MmxZeroGetState().air && MmxZeroGetState().slash > 7 &&
        (g_ram[0xbb0] | g_ram[0xbb1] << 8) < floor, "aerial saber retains jump and gravity");
  zero_capture(capture, ".air.cap");
  zero_replay(100);
  check((g_ram[0xbb0] | g_ram[0xbb1] << 8) == floor && !MmxZeroGetState().slash,
        "original-size Zero lands on the same floor");
  check(RtlLoadSnapshot(fixture), "restore for special weapon probe");
  /* Fixture inventory grant only: production code never grants weapons. */
  g_ram[0x1f87] = 0; g_ram[0x1f88] = 0xdc;
  for (int i=0;i<6;++i) frame(SNES_PAD_R);
  zero_replay(1);
  check(g_ram[0xbdb] == 2, "native weapon switch selects Homing Torpedo");
  frame(SNES_PAD_Y); zero_replay(8);
  check(zero_projectiles(7) && g_ram[0x1f88] != 0xdc, "normal Torpedo spawns and spends energy");
  for (int i=0;i<181;++i) frame(SNES_PAD_Y);
  frame(0); zero_replay(8);
  check(!zero_projectiles(0x10) && !g_ram[0x1f99], "charged special remains unavailable without arms");
  zero_replay(60); g_ram[0x1f99] = 2;
  for (int i=0;i<181;++i) frame(SNES_PAD_Y);
  frame(0); zero_replay(8);
  check(zero_projectiles(0x10) == 5, "arm upgrade permits five charged Torpedo projectiles");
  zero_capture(capture, ".torpedo.cap");
  check(RtlLoadSnapshot(fixture), "restore for life pickup probe");
  /* Spawn the retail 1-up actor in a copied test fixture. Collection and the
   * life counter still execute the original game's collision/update path. */
  memset(g_ram + 0x1628,0,48); g_ram[0x1628] = 1; g_ram[0x1632] = 4; g_ram[0x1633] = 128;
  unsigned life_x = (g_ram[0xbad] | g_ram[0xbae] << 8) + 48;
  unsigned life_y = (g_ram[0xbb0] | g_ram[0xbb1] << 8) - 24;
  g_ram[0x162d] = (uint8_t)life_x; g_ram[0x162e] = (uint8_t)(life_x >> 8);
  g_ram[0x1630] = (uint8_t)life_y; g_ram[0x1631] = (uint8_t)(life_y >> 8);
  zero_replay(20); check(g_ram[0x163e] == 0x11,"retail life pickup submits its head animation");
  zero_capture(capture, ".life.cap");
  unsigned lives = g_ram[0x1f80];
  memcpy(g_ram + 0xbad,g_ram + 0x162d,2); memcpy(g_ram + 0xbb0,g_ram + 0x1630,2);
  zero_replay(8); check(g_ram[0x1f80] == lives + 1,"Zero collects a 1-up through native collision");
  check(RtlLoadSnapshot(fixture), "restore for pause menu probe");
  frame(SNES_PAD_START); zero_replay(120);
  zero_capture(capture, ".menu.cap"); zero_menu_pixels();
  check(RtlLoadSnapshot(fixture), "restore for upgraded menu probe");
  g_ram[0x1f99] = 15; frame(SNES_PAD_START); zero_replay(120);
  zero_capture(capture, ".menu-armor.cap"); zero_menu_pixels();
  zero_menu_transition_checks(fixture,capture,0);
  zero_menu_transition_checks(fixture,NULL,15);
  zero_weapon_checks(fixture,capture);
  MmxZeroDisable(); MmxBeforeFrame(); MmxPrepareFrame(1280,720,&w,&h);
  check(!g_mmx_custom_renderer && !MmxZeroEnabled(), "disabling returns to stock presentation");
  puts("MMX ZERO RUNTIME CHECKS PASSED");
}
int main(int argc, char **argv) {
  check(argc == 2 || argc == 3, "ROM supplied");
  SDL_SetMainReady();
  check(snesrecomp_sdl_init(SDL_INIT_EVENTS), "SDL initializes");
  g_audio_mutex = SDL_CreateMutex();
  static const SnesDesktopHostGame game = {
    .native_widescreen = 0, .state_menu_hotkeys = 1,
    .prepare_frame = MmxPrepareFrame, .begin_sim_frame = MmxBeginFrame,
    .end_sim_frame = MmxEndFrame, .draw_frame = MmxDrawFrame,
  };
  g_game = &game;
  ConfigUseStateMenuDefaults();
  FILE *f = fopen("config.ini", "w");
  fputs("[Graphics]\nDisplayAspect=8:7\n[KeyMap]\nLoad = F1,F2,F3,F4,F5,F6,F7,F8,F9,F10\n", f); fclose(f);
  ParseConfigFile("config.ini");
  /* Exercise the game adapter with the actual parsed settings, including a
   * setting change between frames. Pure renderer tests cannot catch a host
   * that forgets to pass Display Aspect into adaptive geometry. */
  g_config.widescreen = true;
  g_mmx_custom_aspect = MMX_ASPECT_ADAPTIVE;
  const int display_widths[] = {342, 398, 456};
  check(g_config.display_aspect == kSnesDisplayAspect_SquarePixels8x7, "Display Aspect parsed");
  for (int i = 0; i < 3; ++i) {
    int setting = (i + 1) % 3;
    /* The launcher updates the live setting after the initial config load. */
    if (i) g_config.display_aspect = SnesDisplayAspect_Clamp(setting);
    int w, h;
    MmxPrepareFrame(1920, 1080, &w, &h);
    check(w == display_widths[setting] && h == 224, "adaptive honors Display Aspect");
    SnesDisplayViewport viewport;
    MmxViewport(w, h, 1920, 1080, &viewport);
    check(viewport.width == 1920 && viewport.height == 1080, "adaptive fills matching window");
    g_mmx_custom_aspect = MMX_ASPECT_16_9;
    MmxPrepareFrame(3840, 1080, &w, &h);
    MmxViewport(w, h, 3840, 1080, &viewport);
    check(w == display_widths[setting] && viewport.width == 1920 && viewport.x == 960,
          "fixed view honors pixel shape and boxes wider window");
    g_config.widescreen = false;
    MmxPrepareFrame(1920, 1080, &w, &h);
    MmxViewport(w, h, 1920, 1080, &viewport);
    const int native_widths[] = {1440, 1234, 1080};
    check(w == 256 && viewport.width == native_widths[setting], "native view retains Display Aspect");
    g_config.widescreen = true;
    g_mmx_custom_aspect = MMX_ASPECT_ADAPTIVE;
  }
  g_config.display_aspect = kSnesDisplayAspect_Crt4x3;
  check(FindCmdForSdlKey(SDLK_F7, 0) == kKeys_SaveStateMenu &&
        FindCmdForSdlKey(SDLK_F8, 0) == kKeys_Rewind &&
        FindCmdForSdlKey(SDLK_F11, 0) == kKeys_Load + 6 &&
        FindCmdForSdlKey(SDLK_F12, 0) == kKeys_Load + 7, "F7/F8 and legacy config migration");
  HandleInput(SDLK_F7, 0, true); HandleInput(SDLK_F8, 0, true);
  check(g_savestate_menu_hotkey && g_rewind_hotkey, "host dispatches F7/F8");
  g_savestate_menu_hotkey = g_rewind_hotkey = 0;
  LauncherModel model = {0};
  g_launcher_config_path = "config.ini";
  launcher_binds_set_hotkey(&model, LNG_HK_SAVE_STATE_MENU, SDLK_F9, KMOD_CTRL);
  launcher_binds_set_hotkey(&model, LNG_HK_REWIND, SDLK_F9, 0);
  ConfigReloadKeyMap("config.ini");
  check(FindCmdForSdlKey(SDLK_F9, KMOD_CTRL) == kKeys_SaveStateMenu &&
        FindCmdForSdlKey(SDLK_F9, 0) == kKeys_Rewind, "launcher rebind and slot collision");
  launcher_binds_set_hotkey(&model, LNG_HK_REWIND, 0, 0);
  ConfigReloadKeyMap("config.ini");
  check(!FindCmdForSdlKey(SDLK_F8, 0) &&
        FindCmdForSdlKey(SDLK_F9, 0) != kKeys_Rewind, "launcher clear persists");
  g_launcher_config_path = NULL;
  f = fopen(argv[1], "rb"); check(f != NULL, "ROM opens");
  fseek(f, 0, SEEK_END); long rom_size = ftell(f); rewind(f);
  uint8 *rom = malloc(rom_size);
  check(fread(rom, 1, rom_size, f) == (size_t)rom_size, "ROM reads"); fclose(f);
  if ((rom_size & 0x7fff) == 512) {
    rom_size -= 512;
    memmove(rom, rom + 512, rom_size);
  }
  g_config.new_renderer = true; g_config.widescreen = true;
  g_mmx_custom_renderer = true;
  MmxRendererSetRom(rom, rom_size);
  g_last_drawable_width = 1280; g_last_drawable_height = 720;
  g_ppu_render_flags = kPpuRenderFlags_NewRenderer;
  RtlRegisterGame(&kMmxGameInfo);
  check(SnesInit(rom, rom_size) != NULL, "game initializes");
  g_spc_player = SmwSpcPlayer_Create();
  g_spc_player->initialize(g_spc_player);
  MkDir("saves");
  size_t cap = 2u * 1024u * 1024u;
  uint8 *start = malloc(cap), *expected = malloc(cap), *actual = malloc(cap);
  const char *zero_assets = getenv("MMX_ZERO_TEST_ASSETS");
  const char *zero_title = getenv("MMX_ZERO_TITLE_FIXTURE");
  if (zero_assets && zero_title) {
    check(MmxZeroLoad(zero_assets),"title Zero assets load");
    MmxZeroRegisterHooks();
    check(RtlLoadSnapshot(zero_title),"private title fixture loads");
    zero_replay(45);
    const char *capture=getenv("MMX_ZERO_TEST_CAPTURE");
    zero_capture(capture,".title-idle.cap");
    check(g_ram[0xba9]==2 && g_ram[0xbbf]==0,"title cursor is standing player");
    frame(SNES_PAD_START);
    bool title_shot=false;
    for(unsigned i=0;i<40;++i) {
      frame(0);
      title_shot |= g_ram[0x1229]==2 && g_ram[0x123e]==0x0e;
      if(i==4 || i==9 || i==19) {
        char suffix[64]; snprintf(suffix,sizeof(suffix),".title-shot%u.cap",i+2);
        zero_capture(capture,suffix);
      }
    }
    check(title_shot,"title confirmation preserves native full-buster projectile");
    puts("MMX ZERO TITLE CHECKS PASSED"); return 0;
  }
  if (zero_assets) {
    zero_state_checks(zero_assets, getenv("MMX_ZERO_TEST_FIXTURE"), start, expected, actual, cap);
    return 0;
  }
  if (argc == 3) {
    check(RtlLoadSnapshot("saves/save11.sav"), "file loads in new process");
    replay(10);
    size_t n = RtlSaveSnapshotToMemory(actual, cap);
    f = fopen("expected.bin", "rb"); check(f != NULL, "reference opens");
    size_t en = fread(expected, 1, cap, f); fclose(f);
    same(expected, en, actual, n, "new-process replay");
    puts("MMX STATE CHECKS PASSED"); return 0;
  }
  for (int phase = 0; phase < 2; ++phase) {
    for (int i = 0; i < 600; ++i) frame(i == 200 || i == 350 ? SNES_PAD_START : 0);
    size_t n = RtlSaveSnapshotToMemory(start, cap);
    replay(30); size_t en = RtlSaveSnapshotToMemory(expected, cap);
    check(RtlLoadSnapshotFromMemory(start, n), "memory load");
    size_t an = RtlSaveSnapshotToMemory(actual, cap);
    same(start, n, actual, an, "complete immediate restore");
    replay(30); an = RtlSaveSnapshotToMemory(actual, cap);
    same(expected, en, actual, an, "30-frame deterministic replay");
  }
  snes_savestate_menu_poll_open(SNES_PAD_SELECT | SNES_PAD_R);
  check(snes_savestate_menu_is_open(), "save browser opens");
  uint64 clock = g_cpu.master_cycles;
  snes_savestate_menu_handle_key(SDLK_EQUALS, 0);
  SDL_Event key = {0};
  key.type = SDL_KEYDOWN;
#if SNESRECOMP_SDL3
  key.key.key = SDLK_s;
#else
  key.key.keysym.sym = SDLK_s;
#endif
  SDL_PushEvent(&key);
  bool running = true;
  PumpOverlayEvents(&running, snes_savestate_menu_handle_key);
  check(OverlayNavInputs() & SNES_PAD_X, "browser receives keyboard save action");
  snes_savestate_menu_poll_nav(OverlayNavInputs(), 1);
  HandleInput(SDLK_s, 0, false);
  check(clock == g_cpu.master_cycles, "saving does not advance guest");
  snes_savestate_menu_poll_nav(0, 2);
  snes_savestate_menu_poll_nav(SNES_PAD_B, 3);
  check(!snes_savestate_menu_is_open(), "B closes save browser");
  replay(10); size_t en = RtlSaveSnapshotToMemory(expected, cap);
  f = fopen("expected.bin", "wb"); fwrite(expected, 1, en, f); fclose(f);
  snes_rewind_configure();
  for (int i = 0; i < 12; ++i) { frame(0); snes_rewind_note_frame(); }
  size_t n = RtlSaveSnapshotToMemory(start, cap);
  for (int i = 0; i < 6; ++i) { frame(SNES_PAD_RIGHT); snes_rewind_note_frame(); }
  check(snes_rewind_open(), "rewind opens");
  rewind_six_frames();
  snes_rewind_commit();
  size_t an = RtlSaveSnapshotToMemory(actual, cap);
  same(start, n, actual, an, "rewind restores selected frame");
  snes_rewind_shutdown();
  /* Both meanings of the old game chunk v2 must load without reading EOF.
   * File and memory use the same format discrimination. */
  n = RtlSaveSnapshotToMemory(start, cap);
  size_t chunk = 0;
  for (size_t i = n - 8; i > 8; --i) {
    uint32_t magic; memcpy(&magic, start + i, 4);
    if (magic == 0x4d4d5854u) { chunk = i; break; }
  }
  check(chunk != 0, "game chunk located");
  uint32_t version = 2; memcpy(start + chunk + 4, &version, 4);
  check(RtlLoadSnapshotFromMemory(start, n), "shared-host v2 with execution tail loads");
  f = fopen("main-v2.sav", "wb"); fwrite(start, 1, n, f); fclose(f);
  check(RtlLoadSnapshot("main-v2.sav"), "shared-host v2 file loads");
  check(RtlLoadSnapshotFromMemory(start, chunk + 464), "adaptive v2 without tail loads");
  f = fopen("adaptive-v2.sav", "wb"); fwrite(start, 1, chunk + 464, f); fclose(f);
  check(RtlLoadSnapshot("adaptive-v2.sav"), "adaptive v2 file loads");
  replay(10);
  check(g_mmx_custom_renderer && g_mmx_custom_view.width > 256, "adaptive remains active");
  puts("MMX STATE CHECKS PASSED");
  return 0;
}

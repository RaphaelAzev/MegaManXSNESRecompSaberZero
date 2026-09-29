static void weapon_silk_checks(const char *assets,const char *fixture,uint8 *start,
                              uint8 *expected,uint8 *actual,size_t cap) {
  check(MmxWeaponsLoad(assets),"Silk Shot original material assets load");
  check(MmxWeaponsPose(1,3,72,5) && MmxWeaponsPose(1,3,72,13) &&
      MmxWeaponsPose(1,3,73,19) && MmxWeaponsPose(1,3,74,26),"all five original Silk materials are extracted");
  check(memcmp(MmxWeaponsGroupPalette(1,3,72),MmxWeaponsGroupPalette(1,3,73),32) &&
      memcmp(MmxWeaponsGroupPalette(1,3,72),MmxWeaponsGroupPalette(1,3,74),32),
      "leaves and crystal use their original distinct palettes");
  for(unsigned character=0;character<2;++character) {
    check(RtlLoadSnapshot(fixture),"restore Silk Shot fixture");if(character) zero_health_swap();
    MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=3;MmxWeaponsSetState(w);
    frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
    for(unsigned i=0;i<8;++i) if(c.shots[i].active) {slot=i;break;}
    check(slot<8 && c.shots[slot].vx==512 && c.shots[slot].vy<0 && c.shots[slot].variant==1 &&
        MmxWeaponsEnergyAmount(1,3)==27*256,"normal Silk launches the metal form on the highway for one energy");
    frame(SNES_PAD_Y);frame(0);check(extended_shots(false)==1,"source normal Silk firing limit is one");
    size_t n=RtlSaveSnapshotToMemory(start,cap);zero_replay(18);size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n),"restore normal Silk arc");zero_replay(18);
    size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"normal Silk arc and animation replay exactly");
    unsigned bounces=0;bool fragments=false;int previous_vy=MmxWeaponsGetCombatState().shots[slot].vy;
    for(unsigned i=0;i<170;++i) {
      frame(SNES_PAD_RIGHT);c=MmxWeaponsGetCombatState();
      if(c.shots[slot].active && c.shots[slot].muzzle_pose!=3) {
        bounces+=previous_vy>0 && c.shots[slot].vy<0;previous_vy=c.shots[slot].vy;
      }
      unsigned scattered=0;for(unsigned j=0;j<8;++j) scattered+=c.shots[j].active && c.shots[j].muzzle_pose==3;
      if(scattered && !fragments) {
        fragments=true;zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-silk-fragments.cap":".zero-silk-fragments.cap");
      }
    }
    check(bounces>=1 && fragments && !g_ram[0xbdd],"normal Silk bounces on X1 ground, scatters four pieces and cleans up");
    for(unsigned i=0;i<220;++i) frame(SNES_PAD_Y);frame(0);
    check(!extended_shots(true),"Silk gathering requires X1 arms");
    check(RtlLoadSnapshot(fixture),"restore charged Silk fixture");if(character) zero_health_swap();
    w=MmxWeaponsGetState();w.page=1;w.weapon=3;MmxWeaponsSetState(w);g_ram[0x1f99]|=2;
    bool gathering=false;
    for(unsigned i=0;i<220;++i) {
      frame(SNES_PAD_Y);c=MmxWeaponsGetCombatState();
      for(unsigned j=0;j<8;++j) if(c.shots[j].active && c.shots[j].charged && !c.shots[j].muzzle_pose) gathering=true;
    }
    c=MmxWeaponsGetCombatState();unsigned held=0;
    for(unsigned i=0;i<8;++i) held+=c.shots[i].active && c.shots[i].charged && c.shots[i].muzzle_pose==1;
    check(gathering && held==8 && MmxWeaponsEnergyAmount(1,3)==27*256,
        "full charge draws eight original pieces into a held cluster without spending charged energy yet");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-silk-held.cap":".zero-silk-held.cap");
    n=RtlSaveSnapshotToMemory(start,cap);for(unsigned i=0;i<12;++i) frame(SNES_PAD_Y|SNES_PAD_RIGHT);
    en=RtlSaveSnapshotToMemory(expected,cap);check(RtlLoadSnapshotFromMemory(start,n),"restore held Silk cluster");
    for(unsigned i=0;i<12;++i) frame(SNES_PAD_Y|SNES_PAD_RIGHT);
    an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"moving held Silk formation replays exactly");
    frame(0);frame(0);c=MmxWeaponsGetCombatState();unsigned flying=0;
    for(unsigned i=0;i<8;++i) flying+=c.shots[i].active && c.shots[i].charged && c.shots[i].muzzle_pose==2 && c.shots[i].vx==1024;
    check(flying==8 && MmxWeaponsEnergyAmount(1,3)==25*256 && !(g_ram[0xc2f]&64),
        "release launches eight pieces at source speed for two energy and stops charge audio");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-silk-charged.cap":".zero-silk-charged.cap");
    bool burst=false;
    for(unsigned i=0;i<105;++i) {frame(0);c=MmxWeaponsGetCombatState();for(unsigned j=0;j<8;++j) burst|=c.shots[j].active && c.shots[j].muzzle_pose==3;}
    check(burst && !g_ram[0xbdd] && !extended_shots(true),"charged Silk disperses on X1 terrain and releases native slots");
  }
  check(RtlLoadSnapshot(fixture),"restore first-full-tick Silk release");
  MmxWeaponsState edge=MmxWeaponsGetState();edge.page=1;edge.weapon=3;MmxWeaponsSetState(edge);g_ram[0x1f99]|=2;
  for(unsigned i=0;i<250 && g_ram[0xc03]!=1;++i) frame(SNES_PAD_Y);
  check(g_ram[0xc03]==1 && !extended_shots(true),"native full charge is reached before the gathering hook");
  frame(0);frame(0);
  check(extended_shots(true)==8 && MmxWeaponsEnergyAmount(1,3)==25*256,
        "releasing on the first full-charge tick launches all pieces without losing the charged shot");
  MmxWeaponsCancelShots(g_ram);
  check(RtlLoadSnapshot(fixture),"restore leaf-material fixture");
  MmxWeaponsState leaves=MmxWeaponsGetState();leaves.page=1;leaves.weapon=3;MmxWeaponsSetState(leaves);
  frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState leaf=MmxWeaponsGetCombatState();unsigned leafslot=8;
  for(unsigned i=0;i<8;++i) if(leaf.shots[i].active) {leafslot=i;leaf.shots[i].age=0;break;}
  check(leafslot<8,"leaf-material source actor available");MmxWeaponsSetCombatState(leaf);
  /* Choose Chameleon's material at initialization only. Keep the actual
   * highway terrain fixture and native stage scripts intact. */
  unsigned stage=g_ram[0x1f7a];g_ram[0x1f7a]=2;MmxWeaponsProjectileTick(g_ram,0x1228+leafslot*64,1);g_ram[0x1f7a]=(uint8_t)stage;
  leaf=MmxWeaponsGetCombatState();check(leaf.shots[leafslot].variant==3 && leaf.shots[leafslot].group==73 &&
      leaf.shots[leafslot].pose==19,"Chameleon environment selects original green leaves");
  bool trail=false,fall=false;
  for(unsigned i=0;i<95;++i) {
    frame(0);leaf=MmxWeaponsGetCombatState();
    for(unsigned j=0;j<16;++j) trail|=leaf.effects[j].active && leaf.effects[j].weapon==3;
    for(unsigned j=0;j<8;++j) fall|=leaf.shots[j].active && leaf.shots[j].muzzle_pose==3;
    if(i==24) zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".silk-leaves.cap");
  }
  leaf=MmxWeaponsGetCombatState();unsigned particles=0;for(unsigned i=0;i<16;++i) particles+=leaf.effects[i].active;
  check(trail && fall && !particles && !g_ram[0xbdd],"leaf burst curves, sheds original animated leaves and expires without slot leaks");
  /* Native collision, rather than a direct HP edit, must invoke the port. */
  check(RtlLoadSnapshot(fixture),"restore Silk enemy encounter");unsigned victim=0;
  for(unsigned i=0;i<400 && !victim;++i) {
    frame(SNES_PAD_RIGHT);
    for(unsigned d=0xe68;d<0x1228;d+=64)
      if(g_ram[d] && (g_ram[d+0x27]&127) && (g_ram[d+0x20]|g_ram[d+0x21])) {victim=d;break;}
  }
  check(victim!=0,"native enemy available for Silk collision");
  MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=3;MmxWeaponsSetState(w);
  frame(0);frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
  for(unsigned i=0;i<8;++i) if(c.shots[i].active) {
    c.shots[i].x=(g_ram[victim+5]|g_ram[victim+6]<<8)*256;c.shots[i].y=(g_ram[victim+8]|g_ram[victim+9]<<8)*256;
    c.shots[i].vx=c.shots[i].vy=0;slot=i;break;
  }
  check(slot<8,"Silk is available for native contact");g_ram[victim+0x27]=32;MmxWeaponsSetCombatState(c);
  zero_replay(3);check((g_ram[victim+0x27]&127)<=27,"normal Silk deals five source-scaled buster hits through native collision");
  check(extended_shots(false)>1,"native enemy contact disperses normal Silk");
  MmxWeaponsCancelShots(g_ram);
  puts("MMX SILK SHOT CHECKS PASSED");
}

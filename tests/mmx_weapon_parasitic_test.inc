static unsigned parasitic_test_slot(MmxWeaponCombatState c,unsigned variant) {
  for(unsigned i=0;i<8;++i) if(c.shots[i].active && c.shots[i].page==2 &&
      c.shots[i].weapon==2 && c.shots[i].variant==variant) return i;
  return 8;
}
static unsigned parasitic_test_count(MmxWeaponCombatState c,unsigned variant) {
  unsigned count=0;for(unsigned i=0;i<8;++i) if(c.shots[i].active &&
      c.shots[i].page==2 && c.shots[i].weapon==2 && c.shots[i].variant==variant) ++count;
  return count;
}
static void weapon_parasitic_checks(const char *assets,const char *fixture,uint8 *start,
                                  uint8 *expected,uint8 *actual,size_t cap) {
  check(MmxWeaponsLoad(assets),"Parasitic Bomb original graphics load");
  for(unsigned character=0;character<2;++character) {
    check(RtlLoadSnapshot(fixture),"restore Parasitic Bomb fixture");if(character) zero_health_swap();
    MmxWeaponsState w=MmxWeaponsGetState();w.page=2;w.weapon=2;MmxWeaponsSetState(w);
    frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=parasitic_test_slot(c,0);
    check(slot<8 && c.shots[slot].vx==768 && MmxWeaponsEnergyAmount(2,2)==27*256,
        "normal bomb uses source three-pixel speed and one-energy cost");
    int x=c.shots[slot].x;zero_replay(8);c=MmxWeaponsGetCombatState();
    check(c.shots[slot].x==x+8*768,"normal bomb travels straight at source speed");
    zero_replay(110);for(unsigned i=0;i<330;++i) frame(SNES_PAD_Y);frame(0);zero_replay(3);
    check(!parasitic_test_count(MmxWeaponsGetCombatState(),2),"Parasitic Bomb charge requires X1 arms");
    MmxWeaponsCancelShots(g_ram);zero_replay(90);MmxWeaponsRefill();g_ram[0x1f99]|=2;
    for(unsigned i=0;i<330;++i) frame(SNES_PAD_Y);
    c=MmxWeaponsGetCombatState();check(parasitic_test_count(c,2)==4 && MmxWeaponsEnergyAmount(2,2)==27*256,
        "full charge creates four free searching cursors while fire remains held");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-parasitic-search.cap":".zero-parasitic-search.cap");
    size_t n=RtlSaveSnapshotToMemory(start,cap);
    for(unsigned i=0;i<30;++i) frame(SNES_PAD_Y|SNES_PAD_RIGHT);
    size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n),"restore searching cursors");
    for(unsigned i=0;i<30;++i) frame(SNES_PAD_Y|SNES_PAD_RIGHT);
    size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"moving cursors replay exactly");
    frame(0);zero_replay(2);check(!parasitic_test_count(MmxWeaponsGetCombatState(),2) && !g_ram[0xbdd],
        "releasing fire removes idle cursors and frees firing slots");
  }
  check(RtlLoadSnapshot(fixture),"restore real Parasitic Bomb encounter");unsigned victim=0;
  for(unsigned i=0;i<400 && !victim;++i) {
    frame(SNES_PAD_RIGHT);
    for(unsigned d=0xe68;d<0x1228;d+=64) if(g_ram[d] && g_ram[d+1]==2 &&
        (g_ram[d+0x27]&127) && g_ram[d+0x28]<6 && (g_ram[d+0x20]|g_ram[d+0x21])) {victim=d;break;}
  }
  check(victim!=0,"real native enemy found for bomb capture");
  uint8_t enemy[64];memcpy(enemy,g_ram+victim,64);
  MmxWeaponsState w=MmxWeaponsGetState();w.page=2;w.weapon=2;MmxWeaponsSetState(w);
  frame(0);frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=parasitic_test_slot(c,0);
  check(slot<8,"normal bomb available for native collision");
  unsigned capture_x=(g_ram[0xbad]|g_ram[0xbae]<<8)+64,capture_y=(g_ram[0xbb0]|g_ram[0xbb1]<<8)-32;
  g_ram[victim+5]=(uint8_t)capture_x;g_ram[victim+6]=(uint8_t)(capture_x>>8);
  g_ram[victim+8]=(uint8_t)capture_y;g_ram[victim+9]=(uint8_t)(capture_y>>8);
  c.shots[slot].x=(g_ram[victim+5]|g_ram[victim+6]<<8)*256;
  c.shots[slot].y=(g_ram[victim+8]|g_ram[victim+9]<<8)*256;MmxWeaponsSetCombatState(c);
  g_ram[victim+0x27]=16;zero_replay(3);c=MmxWeaponsGetCombatState();
  check(MmxWeaponsMovedEnemy(&c,victim) && c.shots[slot].variant==1,"native collision captures an ordinary enemy");
  uint8_t action[3];memcpy(action,g_ram+victim+1,3);zero_replay(15);
  check(!memcmp(action,g_ram+victim+1,3) && (g_ram[victim+0x27]&127)==16,
      "captured host keeps its HP and suspends its native AI during formation");
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".parasitic-capture.cap");
  size_t n=RtlSaveSnapshotToMemory(start,cap);zero_replay(30);size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"restore captured enemy");zero_replay(30);
  size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"captured enemy formation and motion replay exactly");
  c=MmxWeaponsGetCombatState();check(c.shots[slot].active && c.shots[slot].muzzle_pose,
      "original formation animation finishes and activates the payload");
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".parasitic-payload.cap");
  n=RtlSaveSnapshotToMemory(start,cap);
  unsigned second=victim==0xe68?0xea8:0xe68;
  memcpy(g_ram+second,enemy,64);g_ram[second+0x27]=64;
  unsigned sx=(unsigned)(c.shots[slot].x>>8)+4,sy=(unsigned)(c.shots[slot].y>>8);
  g_ram[second+5]=(uint8_t)sx;g_ram[second+6]=(uint8_t)(sx>>8);
  g_ram[second+8]=(uint8_t)sy;g_ram[second+9]=(uint8_t)(sy>>8);
  zero_replay(3);check((g_ram[second+0x27]&127)<64 && g_ram[victim+1]==4,
      "captured payload damages another native enemy and consumes its host");
  check(RtlLoadSnapshotFromMemory(start,n),"restore payload before cancellation");
  MmxWeaponsCancelShots(g_ram);check(g_ram[victim+1]==4 && !g_ram[victim+0x26],
      "payload cleanup uses native death and clears contact damage");
  check(RtlLoadSnapshot(fixture),"restore charged bomb target encounter");
  w=MmxWeaponsGetState();w.page=2;w.weapon=2;MmxWeaponsSetState(w);g_ram[0x1f99]|=2;
  for(unsigned i=0;i<330;++i) frame(SNES_PAD_Y);
  memcpy(g_ram+victim,enemy,64);g_ram[victim+0x27]=16;
  unsigned ex=(g_ram[0x1e4d]|g_ram[0x1e4e]<<8)+170,ey=(g_ram[0x1e50]|g_ram[0x1e51]<<8)+120;
  g_ram[victim+5]=(uint8_t)ex;g_ram[victim+6]=(uint8_t)(ex>>8);
  g_ram[victim+8]=(uint8_t)ey;g_ram[victim+9]=(uint8_t)(ey>>8);g_ram[victim+0x28]=6;
  for(unsigned i=0;i<6;++i) frame(SNES_PAD_Y);
  c=MmxWeaponsGetCombatState();bool idle=true;
  for(unsigned i=0;i<8;++i) if(c.shots[i].active && c.shots[i].variant==2 && c.shots[i].muzzle_pose) idle=false;
  check(idle,"charged cursors exclude protected enemy categories");g_ram[victim+0x28]=enemy[0x28];
  unsigned bee=8;
  for(unsigned i=0;i<120 && bee==8;++i) {frame(SNES_PAD_Y);bee=parasitic_test_slot(MmxWeaponsGetCombatState(),3);}
  check(bee<8 && MmxWeaponsEnergyAmount(2,2)==26*256,"locked cursor launches a homing unit for one energy");
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".parasitic-launch.cap");
  bool impact=false;
  for(unsigned i=0;i<100 && !impact;++i) {
    frame(0);c=MmxWeaponsGetCombatState();unsigned burst=parasitic_test_slot(c,4);
    if(burst<8) {impact=true;check(c.shots[burst].group==23,"homing unit uses X3's shared impact animation");}
  }
  check(impact && (g_ram[victim+0x27]&127)<16,"homing unit reaches and damages a real native target");
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".parasitic-impact.cap");
  n=RtlSaveSnapshotToMemory(start,cap);zero_replay(160);en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"restore homing unit");zero_replay(160);
  an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"homing unit, collision and cursor recovery replay exactly");
  check(!parasitic_test_count(MmxWeaponsGetCombatState(),2) && !g_ram[0xbdd],
      "busy cursors finish and clean up after fire is released");
  check(MmxWeaponsEnergyAmount(2,2)==26*256,"released cursors cannot launch additional units");
  puts("MMX PARASITIC BOMB CHECKS PASSED");
}

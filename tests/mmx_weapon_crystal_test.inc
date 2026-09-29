static void weapon_crystal_checks(const char *assets,const char *fixture,uint8 *start,
                                 uint8 *expected,uint8 *actual,size_t cap) {
  check(MmxWeaponsLoad(assets),"Crystal Hunter source graphics load");
  for(unsigned character=0;character<2;++character) {
    check(RtlLoadSnapshot(fixture),"restore Crystal Hunter fixture");if(character) zero_health_swap();
    MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=1;MmxWeaponsSetState(w);
    frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
    for(unsigned i=0;i<8;++i) if(c.shots[i].active) {slot=i;break;}
    check(slot<8 && c.shots[slot].vx==832 && MmxWeaponsEnergyAmount(1,1)==27*256,
        "Crystal Hunter normal uses source speed and one-energy cost");
    int y=c.shots[slot].y;zero_replay(8);c=MmxWeaponsGetCombatState();
    check(c.shots[slot].vy>0 && c.shots[slot].y>y,"crystal projectile follows its original falling arc");
    zero_replay(110);for(unsigned i=0;i<205;++i) frame(SNES_PAD_Y);frame(0);zero_replay(3);
    check(!MmxWeaponsTimeActive(),"Crystal Hunter charged effect requires X1 arms");
    MmxWeaponsCancelShots(g_ram);zero_replay(90);MmxWeaponsRefill();g_ram[0x1f99]|=2;
    for(unsigned i=0;i<205;++i) frame(SNES_PAD_Y);frame(0);zero_replay(3);
    check(MmxWeaponsTimeActive() && MmxWeaponsEnergyAmount(1,1)==25*256,
        "charged crystal effect costs two units after the initial normal shot");
    unsigned x=g_ram[0xbad]|g_ram[0xbae]<<8;
    for(unsigned i=0;i<70;++i) frame(SNES_PAD_RIGHT);
    check((unsigned)(g_ram[0xbad]|g_ram[0xbae]<<8)==x,"original distortion onset suspends player motion");
    c=MmxWeaponsGetCombatState();int16_t ripple[224];MmxWeaponsTimeRipple(&c,ripple);bool distorted=false;
    for(unsigned i=0;i<224;++i) if(ripple[i]) distorted=true;
    check(distorted,"charged onset reproduces original scanline distortion");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-crystal-time.cap":".zero-crystal-time.cap");
    size_t n=RtlSaveSnapshotToMemory(start,cap);zero_replay(140);size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n),"restore charged time distortion");zero_replay(140);
    size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"time distortion and half-speed transition replay exactly");
    unsigned moved=0,stayed=0;
    for(unsigned i=0;i<30;++i) {
      x=g_ram[0xbad]|g_ram[0xbae]<<8;frame(SNES_PAD_RIGHT);
      if((unsigned)(g_ram[0xbad]|g_ram[0xbae]<<8)==x) ++stayed;else ++moved;
    }
    check(moved>=12 && moved<=16 && stayed>=14,"charged Crystal Hunter runs the game on alternate frames");
    zero_replay(460);check(!MmxWeaponsTimeActive() && !g_ram[0xbdd],"time effect expires and releases its native slot");
  }
  check(RtlLoadSnapshot(fixture),"restore Crystal Hunter real-enemy encounter");unsigned victim=0;
  for(unsigned i=0;i<400 && !victim;++i) {
    frame(SNES_PAD_RIGHT);
    for(unsigned d=0xe68;d<0x1228;d+=64)
      if(g_ram[d] && (g_ram[d+0x27]&127) && g_ram[d+0x28]<6 &&
          (g_ram[d+0x20]|g_ram[d+0x21])) {victim=d;break;}
  }
  check(victim!=0,"native susceptible enemy is available for crystallization");
  MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=1;MmxWeaponsSetState(w);
  frame(0);frame(SNES_PAD_Y);frame(0);MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
  for(unsigned i=0;i<8;++i) if(c.shots[i].active) {
    c.shots[i].x=(g_ram[victim+5]|g_ram[victim+6]<<8)*256;
    c.shots[i].y=(g_ram[victim+8]|g_ram[victim+9]<<8)*256;
    c.shots[i].vx=c.shots[i].vy=0;slot=i;break;
  }
  check(slot<8,"crystal projectile available for native collision");g_ram[victim+0x27]=16;MmxWeaponsSetCombatState(c);
  zero_replay(3);c=MmxWeaponsGetCombatState();
  check(MmxWeaponsFrozenEnemy(&c,victim),"native enemy contact begins crystallization");
  uint8 enemy_action[3];memcpy(enemy_action,g_ram+victim+1,3);
  zero_replay(25);check(!memcmp(enemy_action,g_ram+victim+1,3),"crystallized enemy AI remains suspended");
  c=MmxWeaponsGetCombatState();check(c.shots[slot].muzzle_pose>=2,"source formation animation produces a solid crystal");
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".crystal-enemy.cap");
  size_t n=RtlSaveSnapshotToMemory(start,cap);zero_replay(15);size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"restore frozen enemy");zero_replay(15);
  size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"frozen enemy and crystal physics replay exactly");
  c=MmxWeaponsGetCombatState();unsigned x=(unsigned)(c.shots[slot].x>>8),y=(unsigned)(c.shots[slot].y>>8);
  g_ram[0xbad]=(uint8_t)x;g_ram[0xbae]=(uint8_t)(x>>8);
  g_ram[0xbb0]=(uint8_t)(y-36);g_ram[0xbb1]=(uint8_t)((y-36)>>8);
  g_ram[0xbcc]=(uint8_t)(y-40);g_ram[0xbcd]=(uint8_t)((y-40)>>8);
  g_ram[0xbc4]=0;g_ram[0xbc5]=255;g_ram[0xbaa]=8;g_ram[0xbab]=2;g_ram[0xbd3]=0;
  MmxWeaponsTerrainEnd(g_ram,0xba8);
  check((g_ram[0xbd3]&4) && (unsigned)(g_ram[0xbb0]|g_ram[0xbb1]<<8)==y-37,
        "player can stand on the source crystal block");
  g_ram[0xbad]=(uint8_t)(x-20);g_ram[0xbae]=(uint8_t)((x-20)>>8);
  g_ram[0xbb0]=(uint8_t)y;g_ram[0xbb1]=(uint8_t)(y>>8);g_ram[0xbab]=0;g_ram[0xbaa]=0;
  g_ram[0xbd3]|=4;g_ram[0x1f99]|=8;
  unsigned health=g_ram[0xbcf]&127;
  frame(SNES_PAD_RIGHT|SNES_PAD_A);
  c=MmxWeaponsGetCombatState();unsigned fragments=0;
  for(unsigned i=0;i<16;++i) if(c.effects[i].active && c.effects[i].weapon==1 &&
      (c.effects[i].pose==14 || c.effects[i].pose==15)) ++fragments;
  check(fragments==8,"dash shatter creates eight original Crystal Hunter shards");
  n=RtlSaveSnapshotToMemory(start,cap);zero_replay(8);en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"restore shatter debris");zero_replay(8);
  an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"shatter debris and native enemy death replay exactly");
  zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".crystal-shatter.cap");
  check(!g_ram[victim] || g_ram[victim+1]==4,"dashing through crystal invokes native enemy destruction");
  check((g_ram[0xbcf]&127)==health && g_ram[0xbaa]!=14,"shattering enemy cannot hurt the dashing player");
  puts("MMX CRYSTAL HUNTER CHECKS PASSED");
}

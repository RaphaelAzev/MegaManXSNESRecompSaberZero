/* Optional owner-provided stage fixtures, copied into a private test folder.
 * Slot numbers in the UI are one-based: slot 05 is save4.sav, 06 save5.sav. */
static void weapon_stage_checks(const char *assets) {
  check(MmxWeaponsLoad(assets),"stage interaction assets load");
  const char *ride=getenv("MMX_RIDE_FIXTURE"),*igloo=getenv("MMX_IGLOO_FIXTURE");
  if(ride) {
    check(RtlLoadSnapshot(ride),"private Ride Armor fixture loads");
    for(unsigned i=0;i<100;++i) {
      frame(i<60?SNES_PAD_LEFT:0);
      if(i==40) zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".ride-enter.cap");
    }
    check(g_ram[0xbbe]==0x6b && g_ram[0xbaa]==0x2c,"native boarding reaches Ride Armor pilot group");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".ride-idle.cap");
    for(unsigned i=0;i<25;++i) frame(SNES_PAD_RIGHT);
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".ride-walk.cap");
    frame(SNES_PAD_Y);zero_replay(8);
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".ride-punch.cap");
    frame(SNES_PAD_UP|SNES_PAD_B);zero_replay(25);
    check(g_ram[0xbbe]!=0x6b,"jumping out restores the normal player sprite group");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".ride-exit.cap");
  }
  if(igloo) {
    for(unsigned character=0;character<2;++character) {
      check(RtlLoadSnapshot(igloo),"private Penguin igloo fixture loads");
      if(character) zero_health_swap();
      MmxWeaponsCancelShots(g_ram);MmxWeaponsState w=MmxWeaponsGetState();
      w.page=1;w.weapon=8;MmxWeaponsSetState(w);MmxWeaponsRefill();
      for(unsigned i=0;i<60;++i) frame(SNES_PAD_RIGHT);
      bool destroyed=false;unsigned target=0;
      for(unsigned d=0xe68;d<0x1228;d+=64) if(g_ram[d] && g_ram[d+10]==0x57 &&
          (g_ram[d+5]|g_ram[d+6]<<8)>(g_ram[0xbad]|g_ram[0xbae]<<8)) {target=d;break;}
      check(target!=0,"original Penguin bunker is in front of the player");
      for(unsigned i=0;i<240 && !destroyed;++i) {
        g_ram[0xbd8]=60; /* Player +$30: isolate contact from the fixture's flyers. */
        frame((i%25==0)?SNES_PAD_Y:0);
        destroyed=g_ram[target+1]==8 || g_ram[target+0x3a]!=0 || !g_ram[target];
      }
      check(destroyed,"Speed Burner invokes native bunker destruction for both characters");
      zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".igloo-x.cap":".igloo-zero.cap");
    }
  }
  const char *boss=getenv("MMX_ARMADILLO_FIXTURE");
  if(boss) {
    check(RtlLoadSnapshot(boss),"private Armadillo fixture loads");
    const unsigned enemy=0xe68;
    check(g_ram[enemy+10]==0x14,"original Armadillo actor is present");
    MmxWeaponsCancelShots(g_ram);MmxWeaponsState w=MmxWeaponsGetState();
    w.page=2;w.weapon=3;MmxWeaponsSetState(w);MmxWeaponsRefill();
    /* Restore armor on this old damaged-boss fixture, then let native AI
     * and collision own the actual hit and break sequence. */
    g_ram[enemy+0x33]=g_ram[enemy+0x38]=g_ram[enemy+0x37]=0;
    g_ram[enemy+0x27]=g_ram[enemy+0x39]=32;g_ram[enemy+2]=4;g_ram[enemy+3]=0;
    frame(0);frame(SNES_PAD_Y);zero_replay(2);
    MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
    for(unsigned i=0;i<8;++i) if(c.shots[i].active) {slot=i;break;}
    check(slot<8,"Triad orb available for armor contact");
    for(unsigned i=0;i<8;++i) if(i!=slot) {memset(&c.shots[i],0,sizeof(c.shots[i]));memset(g_ram+0x1228+i*64,0,64);}
    c.shots[slot].variant=6;c.shots[slot].muzzle_pose=1;c.shots[slot].radius=0;
    c.shots[slot].x=(g_ram[enemy+5]|g_ram[enemy+6]<<8)*256;
    c.shots[slot].y=(g_ram[enemy+8]|g_ram[enemy+9]<<8)*256;
    c.shots[slot].vx=c.shots[slot].vy=0;MmxWeaponsSetCombatState(c);
    bool breaking=false;
    for(unsigned i=0;i<8;++i) {
      frame(0);if(g_ram[enemy+2]==8 && g_ram[enemy+3]==2) breaking=true;
    }
    check(breaking,"Triad contact invokes Armadillo's native electric armor-break action");
    check((g_ram[enemy+39]&127)==29,"Triad uses Electric Spark's three-damage weakness hit");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),".armadillo-break.cap");
  }
  puts("MMX STAGE INTERACTION CHECKS PASSED");
}

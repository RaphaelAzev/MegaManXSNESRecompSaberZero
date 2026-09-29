static unsigned chain_test_slot(void) {
  MmxWeaponCombatState c=MmxWeaponsGetCombatState();
  for(unsigned i=0;i<8;++i) if(c.shots[i].active && c.shots[i].page==1 && c.shots[i].weapon==6) return i;
  return 8;
}
static void weapon_chain_checks(const char *assets,const char *fixture,uint8 *start,
                               uint8 *expected,uint8 *actual,size_t cap) {
  check(MmxWeaponsLoad(assets),"Strike Chain original assets load");
  check(MmxWeaponsPose(1,6,71,0) && MmxWeaponsPose(1,6,71,23),"normal and charged hook/link art exists");
  for(unsigned character=0;character<2;++character) {
    check(RtlLoadSnapshot(fixture),"restore Strike Chain fixture");if(character) zero_health_swap();
    MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=6;MmxWeaponsSetState(w);
    frame(SNES_PAD_Y);frame(0);zero_replay(5);
    check(chain_test_slot()==8 && MmxWeaponsEnergyAmount(1,6)==27*256+128,
        "tap retracts before extending and costs half an energy");
    frame(SNES_PAD_Y);unsigned slot=chain_test_slot();check(slot<8,"held chain allocates a native hook");
    check(MmxWeaponsGetCombatState().shots[slot].pose==1,"normal startup consumes the original first one-tick pose on allocation");
    for(unsigned i=0;i<12;++i) frame(SNES_PAD_Y);
    MmxWeaponCombatState c=MmxWeaponsGetCombatState();
    check(c.shots[slot].active && c.shots[slot].variant==1 && c.shots[slot].vx==1024 &&
        c.shots[slot].x/256-c.shots[slot].origin_x>=32,"holding extends normal hook at four pixels per tick");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-chain-normal.cap":".zero-chain-normal.cap");
    size_t n=RtlSaveSnapshotToMemory(start,cap);for(unsigned i=0;i<25;++i) frame(SNES_PAD_Y);
    size_t en=RtlSaveSnapshotToMemory(expected,cap);check(RtlLoadSnapshotFromMemory(start,n),"restore extended chain");
    for(unsigned i=0;i<25;++i) frame(SNES_PAD_Y);size_t an=RtlSaveSnapshotToMemory(actual,cap);
    same(expected,en,actual,an,"normal chain extension, retraction and original animations replay exactly");
    check(chain_test_slot()==8 && !g_ram[0xbdd],"normal chain retracts after source timer and releases native slot");
    frame(0);for(unsigned i=0;i<220;++i) frame(SNES_PAD_Y);frame(0);zero_replay(6);
    check(!extended_shots(true),"charged chain requires X1 arms");
    g_ram[0x1f99]|=2;for(unsigned i=0;i<205;++i) frame(SNES_PAD_Y);frame(0);frame(0);
    slot=chain_test_slot();c=MmxWeaponsGetCombatState();
    check(slot<8 && c.shots[slot].charged && c.shots[slot].vx==1536 && !c.shots[slot].variant,
        "full charge starts the original charged hook startup at six pixels per tick");
    zero_replay(21);c=MmxWeaponsGetCombatState();
    check(g_ram[0xbf8]>=14,"source shooting pose remains active through the charged tether");
    check(c.shots[slot].active && c.shots[slot].variant==1 && c.shots[slot].x/256-c.shots[slot].origin_x>=54,
        "charged chain extends automatically after its longer startup");
    zero_capture(getenv("MMX_ZERO_TEST_CAPTURE"),character?".x-chain-charged.cap":".zero-chain-charged.cap");
    n=RtlSaveSnapshotToMemory(start,cap);zero_replay(35);en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n),"restore charged chain");zero_replay(35);
    an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"charged chain and contact state replay exactly");
    check(chain_test_slot()==8 && !g_ram[0xbdd],"charged chain retracts and cleans up");
    /* A real X1 pickup is pulled to the player and collected by its native
     * handler, including the extended weapon refill and active-only HP rule. */
    frame(SNES_PAD_Y);for(unsigned i=0;i<10;++i) frame(SNES_PAD_Y);
    slot=chain_test_slot();check(slot<8,"chain available for pickup retrieval");c=MmxWeaponsGetCombatState();
    unsigned item=0x1628;memset(g_ram+item,0,48);g_ram[item]=1;g_ram[item+10]=2;g_ram[item+11]=129;
    unsigned x=c.shots[slot].x/256+4,y=c.shots[slot].y/256;
    g_ram[item+5]=(uint8)x;g_ram[item+6]=(uint8)(x>>8);g_ram[item+8]=(uint8)y;g_ram[item+9]=(uint8)(y>>8);
    g_ram[0xbcf]=8;bool grabbed=false;
    for(unsigned i=0;i<40;++i) {frame(SNES_PAD_Y);c=MmxWeaponsGetCombatState();grabbed|=c.shots[slot].active && c.shots[slot].variant==4;}
    check(grabbed && !g_ram[item] && (g_ram[0xbcf]&127)==10,"normal hook retrieves and natively collects a small health pickup");
    MmxWeaponsCancelShots(g_ram);
  }
  /* Use one private metatile to test actual wall contact without depending
   * on stage progress or modifying the owner's running game. */
  for(unsigned charged=0;charged<2;++charged) {
    check(RtlLoadSnapshot(fixture),"restore wall-pull fixture");
    MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=6;MmxWeaponsSetState(w);g_ram[0x1f99]|=2;
    if(charged) {for(unsigned i=0;i<205;++i) frame(SNES_PAD_Y);frame(0);} else frame(SNES_PAD_Y);
    unsigned slot=chain_test_slot();check(slot<8,"wall test has a hook");
    MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned px=g_ram[0xbad]|g_ram[0xbae]<<8;
    int x=(px+48)&~15,y=c.shots[slot].y>>8;
    unsigned screen=g_ram[0xe800+(y>>8)*32+(x>>8)];
    unsigned cell=0x2000+((screen*512+((y&240)<<1)+((x&240)>>3))&65535);
    unsigned base=g_ram[0xb92]|g_ram[0xb93]<<8|g_ram[0xb94]<<16;
    unsigned delta=(0xffd0-(base&65535))&65535;
    unsigned offset=((base>>16)&127)*32768+0x7fd0;
    uint8 low=g_ram[cell],high=g_ram[cell+1],property=g_snes->cart->rom[offset];
    g_ram[cell]=(uint8)delta;g_ram[cell+1]=(uint8)(delta>>8);g_snes->cart->rom[offset]=0x13;
    bool anchor=false;
    for(unsigned i=0;i<30 && !anchor;++i) {frame(charged?0:SNES_PAD_Y);c=MmxWeaponsGetCombatState();anchor=c.shots[slot].active && c.shots[slot].variant==3;}
    g_ram[cell]=low;g_ram[cell+1]=high;g_snes->cart->rom[offset]=property;
    check(anchor,"hook attaches to actual X1 wall collision");
    unsigned before=g_ram[0xbad]|g_ram[0xbae]<<8;frame(charged?0:SNES_PAD_Y);
    check((unsigned)(g_ram[0xbad]|g_ram[0xbae]<<8)==before+(charged?6:4),"wall anchor pulls at original normal/charged speed");
    frame(SNES_PAD_LEFT);zero_replay(2);check(chain_test_slot()==8,"opposite input detaches chain and restores movement");
  }
  /* Exercise the actual native enemy loop, including fractional charged
   * damage. Test placement brings an existing enemy into the hook path. */
  check(RtlLoadSnapshot(fixture),"restore native chain combat fixture");unsigned enemy=0;
  for(unsigned i=0;i<400 && !enemy;++i) {
    frame(SNES_PAD_RIGHT);
    for(unsigned d=0xe68;d<0x1228;d+=64) if(g_ram[d] && (g_ram[d+0x27]&127) &&
        (g_ram[d+0x20]|g_ram[d+0x21])) {enemy=d;break;}
  }
  check(enemy!=0,"native enemy available for Strike Chain");
  MmxWeaponsState w=MmxWeaponsGetState();w.page=1;w.weapon=6;MmxWeaponsSetState(w);
  frame(0);frame(SNES_PAD_Y);for(unsigned i=0;i<5;++i) frame(SNES_PAD_Y);
  unsigned slot=chain_test_slot();check(slot<8,"normal hook available for native hit");
  MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned d=0x1228+slot*64;
  c.shots[slot].x=(g_ram[enemy+5]|g_ram[enemy+6]<<8)*256;c.shots[slot].y=(g_ram[enemy+8]|g_ram[enemy+9]<<8)*256;
  MmxWeaponsSetCombatState(c);g_ram[enemy+0x27]=32;
  g_ram[d+5]=g_ram[enemy+5];g_ram[d+6]=g_ram[enemy+6];g_ram[d+8]=g_ram[enemy+8];g_ram[d+9]=g_ram[enemy+9];
  /* Damage dispatch runs in the native collision pass before projectile tick. */
  frame(SNES_PAD_Y);frame(SNES_PAD_Y);
  check((g_ram[enemy+0x27]&127)<=30 && chain_test_slot()==8 && MmxWeaponsEnergyAmount(1,6)==27*256,
      "normal hook hits through native collision at 5/3 buster, costs contact energy and retires on a survivor");
  check(RtlLoadSnapshot(fixture),"restore charged chain contact fixture");
  w=MmxWeaponsGetState();w.page=1;w.weapon=6;MmxWeaponsSetState(w);g_ram[0x1f99]|=2;
  for(unsigned i=0;i<205;++i) frame(SNES_PAD_Y);frame(0);frame(0);
  slot=chain_test_slot();check(slot<8,"native firing allocates charged contact test hook");d=0x1228+slot*64;
  check(MmxWeaponsEnergyAmount(1,6)==27*256,"charged launch and initial normal shot each cost half an energy");
  enemy=0xe68;g_ram[enemy]=1;g_ram[enemy+1]=2;g_ram[enemy+10]=1;g_ram[enemy+0x28]=0;g_ram[enemy+0x27]=32;
  unsigned damage=0,energy=MmxWeaponsEnergyAmount(1,6);
  for(unsigned i=0;i<3;++i) {
    MmxWeaponsProjectileTick(g_ram,d,1);
    damage+=MmxWeaponsDamage(g_ram,enemy,d,1);
    check(!MmxWeaponsDamage(g_ram,enemy,d,1),"charged chain deduplicates an enemy inside one collision tick");
  }
  check(damage==1 && MmxWeaponsEnergyAmount(1,6)==energy-48 && chain_test_slot()<8,
      "three charged contacts preserve one-third buster damage, drain three sixteenths and keep the hook alive");
  memset(g_ram+0x1628,0,0x300);g_ram[enemy+0x27]=1;bool lethal=false;
  for(unsigned i=0;i<3 && !lethal;++i) {
    MmxWeaponsProjectileTick(g_ram,d,1);lethal=MmxWeaponsDamage(g_ram,enemy,d,1)!=0;
  }
  check(lethal && g_ram[0x1628] && (g_ram[0x1632]==1 || g_ram[0x1632]==2) && g_ram[0x1633]==1,
      "charged kill supplies the original guaranteed small health/energy reward as a native X1 pickup");
  MmxWeaponsCancelShots(g_ram);
  puts("MMX STRIKE CHAIN CHECKS PASSED");
}

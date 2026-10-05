/* Private owner saves: slot 01 sensors, 09 small lift, 10 red platforms,
 * and 12 main elevator. Check both character assignments and world actors. */
static void kuwanger_dump(unsigned t) {
  MmxCoopState s=MmxCoopGetState();
  fprintf(stderr,"kuwanger t=%u anchor=%u p1=%u/%u p2=%u/%u hp=%u/%u\n",t,s.anchor,
      coop_x(&s.players[0]),coop_y(&s.players[0]),coop_x(&s.players[1]),coop_y(&s.players[1]),
      s.players[0].body[0x27]&127,s.players[1].body[0x27]&127);
  for(unsigned d=0xe68;d<0x1228;d+=64) if(g_ram[d])
    fprintf(stderr," enemy %04x kind=%02x state=%02x/%02x xy=%u/%u contact=%02x timer=%02x owner=%02x\n",
        d,g_ram[d+10],g_ram[d+1],g_ram[d+2],eagle_word(g_ram+d+5),eagle_word(g_ram+d+8),
        g_ram[d+0x2c],g_ram[d+0x36],g_ram[d+0x3f]);
}
static void kuwanger_roster(unsigned roster,unsigned anchor) {
  MmxCoopState s=MmxCoopGetState();
  for(unsigned seat=0;seat<2;++seat) {
    s.players[seat].character=(uint8_t)(roster^seat);s.players[seat].zero.active_x=!(roster^seat);
  }
  MmxZeroSetState(s.players[s.current].zero);MmxCoopSetState(&s);MmxCoopSelect(g_ram,anchor);
  s=MmxCoopGetState();s.anchor=(uint8_t)anchor;MmxCoopSetState(&s);
}
static void kuwanger_place(unsigned seat,unsigned x,unsigned y) {
  unsigned current=MmxCoopGetState().current;MmxCoopSelect(g_ram,seat);
  eagle_putword(g_ram+0xbad,x);eagle_putword(g_ram+0xbca,x);
  eagle_putword(g_ram+0xbb0,y);eagle_putword(g_ram+0xbcc,y-2);
  eagle_putword(g_ram+0xbc2,0);eagle_putword(g_ram+0xbc4,0xfd00);
  g_ram[0xbaa]=8;g_ram[0xbab]=2;g_ram[0xbd3]=g_ram[0xbd4]=0;
  g_ram[0xbce]=127;g_ram[0xc11]=0;MmxCoopCapture(g_ram);MmxCoopSelect(g_ram,current);
}
static unsigned kuwanger_aims[2];
static void kuwanger_aim_probe(CpuState *cpu,uint32_t pc) {
  (void)pc;
  if(cpu->D>=0xe68 && cpu->D<0x1228 && !((cpu->D-0xe68)%64) && g_ram[cpu->D+10]==0x44)
    ++kuwanger_aims[MmxCoopGetState().current];
}
static void coop_kuwanger_checks(const char *directory) {
  for(unsigned roster=0;roster<2;++roster) for(unsigned anchor=0;anchor<2;++anchor) {
    fprintf(stderr,"kuwanger support roster=%u anchor=%u\n",roster,anchor);
    owner_wide_load(directory,11);kuwanger_roster(roster,anchor);
    coop_frames(60,SNES_PAD_RIGHT,SNES_PAD_RIGHT);coop_frames(120,0,0);
    MmxCoopState s=MmxCoopGetState();kuwanger_dump(180);
    check(abs((int)coop_y(&s.players[0])-(int)coop_y(&s.players[1]))<=2 &&
        coop_y(&s.players[0])<4440 && (g_ram[0xea8+0x3f]&3)==3,
        "both players naturally board and ascend Kuwanger's main elevator");
    owner_wide_load(directory,11);kuwanger_roster(roster,anchor);
    s=MmxCoopGetState();unsigned original_x=coop_x(&s.players[0]);
    coop_frames(60,0,SNES_PAD_RIGHT);coop_frames(120,0,0);s=MmxCoopGetState();kuwanger_dump(180);
    check(coop_x(&s.players[0])<=original_x+16 && coop_y(&s.players[0])==4431 &&
        coop_y(&s.players[1])<4440 && g_ram[0xea8+0x3f]==2,
        "P2 can start and ride the elevator without carrying P1 from the ledge");
    owner_wide_load(directory,8);kuwanger_roster(roster,anchor);
    kuwanger_place(1,eagle_word(g_ram+0xe68+5)+4,eagle_word(g_ram+0xe68+8)-36);
    coop_frames(50,0,0);s=MmxCoopGetState();kuwanger_dump(50);
    check(g_ram[0xe68+0x2c]==3 && abs((int)coop_y(&s.players[0])-(int)coop_y(&s.players[1]))<=2,
        "P2 lands on and follows the small moving lift beside P1");
    owner_wide_load(directory,9);kuwanger_roster(roster,anchor);
    unsigned px=eagle_word(g_ram+0xe68+5),py=eagle_word(g_ram+0xe68+8);
    /* Board the exposed right end; the left half initially crosses a wall. */
    kuwanger_place(0,px+24,py-28);kuwanger_place(1,px+28,py-28);
    coop_frames(40,0,0);s=MmxCoopGetState();kuwanger_dump(40);
    check(g_ram[0xe68+0x3f]==3 && abs((int)coop_y(&s.players[0])-(int)coop_y(&s.players[1]))<=2 &&
        coop_x(&s.players[0])>px+24 && coop_x(&s.players[1])>px+28,
        "both players land on and move with the red platform");
    owner_wide_load(directory,9);kuwanger_roster(roster,anchor);
    kuwanger_place(1,px+28,py-28);coop_frames(40,0,0);s=MmxCoopGetState();
    check(g_ram[0xe68+0x3f]==2 && coop_y(&s.players[0])==2495 &&
        coop_x(&s.players[1])>px+28 && coop_y(&s.players[1])<2440,
        "a lone P2 rides the red platform without carrying P1 underneath");
  }
  interp_bridge_add_pre_opcode_hook(0x87bb09,kuwanger_aim_probe);
  for(unsigned roster=0;roster<2;++roster) for(unsigned anchor=0;anchor<2;++anchor)
      for(unsigned tripper=0;tripper<2;++tripper) {
    owner_wide_load(directory,0);kuwanger_roster(roster,anchor);
    unsigned d=0xe68,turret=0xea8,x=eagle_word(g_ram+d+5),y=eagle_word(g_ram+d+8);
    g_ram[d+1]=2;g_ram[d+0x34]=50;g_ram[d+0x3a]=0;g_ram[d+0x33]=1;
    kuwanger_place(tripper,x+20,y+10);kuwanger_place(tripper^1,x+110,y+59);
    memset(kuwanger_aims,0,sizeof(kuwanger_aims));coop_frames(1,0,0);kuwanger_dump(1);
    check(g_ram[turret+0x36] && g_ram[turret+0x3f]==tripper+1,
        "either player's laser contact arms the turret with its actual tripper");
    check(MmxCoopGetState().current==anchor,"sensor returns the normal world actor after its trigger");
    const size_t cap=2u*1024u*1024u;uint8_t *before=malloc(cap),*expected=malloc(cap),*actual=malloc(cap);
    check(before && expected && actual,"laser ownership replay buffers allocate");
    size_t n=RtlSaveSnapshotToMemory(before,cap);coop_frames(40,0,0);
    check(kuwanger_aims[tripper]>0 && !kuwanger_aims[tripper^1],
        "the native delayed laser shot aims at the player who tripped the sensor");
    size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(before,n),"armed laser snapshot restores");coop_frames(40,0,0);
    same(expected,en,actual,RtlSaveSnapshotToMemory(actual,cap),"laser target ownership survives snapshot replay");
    free(before);free(expected);free(actual);
  }
  puts("MMX CO-OP KUWANGER CHECKS PASSED");
}

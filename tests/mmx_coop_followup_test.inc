/* Focused ROM regressions for the owner's October 3 playtest. */
static void modern_image(const char *suffix);
static void coop_followup_fixture(const char *fixture) {
  MmxCoopDisable();check(RtlLoadSnapshot(fixture),"follow-up fixture loads");
  MmxZeroSetModern(false);check(MmxCoopEnable(0),"follow-up co-op enables");
  coop_frames(8,0,0);MmxCoopState s=MmxCoopGetState();
  check(coop_place_fixture_partner(coop_x(&s.players[0])+32,coop_y(&s.players[0])),"follow-up partner placed");
  coop_frames(8,0,0);
}
static void coop_followup_checks(const char *fixture) {
  coop_followup_fixture(fixture);
  coop_frames(60,0,SNES_PAD_Y);
  unsigned previous=MmxCoopGetState().players[1].zero.charge;bool landed=false;
  for(unsigned t=0;t<90;++t) {
    coop_frames(1,0,SNES_PAD_Y|(t<10?SNES_PAD_B:0));
    MmxCoopPlayer p=MmxCoopGetState().players[1];
    landed|=p.body[2]==0x0a;
    if(p.zero.charge<previous) fprintf(stderr,"charge dropped at action %02x: %u -> %u\n",p.body[2],previous,p.zero.charge);
    check(p.zero.charge>=previous,"held Zero buster charge survives every jump/landing frame");
    previous=p.zero.charge;
  }
  check(landed,"charge check includes native landing recovery");
  for(unsigned kind=0x0f;kind<=0x10;++kind) for(unsigned solo=0;solo<2;++solo) {
    coop_followup_fixture(fixture);MmxCoopState s=MmxCoopGetState();
    unsigned x=coop_x(&s.players[0])+16,y=coop_y(&s.players[0])-40,d=0x16b8;
    memset(g_ram+0x1628,0,0x300);memset(g_ram+d,0,48);
    g_ram[d]=1;g_ram[d+1]=2;g_ram[d+2]=2;g_ram[d+10]=(uint8_t)kind;
    eagle_putword(g_ram+d+5,x);eagle_putword(g_ram+d+8,y);
    eagle_putword(g_ram+d+0x22,x);eagle_putword(g_ram+d+0x24,y);
    eagle_putword(g_ram+d+0x29,y+64);eagle_putword(g_ram+d+0x1c,0x100);
    for(unsigned seat=0;seat<2;++seat) {
      MmxCoopSelect(g_ram,seat);eagle_putword(g_ram+0xbad,x);eagle_putword(g_ram+0xbb0,y-26);
      eagle_putword(g_ram+0xbca,x);eagle_putword(g_ram+0xbcc,y-26);
      eagle_putword(g_ram+0xbc4,0);g_ram[0xbd3]=0;g_ram[0xbaa]=8;g_ram[0xbab]=2;
      if(solo && !seat) {
        eagle_putword(g_ram+0xbad,x-48);eagle_putword(g_ram+0xbb0,y+40);
        g_ram[0xbaa]=g_ram[0xbab]=0;g_ram[0xbd3]=4;
      }
    }
    MmxCoopSelect(g_ram,0);coop_frames(12,0,0);s=MmxCoopGetState();
    fprintf(stderr,"platform kind=%02x rider=%u y=%u players=%u/%u state=%u\n",kind,g_ram[d+0x2c],eagle_word(g_ram+d+8),coop_y(&s.players[0]),coop_y(&s.players[1]),g_ram[d+1]);
    check(g_ram[d+0x2c]==(solo?2:3) &&
        (solo ? coop_y(&s.players[1])+20<coop_y(&s.players[0]) : coop_y(&s.players[0])==coop_y(&s.players[1])),
        "Zero rides rising/flying platforms alone or alongside X");
    if(kind==0x10) check(g_ram[d+1]==4,"Zero boarding launches the original flying platform");
  }
  coop_followup_fixture(fixture);
  coop_weapon_select(0,1,8);
  g_ram[0x1f7a]=8;g_ram[0x1fa0]=0;g_ram[0xd3]=10;g_ram[0xd4]=0;
  unsigned last=~0u,seen=0;
  for(unsigned t=0;t<1500;++t) {
    coop_frames(1,0,0);
    unsigned signature=g_ram[0xd1]|g_ram[0xd2]<<8|g_ram[0xd3]<<16|g_ram[0xd4]<<24;
    if(signature!=last) {fprintf(stderr,"demo t=%u mode=%06x weapon=%u command=%u\n",t,signature,g_ram[0xbdb],g_ram[0xc01]);last=signature;}
    if(g_ram[0xd3]==10 && g_ram[0xd4]>=4) for(unsigned d=0x1228;d<0x1428;d+=64)
      if(g_ram[d] && g_ram[d+10]==14) {
        if(++seen==10) modern_image("weapon-get-ice");
      }
  }
  fprintf(stderr,"demo ice frames=%u\n",seen);
  check(seen,"Shotgun Ice acquisition demo creates native ice projectiles");
  MmxCoopDisable();puts("MMX CO-OP FOLLOW-UP CHECKS PASSED");
}

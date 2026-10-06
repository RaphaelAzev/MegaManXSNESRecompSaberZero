/* ROM-backed Sigma bounce-pad checks using a private owner checkpoint. */
static void coop_bounce_checks(const char *fixture) {
  for(unsigned roster=0;roster<2;++roster) for(unsigned rider=0;rider<2;++rider) {
    check(MmxCoopEnable(0),"bounce-pad roster enables");
    check(RtlLoadSnapshot(fixture),"private bounce-pad fixture loads");
    MmxCoopCapture(g_ram);MmxCoopState s=MmxCoopGetState();
    if(roster) {MmxCoopPlayer p=s.players[0];s.players[0]=s.players[1];s.players[1]=p;}
    unsigned pad=0;
    for(unsigned d=0xe68;d<0x1228;d+=64) {
      if(!pad && g_ram[d] && g_ram[d+10]==0x40 && g_ram[d+1]==2) pad=d;
      else memset(g_ram+d,0,64);
    }
    check(pad!=0,"fixture contains an idle bounce pad");
    unsigned px=g_ram[pad+5]|g_ram[pad+6]<<8;
    s.current=s.anchor=(uint8_t)(rider^1);s.scene_owner=s.scene_phase=s.stage_pending=0;
    for(unsigned seat=0;seat<2;++seat) {
      /* The owner's X stands on clear ground 80 pixels right of this pad. */
      MmxCoopPlayer *p=&s.players[seat];unsigned x=seat==rider ? px : px+80;
      p->body[5]=(uint8_t)x;p->body[6]=(uint8_t)(x>>8);
      p->body[8]=159;p->body[9]=5;p->body[2]=p->body[3]=0;
      memset(p->body+0x1a,0,4);p->body[0x2b]=p->body[0x2c]=0;
      p->zero.swap_phase=0;p->body[0x27]=0x98;p->status=MMX_COOP_ALIVE;
      memset(p->shots,0,sizeof(p->shots));
    }
    memcpy(g_ram+0xba8,s.players[s.current].body,sizeof(s.players[0].body));
    memset(g_ram+0x1228,0,0x200);
    MmxZeroSetState(s.players[s.current].zero);MmxCoopSetState(&s);
    bool active=false;unsigned lowest=1439,other_lowest=1439;
    for(unsigned tick=0;tick<80;++tick) {
      coop_frames(1,0,0);s=MmxCoopGetState();
      if(g_ram[pad+1]==4) {
        active=true;
        if(g_ram[pad+0x3f]!=rider+1) check(false,"bounce pad retains its triggering seat");
      }
      unsigned y=coop_y(&s.players[rider]);if(y<lowest) lowest=y;
      y=coop_y(&s.players[rider^1]);if(y<other_lowest) other_lowest=y;
    }
    check(active && lowest<1391,"either character in either seat activates and rides the bounce");
    check(other_lowest==1439,"bounce does not launch the partner away from the pad");
    check(!MmxCoopViewsGetWorldState().actor_return,"bounce restores the world actor after each update");
  }
  MmxCoopDisable();puts("MMX CO-OP BOUNCE CHECKS PASSED");
}

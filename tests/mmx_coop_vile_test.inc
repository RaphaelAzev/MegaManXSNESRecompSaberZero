/* Private ROM-backed replay from the Sigma 1 door checkpoint. */
static unsigned coop_vile_slot(unsigned kind) {
  for(unsigned d=0xe68;d<0x1228;d+=64)
    if(g_ram[d] && g_ram[d+10]==kind) return d;
  return 0;
}
static void coop_vile_checks(const char *fixture) {
  unsigned roster=getenv("MMX_COOP_VILE_ZERO_WORLD") ? 1 : 0;
  check(MmxCoopEnable(roster),"Vile co-op roster enables");
  check(RtlLoadSnapshot(fixture),"private Sigma 1 door fixture loads");
  /* This checkpoint predates co-op. Match its native character context to
   * the requested roster before Initialize captures the world actor. */
  MmxZeroState z=MmxZeroGetState();z.active_x=roster==MMX_COOP_X;MmxZeroSetState(z);
  bool captured=false,hidden=false,rescued=false,returned=false;
  unsigned partner_hp=0,partner_weapon=0;
  for(unsigned tick=0;tick<7000;++tick) {
    MmxCoopState s=MmxCoopGetState();
    unsigned vile=coop_vile_slot(0x67),zero=coop_vile_slot(0x66);
    if(vile && g_ram[vile+1]==2 && g_ram[vile+2]==0x16 && !captured) {
      captured=true;partner_hp=s.players[s.anchor^1].body[0x27]&127;
      partner_weapon=s.players[s.anchor^1].body[0x33];
      check(s.players[s.anchor^1].status==MMX_COOP_ALIVE,"partner alive before Vile capture");
      check(RtlSaveSnapshot("vile-capture.sav"),"private pre-capture fixture saves");
    }
    if(captured && s.scene_phase==2) {
      if(!hidden) check(s.scene_owner==s.anchor+1,"Vile capture belongs to the world actor");
      hidden=true;
      if((s.players[s.anchor^1].body[0x27]&127)!=partner_hp ||
          s.players[s.anchor^1].body[0x33]!=partner_weapon ||
          s.players[s.anchor^1].body[2]==0x3c)
        check(false,"hidden partner retains health and avoids Vile restraint");
    }
    if(captured && zero && g_ram[zero+2]>=14) rescued=true;
    unsigned final=coop_vile_slot(0x69);
    if(captured && hidden && rescued && final && g_ram[final+1]==4 &&
        !s.scene_owner && !g_ram[0xc16]) {
      returned=true;
      check(s.players[0].status==MMX_COOP_ALIVE && s.players[1].status==MMX_COOP_ALIVE &&
          (s.players[s.anchor^1].body[0x27]&127)==partner_hp,
          "both players return alive for unarmored Vile");
      unsigned x0=coop_x(&s.players[0]),x1=coop_x(&s.players[1]);
      coop_frames(12,SNES_PAD_LEFT,SNES_PAD_RIGHT);s=MmxCoopGetState();
      check(coop_x(&s.players[0])<x0 && coop_x(&s.players[1])>x1,
          "both players regain movement after the rescue");
      check(RtlSaveSnapshot("vile-final-fight.sav"),"private playable final-fight fixture saves");
      break;
    }
    unsigned pad=(g_ram[0x1e4d]|g_ram[0x1e4e]<<8)<2688 ? SNES_PAD_RIGHT : 0;
    if(g_ram[0x1f3b] && tick%30==0) pad|=SNES_PAD_B|SNES_PAD_Y|SNES_PAD_A;
    coop_frames(1,pad,0);
  }
  if(!returned) {
    MmxCoopState s=MmxCoopGetState();
    fprintf(stderr,"Vile incomplete: capture=%u hidden=%u rescue=%u scene=%u/%u action=%02x:%02x lock=%02x HP=%u/%u\n",
        captured,hidden,rescued,s.scene_owner,s.scene_phase,g_ram[0xbaa],g_ram[0xbab],g_ram[0xc16],g_ram[0xbcf]&127,g_ram[0x1f9a]);
    RtlSaveSnapshot("vile-incomplete.sav");
  }
  check(captured && hidden && rescued && returned,"Sigma 1 capture, rescue and partner return complete");
  MmxCoopDisable();puts("MMX CO-OP VILE CHECKS PASSED");
}

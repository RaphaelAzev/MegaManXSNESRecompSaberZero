/* Focused offline regressions for pit fatalities and voluntary armor exits. */
static unsigned pit_sounds[2],pit_orbs[2];
static uint32_t pit_existing_orbs;
static void coop_pit_effect_check(CpuState *cpu,uint32_t pc) {
  if(cpu->D!=0xba8) return;
  unsigned seat=MmxCoopGetState().current;
  if((pc&65535)==0x8aa4) {++pit_sounds[seat];return;}
  if((pc&65535)==0x8add) {
    pit_existing_orbs=0;
    for(unsigned i=0;i<31;++i) if(g_ram[0x1928+i*32]) pit_existing_orbs|=1u<<i;
    return;
  }
  for(unsigned i=0;i<31;++i) {
    unsigned d=0x1928+i*32;
    if(!(pit_existing_orbs&(1u<<i)) && g_ram[d] && g_ram[d+10]==14) {
      ++pit_orbs[seat];
      check(MmxZeroDeathOrbRed(g_ram,d)==MmxZeroActive(),"pit death orbs retain the emitting character's color");
    }
  }
}
static void coop_pit_drop(unsigned seat) {
  unsigned previous=MmxCoopGetState().current;
  MmxCoopSelect(g_ram,seat);
  unsigned y=eagle_word(g_ram+0x1e50)+260;
  eagle_putword(g_ram+0xbb0,y);eagle_putword(g_ram+0xbcc,y);
  g_ram[0xbaa]=8;g_ram[0xbab]=2;g_ram[0xbd3]=0;
  eagle_putword(g_ram+0xbc4,0xfd00);
  MmxCoopCapture(g_ram);MmxCoopSelect(g_ram,previous);
}
static void coop_team_pit_checks(const char *directory) {
  interp_bridge_set_pre_opcode_hook(0x818aa4,coop_pit_effect_check);
  interp_bridge_set_pre_opcode_hook(0x818add,coop_pit_effect_check);
  interp_bridge_set_pre_opcode_hook(0x818b09,coop_pit_effect_check);
  for(unsigned roster=0;roster<2;++roster)
  for(unsigned first=0;first<2;++first)
  for(unsigned delay=0;delay<3;++delay) {
    owner_wide_load(directory,5);coop_frames(8,0,0);MmxCoopState s=MmxCoopGetState();
    check(coop_place_fixture_partner(coop_x(&s.players[0])+32,coop_y(&s.players[0])),"pit regression partner placed");
    coop_frames(8,0,0);s=MmxCoopGetState();
    for(unsigned seat=0;seat<2;++seat) {
      s.players[seat].character=(uint8_t)(roster^seat);
      s.players[seat].zero.active_x=!(roster^seat);
    }
    MmxZeroSetState(s.players[s.current].zero);MmxCoopSetState(&s);
    unsigned lives=g_ram[0x1f80];memset(pit_sounds,0,sizeof(pit_sounds));memset(pit_orbs,0,sizeof(pit_orbs));
    coop_pit_drop(first);coop_frames(delay==2?90:delay==1?10:0,0,0);
    s=MmxCoopGetState();
    if(delay==2) {
      check(s.players[first].status==MMX_COOP_FALLEN && !(s.players[first].body[0x27]&127),
          "first pit fall completes its native death while the partner stays alive");
      check(pit_sounds[first]==1 && pit_orbs[first]==8,
          "first pit death emits the native sound and eight correctly colored orbs");
      check(g_ram[0x1f80]==lives && s.players[first^1].status==MMX_COOP_ALIVE,
          "first pit death does not restart the surviving player's stage");
    }
    coop_pit_drop(first^1);
    bool returned=false;
    for(unsigned t=0;t<1200 && !returned;++t) {
      coop_frames(1,0,0);s=MmxCoopGetState();
      returned=g_ram[0x1f80]+1==lives && !s.stage_pending && !MmxCoopTransitionActive() &&
          s.players[0].status==MMX_COOP_ALIVE && s.players[1].status==MMX_COOP_ALIVE &&
          (s.players[0].body[0x27]&127)==g_ram[0x1f9a] &&
          (s.players[1].body[0x27]&127)==g_ram[0x1f9a];
    }
    fprintf(stderr,"pit team roster=%u first=%u delay=%u returned=%u lives=%u/%u mode=%u action=%02x:%02x/%02x:%02x status=%u/%u freeze=%02x/%02x solo=%u/%u sounds=%u orbs=%u\n",
        roster,first,delay,returned,lives,g_ram[0x1f80],g_ram[0xd3],
        s.players[0].body[2],s.players[0].body[3],s.players[1].body[2],s.players[1].body[3],
        s.players[0].status,s.players[1].status,g_ram[0x1f13],g_ram[0x1f19],
        s.solo_death[0],s.solo_death[1],pit_sounds[0]+pit_sounds[1],pit_orbs[0]+pit_orbs[1]);
    if(!returned) {
      uint16_t lx=0,ly=0;bool landing=MmxCoopFindLanding(g_ram,&lx,&ly);
      fprintf(stderr,"pit restart stage=%u pending=%u anchor=%u current=%u HP=%u/%u flags=%02x/%02x/%02x/%02x camera=%u/%u pos=%u/%u scene=%u/%u landing=%u:%u/%u checkpoint=%u\n",
          g_ram[0x1f7a],s.stage_pending,s.anchor,s.current,s.players[0].body[0x27]&127,s.players[1].body[0x27]&127,
          g_ram[0x1f0c],g_ram[0x1f10],g_ram[0x1f23],g_ram[0x1f48],eagle_word(g_ram+0x1e4d),eagle_word(g_ram+0x1e50),
          eagle_word(g_ram+0xbad),eagle_word(g_ram+0xbb0),s.scene_owner,s.scene_phase,landing,lx,ly,g_ram[0x1f7b]);
    }
    check(returned,"sequential or overlapping pit deaths spend one life and restart both players");
    check(pit_sounds[0]==1 && pit_sounds[1]==1 && pit_orbs[0]>=8 && pit_orbs[1]>=8,
        "both fatal falls retain their sounds and death orbs even with overlapping countdowns");
    for(unsigned t=0;t<400 && (s.players[0].body[2] || s.players[1].body[2]);++t) {
      coop_frames(1,0,0);s=MmxCoopGetState();
    }
    unsigned x0=coop_x(&s.players[0]),x1=coop_x(&s.players[1]);
    coop_frames(8,SNES_PAD_RIGHT,SNES_PAD_RIGHT);s=MmxCoopGetState();
    check(coop_x(&s.players[0])>x0 && coop_x(&s.players[1])>x1,
        "both players regain movement after the native checkpoint arrival finishes");
  }
  interp_bridge_set_pre_opcode_hook(0x818aa4,NULL);
  interp_bridge_set_pre_opcode_hook(0x818add,NULL);
  interp_bridge_set_pre_opcode_hook(0x818b09,NULL);
  puts("MMX CO-OP TEAM PIT CHECKS PASSED");
}
static void coop_armor_withdraw_checks(const char *directory) {
  for(unsigned pilot=0;pilot<2;++pilot) for(unsigned roster=0;roster<2;++roster) {
    owner_wide_load(directory,8);coop_frames(30,0,0);MmxCoopState s=MmxCoopGetState();
    if(!pilot) {
      MmxCoopPlayer temporary=s.players[0];s.players[0]=s.players[1];s.players[1]=temporary;
      s.current=s.anchor=0;
    }
    for(unsigned seat=0;seat<2;++seat) {
      s.players[seat].character=(uint8_t)(roster^seat);
      s.players[seat].zero.active_x=!(roster^seat);
    }
    MmxZeroSetState(s.players[s.current].zero);MmxCoopSetState(&s);
    coop_frames(1,0,0);coop_frames(120,pilot?0:SNES_PAD_SELECT,pilot?SNES_PAD_SELECT:0);
    s=MmxCoopGetState();
    check(!MmxCoopTransitionActive() && s.players[pilot].status==MMX_COOP_ALIVE &&
        s.players[pilot].body[2]==0x2c && s.anchor==pilot && (g_ram[0xe22]&0x40),
        "holding Select cannot withdraw either character or seat while piloting Ride Armor");
    unsigned tick=g_ram[0xb9c];
    coop_frames(20,pilot?0:SNES_PAD_RIGHT,pilot?SNES_PAD_RIGHT:0);
    s=MmxCoopGetState();
    fprintf(stderr,"armor pilot=%u roster=%u ticks=%u anchor=%u current=%u pass=%u input=%04x held=%02x%02x status=%u/%u HP=%u/%u flags=%02x/%02x/%02x/%02x scene=%u/%u action=%02x:%02x pos=%u/%u armor=%u/%u\n",
        pilot,roster,(uint8_t)(g_ram[0xb9c]-tick),s.anchor,s.current,s.controller_pass,s.players[pilot].input,
        s.players[pilot].body[0x37],s.players[pilot].body[0x36],s.players[0].status,s.players[1].status,
        s.players[0].body[0x27]&127,s.players[1].body[0x27]&127,g_ram[0x1f0c],g_ram[0x1f13],g_ram[0x1f19],g_ram[0x1f48],
        s.scene_owner,s.scene_phase,s.players[pilot].body[2],s.players[pilot].body[3],coop_x(&s.players[pilot]),coop_y(&s.players[pilot]),
        eagle_word(g_ram+0xe1d),eagle_word(g_ram+0xe20));
    check((uint8_t)(g_ram[0xb9c]-tick)==20 && (s.players[pilot].body[0x37]&1) &&
        s.anchor==pilot && !s.controller_pass,
        "native gameplay and pilot input continue after a rejected armor withdrawal");
  }
  puts("MMX CO-OP ARMOR WITHDRAWAL CHECKS PASSED");
}

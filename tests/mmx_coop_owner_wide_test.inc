/* Optional ROM regressions using private copies of the owner's October 4
 * saves. No networking or interactive instances are needed. */
static void owner_wide_image(const char *name) {
  const char *base=getenv("MMX_ZERO_TEST_CAPTURE");if(!base) return;
  MmxRenderView v=g_mmx_custom_view;
  uint32_t *pixels=malloc(v.width*224*4);
  check(pixels && MmxRendererDraw(pixels,v,false),"owner widescreen frame renders");
  char path[4096];snprintf(path,sizeof(path),"%s.%s.bmp",base,name);
  FILE *f=fopen(path,"wb");check(f!=NULL,"owner capture opens");
  uint8_t signature[2]={'B','M'};
  uint32_t header[]={54+v.width*224*4,0,54,40,v.width,224,0x200001,0,v.width*224*4,0,0,0,0};
  fwrite(signature,2,1,f);fwrite(header,sizeof(header),1,f);
  for(int y=223;y>=0;--y) fwrite(pixels+y*v.width,v.width*4,1,f);
  fclose(f);free(pixels);
}
static void owner_wide_load(const char *directory,unsigned slot) {
  char path[4096];snprintf(path,sizeof(path),"%s/save%u.sav",directory,slot);
  check(MmxCoopEnable(0) && RtlLoadSnapshot(path),"owner fixture loads privately");
  MmxCoopState s=MmxCoopGetState();
  fprintf(stderr,"fixture %u stage=%u anchor=%u current=%u status=%u/%u action=%02x/%02x armor=%02x:%02x%02x%02x\n",
      slot,g_ram[0x1f7a],s.anchor,s.current,s.players[0].status,s.players[1].status,
      s.players[0].body[2],s.players[1].body[2],g_ram[0xe22],g_ram[0xe19],g_ram[0xe1a],g_ram[0xe1b]);
}
static void owner_heart_checks(const char *fixture) {
  for(unsigned collector=0;collector<2;++collector) {
    coop_followup_fixture(fixture);MmxCoopSelect(g_ram,collector^1);
    unsigned floor=eagle_word(g_ram+0xbb0),x=eagle_word(g_ram+0xbad),y=floor-24;
    eagle_putword(g_ram+0xbb0,y);eagle_putword(g_ram+0xbcc,y);
    g_ram[0xbaa]=8;g_ram[0xbab]=2;g_ram[0xbd3]=0;
    eagle_putword(g_ram+0xbc2,0x178);eagle_putword(g_ram+0xbc4,0x300);
    MmxCoopSelect(g_ram,collector);
    g_ram[0x1f9a]=16;g_ram[0x1f9c]=0;g_ram[0xbcf]=0x8e;
    uint8_t *tank=g_ram+0x1628;memset(tank,0,48);
    tank[0]=1;tank[10]=11;tank[11]=1;
    eagle_putword(tank+5,eagle_word(g_ram+0xbad));
    eagle_putword(tank+8,eagle_word(g_ram+0xbb0)-12);
    MmxCoopSelect(g_ram,0);
    unsigned pauses=0,last_x=0,last_y=0;
    for(unsigned t=0;t<150;++t) {
      coop_frames(1,0,0);MmxCoopState s=MmxCoopGetState();
      bool pause=g_ram[0x1f13] && g_ram[0x1f16];
      if(pause) {
        check(!s.scene_owner,"Heart Tank refill does not start partner cutscene transport");
        if(pauses) check(coop_x(&s.players[collector^1])==last_x && coop_y(&s.players[collector^1])==last_y,
            "airborne partner does not integrate motion during Heart Tank refill");
        last_x=coop_x(&s.players[collector^1]);last_y=coop_y(&s.players[collector^1]);++pauses;
      }
    }
    MmxCoopState s=MmxCoopGetState();
    fprintf(stderr,"heart collector=%u paused=%u max=%u other=%u/%u floor=%u initial=%u/%u\n",
        collector,pauses,g_ram[0x1f9a],coop_x(&s.players[collector^1]),coop_y(&s.players[collector^1]),floor,x,y);
    check(pauses>70 && g_ram[0x1f9a]==18 && (g_ram[0x1f9c]&1),"native Heart Tank collection completes its upgrade");
    check(coop_y(&s.players[collector^1])==floor && (s.players[collector^1].body[0x27]&127),
        "airborne partner resumes collision and lands after Heart Tank upgrade");
  }
}
static unsigned rewind_modal_presents;
static void rewind_modal_begin(int w,int h,uint8 **pixels,int *pitch) {
  (void)w;(void)h;*pixels=NULL;*pitch=0;
}
static void rewind_modal_end(void) {
  if(++rewind_modal_presents==3) {
    SDL_Event e={0};e.type=SDL_KEYDOWN;
#if defined(LNG_SDL3)
    e.key.key=SDLK_ESCAPE;
#else
    e.key.keysym.sym=SDLK_ESCAPE;
#endif
    SDL_PushEvent(&e);
  }
}
static void owner_rewind_checks(const char *fixture) {
  coop_followup_fixture(fixture);
  snes_rewind_shutdown();snes_rewind_set_defaults(1,50,15);snes_rewind_configure();
  const size_t cap=2u*1024u*1024u;
  uint8_t *saved=malloc(cap),*actual=malloc(cap);
  check(saved && actual,"rewind validation buffers allocate");
  size_t n=0;
  for(unsigned t=0;t<750;++t) {
    coop_frames(1,SNES_PAD_Y,SNES_PAD_Y);
    snes_rewind_note_frame();
    if(t==599) n=RtlSaveSnapshotToMemory(saved,cap);
  }
  check(n && snes_rewind_open(),"co-op rewind opens after filling the owner's history depth");
  for(unsigned t=0;t<80;++t) RewindKeyDown(SDLK_LEFT,0);
  check(snes_rewind_selected_seconds()==12.25f,"50 snapshots at 15-frame spacing retain 12.25 seconds between endpoints");
  const uint32_t *pixels=NULL;int w=0,h=0;
  check(snes_rewind_overlay_image(&pixels,&w,&h) && pixels && w==512 && h==176,
      "oldest co-op rewind filmstrip renders");
  RewindKeyDown(SDLK_ESCAPE,0);
  check(!snes_rewind_is_open(),"rewind cancel returns without restoring a snapshot");
  check(snes_rewind_open(),"co-op rewind reopens after cancel");
  for(unsigned t=0;t<10;++t) RewindKeyDown(SDLK_LEFT,0);
  check(snes_rewind_selected_seconds()==2.5f,"rewind selects the saved active-charge frame");
  RewindKeyDown(SDLK_RETURN,0);
  check(!snes_rewind_is_open(),"keyboard rewind commit closes the filmstrip");
  same(saved,n,actual,RtlSaveSnapshotToMemory(actual,cap),"rewind restores both charging actors and native world exactly");
  GameReset();
  coop_frames(1,0,0);
  for(unsigned t=0;t<90;++t) {coop_frames(1,0,0);snes_rewind_note_frame();}
  MmxCoopState s=MmxCoopGetState();
  check(MmxCoopValidState(&s) && s.players[0].status==MMX_COOP_ALIVE &&
      s.players[1].status==MMX_COOP_ALIVE && !s.controller_pass,
      "both actors resume gameplay after rewind with no stuck controller pass");
  check(snes_rewind_open(),"rewind remains available on the resumed timeline");
  for(unsigned t=0;t<80;++t) snes_rewind_step(-1);
  snes_rewind_commit();GameReset();coop_frames(30,0,0);
  s=MmxCoopGetState();check(MmxCoopValidState(&s) && !s.controller_pass,"oldest co-op rewind resumes native tasks");
  for(unsigned t=0;t<30;++t) {coop_frames(1,0,0);snes_rewind_note_frame();}
  check(snes_rewind_open(),"rewind opens for the real host modal input check");
  g_renderer_funcs.BeginDraw=rewind_modal_begin;g_renderer_funcs.EndDraw=rewind_modal_end;
  rewind_modal_presents=0;g_input_state=SNES_PAD_B;g_rewind_hotkey=1;
  bool running=true;RunRewindLoop(&running);
  check(running && rewind_modal_presents>=3,"held jump button cannot instantly cancel the rewind modal");
  check(!g_rewind_hotkey && !snes_rewind_is_open(),"closing rewind consumes F8 instead of reopening it");
  g_input_state=0;g_overlay_release_mask=0;
  snes_rewind_shutdown();free(saved);free(actual);
  puts("MMX CO-OP REWIND CHECKS PASSED");
}
static void owner_cart_checks(const char *fixture) {
  for(unsigned anchor=0;anchor<2;++anchor) {
    coop_followup_fixture(fixture);
    MmxCoopState s=MmxCoopGetState();unsigned x=coop_x(&s.players[0]),y=coop_y(&s.players[0])-48;
    memset(g_ram+0xe68,0,0x3c0);
    unsigned d=0xe68;g_ram[d]=1;g_ram[d+10]=0x2b;
    eagle_putword(g_ram+d+5,x);eagle_putword(g_ram+d+8,y+24);
    coop_frames(1,0,0);
    for(unsigned seat=0;seat<2;++seat) {
      MmxCoopSelect(g_ram,seat);
      eagle_putword(g_ram+0xbad,x+(seat?8:0));eagle_putword(g_ram+0xbca,x+(seat?8:0));
      eagle_putword(g_ram+0xbb0,y);eagle_putword(g_ram+0xbcc,y-2);
      eagle_putword(g_ram+0xbc4,0xfd00);
      g_ram[0xbaa]=8;g_ram[0xbab]=2;g_ram[0xbd3]=0;g_ram[0xbd4]=0;
    }
    MmxCoopSelect(g_ram,anchor);s=MmxCoopGetState();s.anchor=(uint8_t)anchor;MmxCoopSetState(&s);
    unsigned riders=0,carried=0;
    for(unsigned t=0;t<25;++t) {
      unsigned before_x=eagle_word(g_ram+d+5);
      coop_frames(1,0,0);s=MmxCoopGetState();riders|=g_ram[d+0x2c]&6;
      fprintf(stderr,"cart anchor=%u t=%u cart=%u/%u riders=%02x old=%02x p1=%u/%u p2=%u/%u ground=%02x/%02x\n",
          anchor,t,eagle_word(g_ram+d+5),eagle_word(g_ram+d+8),g_ram[d+0x2c],g_ram[d+0x38],
          coop_x(&s.players[0]),coop_y(&s.players[0]),coop_x(&s.players[1]),coop_y(&s.players[1]),
          s.players[0].body[0x7e],s.players[1].body[0x7e]);
      if((g_ram[d+0x2c]&6)==6 && eagle_word(g_ram+d+5)!=before_x) {
        check(coop_y(&s.players[0])==coop_y(&s.players[1]),"both minecart riders share its native support height");
        ++carried;
      }
    }
    check(riders==6 && carried>5,"both players board and travel on the native minecart with either anchor");
    check(s.current==anchor && !s.controller_pass,"minecart carry restores the world actor's controller context");
    MmxCoopSelect(g_ram,0);eagle_putword(g_ram+0xbad,eagle_word(g_ram+d+5)-72);
    eagle_putword(g_ram+0xbb0,y+48);MmxCoopSelect(g_ram,anchor);
    coop_frames(12,0,0);s=MmxCoopGetState();
    check((g_ram[d+0x2c]&6)==4 && (s.players[1].body[0x7e]&32) &&
        coop_y(&s.players[0])>coop_y(&s.players[1]),"P2 rides alone while P1 stays off the cart");
    const size_t cap=2u*1024u*1024u;uint8_t *before=malloc(cap),*expected=malloc(cap),*actual=malloc(cap);
    check(before && expected && actual,"cart replay buffers allocate");
    size_t n=RtlSaveSnapshotToMemory(before,cap);coop_frames(5,0,0);
    size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(n && RtlLoadSnapshotFromMemory(before,n),"moving two-seat cart snapshot restores");
    coop_frames(5,0,0);same(expected,en,actual,RtlSaveSnapshotToMemory(actual,cap),"cart rider ownership survives an exact snapshot replay");
    free(before);free(expected);free(actual);
  }
  puts("MMX CO-OP MINECART CHECKS PASSED");
}
static void owner_charged_checks(const char *fixture) {
  coop_followup_fixture(fixture);g_ram[0x1f99]|=8;
  for(unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);memset(g_ram+0x1228,0,512);memset(g_ram+0xc98,0,0x180);
    g_ram[0xbdd]=g_ram[0xc01]=g_ram[0xc03]=g_ram[0xc2a]=0;
  }
  MmxCoopSelect(g_ram,0);
  coop_frames(201,SNES_PAD_Y,SNES_PAD_Y);
  /* Set X's native full-charge latch explicitly so the render check
   * always includes his charge release beside Zero's delayed muzzle. */
  MmxCoopSelect(g_ram,0);g_ram[0xc00]=4;g_ram[0xc02]=g_ram[0xc03]=3;g_ram[0xbff]=0;
  unsigned owners=0;
  for(unsigned t=0;t<24;++t) {
    coop_frames(1,0,0);MmxCoopState s=MmxCoopGetState();
    for(unsigned seat=0;seat<2;++seat) for(unsigned i=0;i<8;++i) {
      const uint8_t *q=s.players[seat].shots+i*64;
      if(q[0] && (q[22]==0x0e || q[22]==0x9e)) owners|=1u<<seat;
    }
    if(t==3 || t==8 || t==13) {
      char suffix[48];snprintf(suffix,sizeof(suffix),"real-dual-charge-%u",t);owner_wide_image(suffix);
    }
  }
  check(owners==3,"simultaneous full-charge release creates native beams from both players");
}
static void owner_left_edge_checks(const char *fixture) {
  coop_followup_fixture(fixture);
  g_ram[0x1f7a]=2;eagle_putword(g_ram+0x1e4d,0);eagle_putword(g_ram+0x1e56,0);
  MmxCoopState s=MmxCoopGetState();s.stage=2;s.stage_pending=0;MmxCoopSetState(&s);
  for(unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);eagle_putword(g_ram+0xbad,10);eagle_putword(g_ram+0xbca,10);
  }
  MmxCoopSelect(g_ram,0);
  for(unsigned t=0;t<30;++t) {
    coop_frames(1,SNES_PAD_LEFT,SNES_PAD_LEFT);s=MmxCoopGetState();
    check(coop_x(&s.players[0])>=8 && coop_x(&s.players[0])<=10 &&
        coop_x(&s.players[1])>=8 && coop_x(&s.players[1])<=10,
        "both actors stop at Sting Chameleon's authored left edge without wrapping");
  }
}
static void owner_wide_checks(const char *directory,const char *fixture) {
  g_config.widescreen=true;g_mmx_custom_aspect=MMX_ASPECT_16_9;
  int w,h;MmxPrepareFrame(1280,720,&w,&h);
  owner_heart_checks(fixture);
  owner_rewind_checks(fixture);
  owner_cart_checks(fixture);
  owner_left_edge_checks(fixture);
  owner_wide_load(directory,8);coop_frames(30,0,0);
  MmxCoopState s=MmxCoopGetState();
  check(s.anchor==1 && (g_ram[0xe22]&0x40),"owner fixture has P2 riding the occupied armor");
  /* Slot 09 was saved after the old rejoin bug; withdraw its already-live
   * P1 to exercise a clean return with the repaired pilot still seated. */
  s.players[0].status=MMX_COOP_ABSENT;MmxCoopSetState(&s);
  coop_frames(1,SNES_PAD_SELECT,0);coop_frames(60,0,0);s=MmxCoopGetState();
  check(s.players[0].status==MMX_COOP_ALIVE && !s.players[0].zero.swap_phase,"P1 completes return beside P2 armor");
  check(s.anchor==1 && s.current==1,"returning P1 preserves the armor pilot's projection");
  unsigned armor_x=eagle_word(g_ram+0xe1d),p1_x=coop_x(&s.players[0]);
  coop_frames(20,0,SNES_PAD_RIGHT);s=MmxCoopGetState();
  check(eagle_word(g_ram+0xe1d)>armor_x && coop_x(&s.players[0])==p1_x,
      "only P2's controls move P2's armor after P1 rejoins");
  check(coop_x(&s.players[1])==eagle_word(g_ram+0xe1d) &&
      (s.players[1].body[22]==0x6a || s.players[1].body[22]==0x6b),
      "P2 stays seated in the moving armor");owner_wide_image("armor-rejoin");
  for(unsigned slot=5;slot<=6;++slot) for(unsigned dash=0;dash<2;++dash) {
    owner_wide_load(directory,slot);
    for(unsigned t=0;t<(slot==6?200:100);++t) {
      coop_frames(1,SNES_PAD_RIGHT|(dash?SNES_PAD_A:0)|
          (slot==6 && t>=70 && t<100?SNES_PAD_B:0),0);
      if(t%20==0) {
        fprintf(stderr,"wide slot=%u dash=%u tick=%u cam=%u player=%u section=%u\n",slot,dash,t,eagle_word(g_ram+0x1e4d),eagle_word(g_ram+0xbad),g_ram[0x1f08]);
        for(unsigned d=0xe68;d<0x1228;d+=64) if(g_ram[d])
          fprintf(stderr," enemy=%04x id=%02x xy=%u/%u visible=%02x animation=%02x pose=%02x tile=%02x palette=%02x\n",
              d,g_ram[d+10],eagle_word(g_ram+d+5),eagle_word(g_ram+d+8),g_ram[d+14],g_ram[d+22],g_ram[d+23],g_ram[d+24],g_ram[d+17]);
        char suffix[64];snprintf(suffix,sizeof(suffix),"slot-%u-%s-%u",slot+1,dash?"dash":"walk",t);owner_wide_image(suffix);
      }
    }
  }
  owner_wide_load(directory,1);
  for(unsigned t=0;t<100;++t) {
    coop_frames(1,SNES_PAD_RIGHT,0);
    if(t==0 || t==40 || t==80) {char name[48];snprintf(name,sizeof(name),"slimer-%u",t);owner_wide_image(name);}
  }
  coop_graphics_followup_checks(fixture);
  owner_charged_checks(fixture);
  puts("MMX OWNER WIDESCREEN CO-OP CHECKS PASSED");
}

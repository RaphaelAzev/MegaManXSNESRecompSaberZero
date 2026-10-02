/* Optional regressions using private copies of the owner's three Eagle saves.
 * Fixtures/ROMs are never distributed or written back to the owner install. */
static unsigned eagle_word(const uint8_t *p) {return p[0]|p[1]<<8;}
static void eagle_putword(uint8_t *p,unsigned n) {p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void eagle_fixture(const char *directory,unsigned slot,unsigned driver) {
  char path[4096];snprintf(path,sizeof(path),"%s/save%u.sav",directory,slot);
  check(MmxCoopEnable(0) && RtlLoadSnapshot(path),"private Eagle fixture loads");
  MmxCoopState s=MmxCoopGetState();
  if(driver) for(unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);
    memcpy(g_ram+0xbac,s.players[seat^1].body+4,6);
  }
  MmxCoopSelect(g_ram,0);
}
static void coop_eagle_checks(const char *directory,uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  for(unsigned anchor=0;anchor<2;++anchor) {
    eagle_fixture(directory,0,0);
    unsigned d=0x16b8,x=eagle_word(g_ram+d+5),y=eagle_word(g_ram+d+8)-24;
    for(unsigned seat=0;seat<2;++seat) {
      MmxCoopSelect(g_ram,seat);
      eagle_putword(g_ram+0xbad,x);eagle_putword(g_ram+0xbb0,y);
      eagle_putword(g_ram+0xbca,x);eagle_putword(g_ram+0xbcc,y);
      eagle_putword(g_ram+0xbc4,0);g_ram[0xbaa]=4;g_ram[0xbab]=0;
      g_ram[0xbd3]=g_ram[0xbd4]=0;
    }
    MmxCoopSelect(g_ram,anchor);MmxCoopState s=MmxCoopGetState();s.anchor=(uint8_t)anchor;MmxCoopSetState(&s);
    coop_frames(90,0,0);s=MmxCoopGetState();
    check((s.players[0].body[0x27]&127) && (s.players[1].body[0x27]&127),"both survive original moving platform");
    check(g_ram[d+0x2c]==3 && coop_y(&s.players[0])==coop_y(&s.players[1]) &&
        eagle_word(g_ram+d+8)-coop_y(&s.players[1])==32,"both riders follow native diagonal platform movement");
    size_t n=RtlSaveSnapshotToMemory(start,cap);
    coop_frames(12,0,0);size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n),"two rider snapshot restores");coop_frames(12,0,0);
    same(expected,en,actual,RtlSaveSnapshotToMemory(actual,cap),"platform rider ownership replays deterministically");
    coop_frames(8,0,SNES_PAD_B);s=MmxCoopGetState();
    check(g_ram[d+0x2c]==1 && coop_y(&s.players[1])+20<coop_y(&s.players[0]),"P2 jumps independently while P1 keeps riding");
  }
  for(unsigned driver=0;driver<2;++driver) for(unsigned slot=1;slot<=2;++slot) {
    eagle_fixture(directory,slot,driver);
    bool departed=false,hidden=false,returned=false,boss=false;
    unsigned hp[2];MmxCoopState s=MmxCoopGetState();
    for(unsigned i=0;i<2;++i) hp[i]=s.players[i].body[0x27]&127;
    for(unsigned t=0;t<950;++t) {
      unsigned pad=departed?0:slot==1?SNES_PAD_LEFT:SNES_PAD_RIGHT;
      coop_frames(1,driver?0:pad,driver?pad:0);s=MmxCoopGetState();
      departed|=s.scene_phase==1;hidden|=s.scene_phase==2;
      if(slot==2 && g_ram[0xea8] && g_ram[0xeb2]==0x48 && g_ram[0xea9]==2 && g_ram[0xeaa]>=2)
        check(s.scene_owner==driver+1,"lift keeps scene ownership through ascent and destruction");
      for(unsigned d=0xe68;d<0x1228;d+=64)
        boss|=g_ram[d] && g_ram[d+10]==0x52 && g_ram[d+1]==4;
      if(hidden && !s.scene_phase) {
        returned=true;
        if(slot==1 || boss) break;
      }
    }
    check(departed && hidden && returned && s.anchor==driver,"either seat drives the scene and retrieves its partner");
    check((s.players[0].body[0x27]&127)==hp[0] && (s.players[1].body[0x27]&127)==hp[1],"scene transport preserves both health pools");
    if(slot==1) check(coop_x(&s.players[0])<0x17e8 && coop_x(&s.players[1])<0x17e8,"both return inside the shut Eagle door");
    else check(boss && coop_y(&s.players[0])<0x100 && coop_y(&s.players[1])<0x100,"both reach Eagle combat above the destroyed ship");
    coop_image(driver,slot==1?"eagle-door":"eagle-arena");
  }
  puts("MMX CO-OP EAGLE CHECKS PASSED");
}

static void coop_x1_effect_checks(const char *fixture) {
  check(MmxCoopEnable(0) && RtlLoadSnapshot(fixture),"X1 effect fixture loads");
  check(g_ram[0xbdb]==4,"Sting fixture has native Chameleon Sting selected");
  unsigned visible=0,invisible=0;
  uint32_t pixels[256*224];
  for(unsigned t=0;t<270;++t) {
    coop_frames(1,t<220?SNES_PAD_Y:0,0);
    if(t<230) continue;
    check(MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false),"charged Sting renders");
    const uint32_t *stock=MmxRendererStockFrame();MmxCoopState s=MmxCoopGetState();
    int x=(int)coop_x(&s.players[0])-eagle_word(g_ram+0x1e4d),y=(int)coop_y(&s.players[0])-eagle_word(g_ram+0x1e50);
    unsigned different=0;
    for(int row=y-27;row<y+15;++row) for(int col=x-16;col<x+9;++col)
      different+=(pixels[row*256+col]&0xffffff)!=(stock[row*256+col]&0xffffff);
    check(!different,"charged Sting body and armor match native PPU colors and blink");
    visible+=g_ram[0xbb6]!=0;invisible+=g_ram[0xbb6]==0;
  }
  check(visible && invisible,"Sting comparison covers visible and invisible frames");
  coop_image(0,"native-sting");
  /* Compare the split's pixels against a reference run of the original PPU.
   * On replay P2 changes weapons as it splits, overwriting shared weapon CHR.
   * Co-op's private art must preserve the reference without rewriting VRAM. */
  uint32_t *reference=malloc(70*256*224*sizeof(uint32_t));check(reference!=NULL,"Spark reference allocation");
  unsigned overwritten=0,split=0;
  for(unsigned pass=0;pass<2;++pass) {
    check(MmxCoopEnable(0) && RtlLoadSnapshot(fixture),"Spark fixture restores");
    for(unsigned t=0;t<70;++t) {
      unsigned p1=t<6?((t&1)?0:SNES_PAD_R):t==10?SNES_PAD_LEFT:t==20?SNES_PAD_Y:0;
      coop_frames(1,p1,pass && t==30?SNES_PAD_R:0);
      const uint32_t *stock=MmxRendererStockFrame();
      if(!pass) {memcpy(reference+t*256*224,stock,256*224*4);continue;}
      if(t<32) continue;
      check(MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false),"split Spark renders");
      unsigned different=0;
      /* Stay below the co-op HUD, which deliberately differs from retail. */
      for(int row=96;row<210;++row) for(int col=55;col<90;++col) {
        unsigned i=row*256+col;uint32_t wanted=reference[t*256*224+i]&0xffffff;
        different+=(pixels[i]&0xffffff)!=wanted;
        overwritten+=(stock[i]&0xffffff)!=wanted;
      }
      if(different) {
        fprintf(stderr,"Spark t=%u mismatched pixels=%u\n",t,different);
        for(unsigned i=0;i<2;++i) {
          FILE *f=fopen(i?"spark-actual.ppm":"spark-reference.ppm","wb");
          const uint32_t *p=i?pixels:reference+t*256*224;
          if(f) {fprintf(f,"P6\n256 224\n255\n");for(unsigned n=0;n<256*224;++n) {
            uint8_t rgb[3]={p[n]>>16,p[n]>>8,p[n]};fwrite(rgb,1,3,f);
          }fclose(f);}
        }
      }
      check(!different,"split Spark stays pixel-identical when partner replaces shared weapon art");
      for(unsigned d=0x1228;d<0x1428;d+=64) split+=g_ram[d] && g_ram[d+10]==12 && g_ram[d+11]==1;
    }
  }
  free(reference);check(split && overwritten,"Spark regression exercises split particles and real VRAM interference");
  coop_image(0,"native-spark-split");
  puts("MMX CO-OP X1 EFFECT CHECKS PASSED");
}

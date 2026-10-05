/* Private Launch Octopus owner saves: slot 04 Gulpfer, slot 07 vortex. */
static void octopus_replay(uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  size_t n=RtlSaveSnapshotToMemory(start,cap);
  coop_frames(8,0,0);size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"Launch interaction snapshot restores");
  coop_frames(8,0,0);
  same(expected,en,actual,RtlSaveSnapshotToMemory(actual,cap),"Launch interaction replays deterministically");
}
static void octopus_fish_checks(const char *dir,uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  for(unsigned roster=0;roster<2;++roster) for(unsigned target=0;target<2;++target) {
    owner_wide_load(dir,3);kuwanger_roster(roster,target^1);
    unsigned d=0xe68,x=eagle_word(g_ram+d+5),y=eagle_word(g_ram+d+8);
    uint8_t fresh[64];memcpy(fresh,g_ram+d,64);
    /* Re-enrol through the actual placement path: a returned X must still
     * be capturable, regardless of character assignment or spawn order. */
    MmxCoopState s=MmxCoopGetState();s.players[target].status=MMX_COOP_ABSENT;
    MmxCoopSetState(&s);
    check(MmxCoopPlacePartner(g_ram,x+24,y+12),"returned player is placed beside Gulpfer");
    kuwanger_place(target,x+24,y+12);kuwanger_place(target^1,x+140,y+76);
    MmxCoopSelect(g_ram,target);eagle_putword(g_ram+0xbc4,0);
    MmxCoopCapture(g_ram);MmxCoopSelect(g_ram,target^1);
    g_ram[d+1]=4;g_ram[d+2]=2;g_ram[d+3]=0;g_ram[d+17]|=64;
    coop_frames(30,0,0);s=MmxCoopGetState();
    check(g_ram[d+0x3d] && g_ram[d+0x3f]==target+1 &&
        !s.players[target].body[14] && s.players[target].body[0x30] &&
        s.players[target^1].body[14] && !s.players[target^1].body[0x30],
        "either returned character is swallowed without capturing its partner");
    octopus_replay(start,expected,actual,cap);
    g_ram[d+0x3f]=0; /* Old saves predate the ownership byte. */
    coop_frames(1,0,0);
    check(g_ram[d+0x3f]==target+1,"old swallowed-player saves recover the capture owner");
    /* The free fish must skip the nearer body already inside another fish;
     * the holding fish must keep its own victim as the partner approaches. */
    x=eagle_word(g_ram+d+5);y=eagle_word(g_ram+d+8);
    unsigned other=d+64;memcpy(g_ram+other,fresh,64);
    eagle_putword(g_ram+other+5,x);eagle_putword(g_ram+other+8,y);
    g_ram[other+1]=4;g_ram[other+2]=2;g_ram[other+3]=0;g_ram[other+17]|=64;
    kuwanger_place(target^1,x+24,y+12);
    MmxCoopSelect(g_ram,target^1);eagle_putword(g_ram+0xbc4,0);MmxCoopCapture(g_ram);
    coop_frames(30,0,0);s=MmxCoopGetState();
    check(g_ram[d+0x3f]==target+1 && g_ram[other+0x3f]==(target^1)+1 &&
        g_ram[d+0x3d] && g_ram[other+0x3d] &&
        !s.players[0].body[14] && !s.players[1].body[14],
        "two fish retain separate victims and can swallow both players");
    octopus_replay(start,expected,actual,cap);
    g_ram[d+0x27]=1;coop_frames(1,SNES_PAD_B,SNES_PAD_B);coop_frames(2,0,0);s=MmxCoopGetState();
    check(s.players[target].body[14] && !s.players[target].body[0x30] &&
        !s.players[target^1].body[14] && s.players[target^1].body[0x30],
        "killing one fish releases only its own captured player");
  }
}
static void octopus_current_checks(const char *dir,uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  for(unsigned roster=0;roster<2;++roster) for(unsigned anchor=0;anchor<2;++anchor) {
    owner_wide_load(dir,6);kuwanger_roster(roster,anchor);
    MmxCoopState before=MmxCoopGetState();
    coop_frames(20,0,0);MmxCoopState s=MmxCoopGetState();
    check(s.players[0].body[0x3c] && s.players[1].body[0x3c] &&
        coop_y(&s.players[1])+10<coop_y(&before.players[1]),
        "vortex pulls and lifts both characters with either world actor");
    octopus_replay(start,expected,actual,cap);
    kuwanger_place(0,4700,650);kuwanger_place(1,4700,650);coop_frames(2,0,0);s=MmxCoopGetState();
    check(!s.players[0].body[0x3c] && !s.players[1].body[0x3c],"both players leave the vortex contact state");
  }
}
static void octopus_ice_checks(const char *dir,uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  for(unsigned roster=0;roster<2;++roster) {
    owner_wide_load(dir,3);kuwanger_roster(roster,0);memset(g_ram+0xe68,0,0x3c0);
    kuwanger_place(0,3589,575);kuwanger_place(1,3621,575);coop_frames(20,0,0);
    MmxCoopSelect(g_ram,0);
    memset(g_ram+0x1228,0,512);g_ram[0x1228]=1;g_ram[0x1232]=0x17;g_ram[0xbdd]=1;
    g_ram[0x1239]=0x46;coop_frames(50,0,0);
    unsigned x=eagle_word(g_ram+0x122d),y=eagle_word(g_ram+0x1230);
    kuwanger_place(0,x-4,y-40);kuwanger_place(1,x+4,y-40);coop_frames(32,0,0);
    MmxCoopState s=MmxCoopGetState();
    check(s.players[0].body[2]==0 && s.players[1].body[2]==0 &&
        (s.players[0].body[0x2c]&4) && (s.players[1].body[0x2c]&4),
        "both ice-sled riders enter standing action rather than falling");
    unsigned p1=coop_x(&s.players[0]),p2=coop_x(&s.players[1]);coop_frames(35,0,0);s=MmxCoopGetState();
    check(coop_x(&s.players[0])+8<p1 && coop_x(&s.players[1])+8<p2 &&
        s.players[0].body[2]==0 && s.players[1].body[2]==0,
        "moving ice sled carries both standing riders");
    octopus_replay(start,expected,actual,cap);modern_image("octopus-ice-riders");
  }
}
static void octopus_buster_checks(const char *dir) {
  owner_wide_load(dir,3);kuwanger_roster(0,0);memset(g_ram+0xe68,0,0x3c0);
  for(unsigned seat=0;seat<2;++seat) {
    coop_weapon_select(seat,0,0);MmxCoopSelect(g_ram,seat);g_ram[0xbdb]=0;MmxCoopCapture(g_ram);
  }
  MmxCoopSelect(g_ram,0);memset(g_ram+0x1228,0,512);memset(g_ram+0xc98,0,0x180);
  g_ram[0x1228]=1;g_ram[0x1232]=1;g_ram[0xbdd]=1;
  MmxCoopSelect(g_ram,1);MmxZeroState z=MmxZeroGetState();z.charge=201;MmxZeroSetState(z);
  MmxCoopSelect(g_ram,0);bool partial=false;unsigned seats=0;
  for(unsigned t=0;t<30;++t) {
    coop_frames(1,0,0);MmxCoopState s=MmxCoopGetState();
    for(unsigned seat=0;seat<2;++seat) for(unsigned slot=0;slot<8;++slot) {
      const uint8_t *q=s.players[seat].shots+slot*64;
      if(q[0] && q[10]>=1 && q[10]<=3) seats|=1u<<seat;
      if(q[0] && q[22]==0x0e && (q[23]&127)==6) partial=true;
    }
    if(t==10 || t==17) {
      uint32_t original[256*224],poisoned[256*224];uint16_t saved[512],oam[256];
      memcpy(saved,g_ppu->vram+0x6200,sizeof(saved));memcpy(oam,g_ppu->oam,sizeof(oam));
      uint8_t dma=g_ram[0xa3];g_ram[0xa3]=0;unsigned pixels=0;
      for(unsigned seat=0;seat<2;++seat) {
        MmxCoopState isolated=s;unsigned partner=s.current^1;
        memset(isolated.players[s.current].shots,0,sizeof(isolated.players[s.current].shots));
        memcpy(isolated.players[partner].shots,s.players[seat].shots,sizeof(isolated.players[partner].shots));
        memset(isolated.players[partner].body,0,sizeof(isolated.players[partner].body));
        memset(isolated.players[partner].auxiliaries,0,sizeof(isolated.players[partner].auxiliaries));
        for(unsigned i=0;i<128;++i)g_ppu->oam[i*2]=0xf000;
        memcpy(g_ppu->vram+0x6200,saved,sizeof(saved));MmxRendererReset();coop_x_graphics_draw(original,&isolated);
        memset(g_ppu->vram+0x6200,0,sizeof(saved));coop_x_graphics_draw(poisoned,&isolated);
        pixels+=coop_buster_pixels_equal(original,poisoned,&isolated);
      }
      check(pixels>100,"both charged beams render independently of shared weapon VRAM");
      memcpy(g_ppu->vram+0x6200,saved,sizeof(saved));memcpy(g_ppu->oam,oam,sizeof(oam));g_ram[0xa3]=dma;
    }
    if(t==11)modern_image("octopus-dual-buster");
  }
  const MmxSpriteAsset *art=MmxRenderAssetsChargedBuster(0x0e,6);
  check(art && !memcmp(art->tiles+0x31*32,g_snes->cart->rom+0x178720,32),
      "short buster upload preserves the original preceding bottom-right tile");
  check(partial && seats==3,"simultaneous native busters cover both seats and the short-upload pose");
}
static void coop_octopus_checks(const char *fixture,uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  (void)fixture;const char *dir=getenv("MMX_COOP_OCTOPUS_TEST");
  octopus_fish_checks(dir,start,expected,actual,cap);
  octopus_current_checks(dir,start,expected,actual,cap);
  /* The synthetic projectile placement uses the owner's unified-camera
   * fixture; independent views can cull it before the imposed landing. */
  if(!getenv("MMX_COOP_ONLINE_FIXTURE")) {
    octopus_ice_checks(dir,start,expected,actual,cap);
    octopus_buster_checks(dir);
  } else puts("Independent-view fixture checks fish and vortex; projectile fixtures require unified view");
  MmxCoopDisable();puts("MMX LAUNCH OCTOPUS CO-OP CHECKS PASSED");
}

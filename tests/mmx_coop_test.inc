/* Private ROM-backed controller/context checks. No owner save is overwritten. */
#include "mmx_coop.h"
static unsigned coop_x(const MmxCoopPlayer *p) { return p->body[5] | p->body[6] << 8; }
static unsigned coop_y(const MmxCoopPlayer *p) { return p->body[8] | p->body[9] << 8; }
static void coop_frames(unsigned count, unsigned p1, unsigned p2) {
  while (count--) frame(p1 | p2 << 12);
}
static void coop_image(unsigned character,const char *action) {
  const char *base = getenv("MMX_ZERO_TEST_CAPTURE");
  if (!base) return;
  uint32_t pixels[256*224];
  check(MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false), "two-player frame renders");
  MmxCoopState snapshot = MmxCoopGetState();
  uint32_t without_partner[256*224];
  MmxRendererCoopFrame(NULL);
  check(MmxRendererDraw(without_partner,(MmxRenderView){256,0,4.0/3.0},false), "solo render reference draws");
  MmxRendererCoopFrame(&snapshot);
  unsigned changed = 0;
  for (unsigned i=0;i<256*224;++i) changed += pixels[i] != without_partner[i];
  if (!snapshot.menu_owner && snapshot.scene_phase!=2 && snapshot.players[snapshot.current^1].status==MMX_COOP_ALIVE)
    check(changed>100,"partner contributes visible original sprite pixels");
  char path[4096]; snprintf(path,sizeof(path),"%s.coop-%u-%s.bmp",base,character,action);
  FILE *f = fopen(path,"wb"); check(f!=NULL,"private co-op image opens");
  const uint8_t signature[2] = {'B','M'};
  uint32_t header[] = {54+sizeof(pixels),0,54,40,256,224,0x200001,0,sizeof(pixels),0,0,0,0};
  check(fwrite(signature,2,1,f)==1 && fwrite(header,sizeof(header),1,f)==1,"private image header writes");
  bool written = true;
  for (int y=223;y>=0;--y) written &= fwrite(pixels+y*256,1024,1,f)==1;
  check(fclose(f)==0 && written,"private co-op image writes");
  snprintf(path,sizeof(path),"%s.coop-%u-%s.cap",base,character,action);
  check(MmxRendererSaveCapture(path),"two-player capture writes");
}
/* Encounter fixtures place P2 explicitly; automatic arrival itself is
 * exercised by coop_join_checks and the keyboard check below. */
static bool coop_place_fixture_partner(unsigned x,unsigned y) {
  MmxCoopState s=MmxCoopGetState();
  s.players[1].status=MMX_COOP_ABSENT;s.players[1].zero.swap_phase=0;
  s.stage_pending=0;MmxCoopSetState(&s);
  return MmxCoopPlacePartner(g_ram,(uint16_t)x,(uint16_t)y);
}
static void coop_contact_checks(uint8_t *start,size_t cap) {
  unsigned enemy = 0;
  for (unsigned i=0;i<400 && !enemy;++i) {
    coop_frames(1,SNES_PAD_RIGHT,SNES_PAD_RIGHT);
    for (unsigned d=0xe68;d<0x1228;d+=64)
      if (g_ram[d] && g_ram[d+14] && (g_ram[d+0x27]&127) &&
          (g_ram[d+0x20]|g_ram[d+0x21])) {enemy=d;break;}
  }
  check(enemy!=0,"native highway enemy supplies two-player collision target");
  size_t n = RtlSaveSnapshotToMemory(start,cap);
  for (unsigned both=0;both<2;++both) {
    check(RtlLoadSnapshotFromMemory(start,n),"restore private contact encounter");
    unsigned ex=g_ram[enemy+5]|g_ram[enemy+6]<<8, ey=g_ram[enemy+8]|g_ram[enemy+9]<<8;
    unsigned hp[2];
    for (unsigned seat=0;seat<2;++seat) {
      MmxCoopSelect(g_ram,seat);
      unsigned x=ex-(!both && !seat ? 80 : 0);
      g_ram[0xbad]=(uint8_t)x;g_ram[0xbae]=(uint8_t)(x>>8);
      g_ram[0xbb0]=(uint8_t)ey;g_ram[0xbb1]=(uint8_t)(ey>>8);
      g_ram[0xc30]=g_ram[0xc32]=0;
      g_ram[0xbcf]=g_ram[0x1f9a]|128;hp[seat]=g_ram[0xbcf]&127;
    }
    MmxCoopSelect(g_ram,0);
    coop_frames(3,0,0); /* Native visibility updates after the first contact pass. */
    MmxCoopState s=MmxCoopGetState();
    fprintf(stderr,"contact both=%u enemy=%04x kind=%02x damage=%u hp=%u/%u -> %u/%u pass=%u\n",
      both,enemy,g_ram[enemy+10],g_ram[enemy+0x26],hp[0],hp[1],
      s.players[0].body[0x27]&127,s.players[1].body[0x27]&127,s.contact_pass);
    check((s.players[1].body[0x27]&127)<hp[1],"enemy contact damages P2 independently");
    check(both ? (s.players[0].body[0x27]&127)<hp[0] : (s.players[0].body[0x27]&127)==hp[0],
      "contact damages only overlapping players");
    check(!s.contact_pass && s.current==0,"native contact restores its caller context");
  }
  check(RtlLoadSnapshotFromMemory(start,n),"restore P2 projectile encounter");
  coop_frames(1,0,SNES_PAD_Y);
  MmxCoopSelect(g_ram,1);
  unsigned shot=0;
  for (unsigned d=0x1228;d<0x1428;d+=64) if(g_ram[d]) {shot=d;break;}
  check(shot!=0,"P2 buster exists for native enemy collision");
  memcpy(g_ram+shot+5,g_ram+enemy+5,2);memcpy(g_ram+shot+8,g_ram+enemy+8,2);
  g_ram[enemy+0x27]=32;g_ram[enemy+0x30]=0;
  MmxCoopSelect(g_ram,0);
  coop_frames(3,0,0);
  fprintf(stderr,"P2 buster enemy HP=%u contact=%u\n",g_ram[enemy+0x27]&127,MmxCoopGetState().contact_pass);
  check((g_ram[enemy+0x27]&127)==31,"P2 native buster deals one X1 buster hit");
}
static void coop_weapon_select(unsigned seat,unsigned page,unsigned weapon) {
  MmxCoopSelect(g_ram,seat);
  MmxWeaponsCancelShots(g_ram);MmxZeroCancel(g_ram);
  MmxWeaponsState w=MmxWeaponsGetState();w.page=(uint8_t)page;w.weapon=(uint8_t)weapon;
  MmxWeaponsSetState(w);MmxCoopCapture(g_ram);MmxCoopSelect(g_ram,0);
}
static void coop_weapon_checks(unsigned character) {
  for(unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);MmxWeaponsState w=MmxWeaponsGetState();
    memset(w.energy,28,sizeof(w.energy));memset(w.fraction,0,sizeof(w.fraction));MmxWeaponsSetState(w);
  }
  MmxCoopSelect(g_ram,0);
  coop_weapon_select(0,2,1);coop_weapon_select(1,2,4);
  coop_frames(1,0,SNES_PAD_Y);coop_frames(6,0,0);
  MmxCoopState s=MmxCoopGetState();
  check(s.players[0].weapons.energy[8]==28 && s.players[1].weapons.energy[11]<28,
      "P2 special consumes only P2 weapon energy");
  check(s.players[1].combat.shots[0].active && s.players[1].combat.shots[0].weapon==4,
      "P2 imported projectile advances independently");
  coop_frames(1,SNES_PAD_Y,0);coop_frames(4,0,0);
  s=MmxCoopGetState();
  check(s.players[0].combat.shots[0].active && s.players[0].combat.shots[0].weapon==1 &&
      s.players[1].combat.shots[0].active && s.players[1].combat.shots[0].weapon==4,
      "different imported weapons coexist in their own pools");
  coop_image(character,"specials");
  coop_weapon_select(0,2,5);coop_weapon_select(1,2,5);
  coop_frames(1,SNES_PAD_Y,SNES_PAD_Y);coop_frames(9,0,0);
  unsigned total=0;
  g_ram[0xe68]=1;g_ram[0xe69]=2;g_ram[0xe72]=0x1f;g_ram[0xe8f]=32;g_ram[0xe90]=3;
  for (unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);
    MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
    for (unsigned i=0;i<8;++i) if(c.shots[i].active && c.shots[i].variant>=2) {slot=i;break;}
    check(slot<8,"each player has a source Ray Splasher projectile");
    unsigned damage=MmxWeaponsDamage(g_ram,0xe68,0x1228+slot*64,1);
    total+=damage;g_ram[0xe8f]-=(uint8_t)damage;
  }
  MmxCoopSelect(g_ram,0);
  check(total==3,"two attackers share the enemy's fractional damage carry");
  memset(g_ram+0xe68,0,64);
  coop_weapon_select(0,1,1);coop_weapon_select(1,1,1);g_ram[0x1f99]|=2;
  coop_frames(205,SNES_PAD_Y,SNES_PAD_Y);coop_frames(1,0,0);
  for (unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);
    MmxWeaponCombatState c=MmxWeaponsGetCombatState();unsigned slot=8;
    for (unsigned i=0;i<8;++i) if(c.shots[i].active && c.shots[i].charged) {slot=i;break;}
    check(slot<8,"both players create charged Crystal Hunter");
    c.shots[slot].age=(uint16_t)(200+seat);c.shots[slot].muzzle_pose=1;
    MmxWeaponsSetCombatState(c);
  }
  MmxCoopSelect(g_ram,0);
  unsigned tick=g_ram[0xb9c];coop_frames(12,0,0);
  check((uint8_t)(g_ram[0xb9c]-tick)==6,"staggered time effects halve world speed without permanently freezing it");
}
static void coop_join_checks(unsigned character,const char *fixture,uint8_t *start,
                             uint8_t *expected,uint8_t *actual,size_t cap) {
  MmxCoopDisable();check(RtlLoadSnapshot(fixture),"restore private join fixture");
  check(MmxCoopEnable(character),"join uses selected roster");
  for(unsigned i=0;i<30 && !MmxCoopTransitionActive();++i) coop_frames(1,0,0);
  check(MmxCoopTransitionActive() && MmxCoopGetState().enrolled,
      "P2 automatically arrives without controller input when co-op starts");
  g_ram[0xbcf]=7|128;
  unsigned tick=g_ram[0xb9c];
  coop_frames(10,0,0);coop_image(character,"arrival");
  for(unsigned i=0;i<80 && MmxCoopTransitionActive();++i) coop_frames(1,0,0);
  check(!MmxCoopTransitionActive() && g_ram[0xb9c]==tick,"arrival freezes the world until original morph completes");
  coop_frames(2,0,0);
  check((g_ram[0xbcf]&127)==7,"joining does not heal P1");
  MmxCoopSelect(g_ram,1);g_ram[0xbcf]=5|128;g_ram[0x1f88]=(g_ram[0x1f88]&192)|11;
  g_ram[0x1f83]=(g_ram[0x1f83]&240)|9;MmxCoopSelect(g_ram,0);
  coop_frames(89,0,SNES_PAD_SELECT);
  check(!MmxCoopTransitionActive(),"shorter than 1.5 seconds does not withdraw P2");
  coop_frames(1,0,SNES_PAD_SELECT);
  check(MmxCoopTransitionActive(),"90 gameplay frames holding P2 Select begins withdrawal");
  for(unsigned i=0;i<80 && MmxCoopTransitionActive();++i) coop_frames(1,0,0);
  MmxCoopState s=MmxCoopGetState();
  check(s.players[1].status==MMX_COOP_ABSENT && s.enrolled,"voluntary withdrawal retains session enrollment");
  check((s.players[1].body[0x27]&127)==5 && s.players[1].energy[1]==11 && (g_ram[0x1f83]&15)==9,
      "withdrawal retains personal HP/energy and shared subtanks");
  coop_frames(1,0,SNES_PAD_START);coop_frames(5,0,0);
  check(MmxCoopGetState().players[1].status==MMX_COOP_ABSENT && !MmxCoopTransitionActive(),
      "withdrawn P2 stays absent and Start does not rejoin");
  coop_frames(1,0,SNES_PAD_SELECT);coop_frames(2,0,0);
  check(MmxCoopTransitionActive(),"voluntarily absent player can rejoin");
  size_t n=RtlSaveSnapshotToMemory(start,cap);coop_frames(20,0,0);
  size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"mid-arrival snapshot restores");coop_frames(20,0,0);
  size_t an=RtlSaveSnapshotToMemory(actual,cap);
  same(expected,en,actual,an,"arrival timing, enrollment and retained stats replay exactly");
  for(unsigned i=0;i<80 && MmxCoopTransitionActive();++i) coop_frames(1,0,0);
  s=MmxCoopGetState();
  check((s.players[1].body[0x27]&127)==5 && s.players[1].energy[1]==11 && (g_ram[0x1f83]&15)==9,
      "rejoin does not refill the withdrawn player's stats");
  s.players[1].status=MMX_COOP_FALLEN;MmxCoopSetState(&s);
  coop_frames(1,0,SNES_PAD_SELECT);
  check(MmxCoopGetState().players[1].status==MMX_COOP_FALLEN && !MmxCoopTransitionActive(),
      "Select cannot revive a fallen player mid-stage");
  unsigned lives=g_ram[0x1f80];g_ram[0xbcf]=128;g_ram[0xbaa]=12;g_ram[0xbab]=0;
  bool returned=false;
  for (unsigned i=0;i<1000;++i) {
    coop_frames(1,0,0);s=MmxCoopGetState();
    if (g_ram[0x1f80]+1==lives && s.players[1].status==MMX_COOP_ALIVE &&
        !s.players[1].zero.swap_phase && !s.stage_pending && g_ram[0xd3]==4) {returned=true;break;}
  }
  check(returned && s.enrolled,"native checkpoint restart automatically returns enrolled P2 without another Select");
  check((s.players[0].body[0x27]&127)==g_ram[0x1f9a] && (s.players[1].body[0x27]&127)==g_ram[0x1f9a],
      "full team restart refills both HP pools");
}
static void coop_camera_checks(unsigned character) {
  MmxCoopState s=MmxCoopGetState();unsigned x0=coop_x(&s.players[0]);
  coop_frames(230,0,SNES_PAD_RIGHT);s=MmxCoopGetState();
  unsigned camera=g_ram[0x1e4d]|g_ram[0x1e4e]<<8;
  check(coop_x(&s.players[0])==x0 && coop_x(&s.players[1])-x0<=224,
      "P2 cannot move beyond the shared screen while P1 waits");
  check(x0>=camera+12 && coop_x(&s.players[1])<=camera+244,
      "native camera keeps both horizontal edges visible");
  coop_image(character,"camera");
  coop_frames(90,SNES_PAD_RIGHT,SNES_PAD_RIGHT);s=MmxCoopGetState();
  check(coop_x(&s.players[0])>x0+60 && (g_ram[0x1e4d]|g_ram[0x1e4e]<<8)>camera+60,
      "both players advancing releases the shared camera");
}
static void coop_menu_checks(unsigned character,uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  coop_frames(1,0,SNES_PAD_START);coop_frames(75,0,0);
  MmxCoopState s=MmxCoopGetState();
  fprintf(stderr,"P2 pause owner=%u current=%u flags=%02x/%02x cursor=%u page=%u\n",
      s.menu_owner,s.current,g_ram[0x1f24],g_ram[0x1f10],g_ram[0x1ed2],MmxWeaponsGetState().menu_page);
  check(s.menu_owner==2 && s.current==1 && MmxWeaponsMenuVisible(g_ram),"P2 Start opens P2 equipment menu");
  unsigned page=s.players[1].weapons.menu_page;
  coop_frames(1,SNES_PAD_R,0);coop_frames(3,0,0);
  check(MmxWeaponsGetState().menu_page==page,"P1 cannot change P2's pause inventory");
  coop_frames(1,0,SNES_PAD_R);coop_frames(3,0,0);
  check(MmxWeaponsGetState().menu_page==(page+1)%3,"P2 controls their own weapon pages");
  coop_image(character,"menu");
  size_t n=RtlSaveSnapshotToMemory(start,cap);
  coop_frames(1,0,SNES_PAD_DOWN);coop_frames(3,0,0);coop_frames(1,0,SNES_PAD_START);coop_frames(75,0,0);
  size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"P2 pause snapshot restores its owner");
  coop_frames(1,0,SNES_PAD_DOWN);coop_frames(3,0,0);coop_frames(1,0,SNES_PAD_START);coop_frames(75,0,0);
  size_t an=RtlSaveSnapshotToMemory(actual,cap);
  same(expected,en,actual,an,"P2 menu selection and exit replay exactly");
  s=MmxCoopGetState();
  check(!s.current && !s.menu_owner && !s.players[0].weapons.page &&
      s.players[1].weapons.page==1 && s.players[1].weapons.weapon==1,
      "P2 menu changes only P2's selection and returns control to both players");
  coop_frames(1,SNES_PAD_START,0);coop_frames(75,0,0);
  check(MmxCoopGetState().menu_owner==1 && !MmxWeaponsGetState().menu_page,"P1 opens their separate inventory");
  coop_frames(1,SNES_PAD_START,0);coop_frames(75,0,0);
}
static void coop_hud_checks(unsigned character) {
  coop_weapon_select(0,0,0);coop_weapon_select(1,0,0);
  MmxCoopSelect(g_ram,1);g_ram[0xbcf]=12|128;MmxCoopSelect(g_ram,0);
  g_ram[0xbcf]=3|128;g_ram[0x1f12]=0;
  for(unsigned weapon=1;weapon<=8;++weapon) g_ram[0x1f86+weapon*2]=0xc0|9;
  uint32_t pixels[256*224],native[256*224];
  for(unsigned weapon=1;weapon<=8;++weapon) {
    coop_frames(1,SNES_PAD_R,0);coop_frames(3,0,0);
    check(g_ram[0xbdb]==weapon*2,"native weapon selection advances for source HUD comparison");
    MmxCoopState s=MmxCoopGetState();
    check(MmxRendererDraw(pixels,(MmxRenderView){256,0,4.0/3.0},false),"co-op HUD renders");
    MmxRendererCoopFrame(NULL);
    check(MmxRendererDraw(native,(MmxRenderView){256,0,4.0/3.0},false),"original native HUD renders");
    MmxRendererCoopFrame(&s);
    unsigned changed=0;
    for(unsigned y=0;y<96;++y) for(unsigned x=8;x<40;++x) changed+=pixels[y*256+x]!=native[y*256+x];
    if(changed) {fprintf(stderr,"co-op HUD source difference weapon=%u pixels=%u\n",weapon,changed);coop_image(character,"hud-difference");}
    check(!changed,"P1 health and native weapon HUD match original pixels at partial values");
  }
  coop_frames(1,0,SNES_PAD_R);coop_frames(3,0,0);coop_image(character,"hud-native");
  coop_frames(1,SNES_PAD_R,0);coop_frames(3,0,0);coop_image(character,"hud-gap");
  check(!g_ram[0xbdb],"P1 buster leaves reserved weapon column empty");
}
static void coop_spawn_pickup(unsigned seat,unsigned kind) {
  MmxCoopState s=MmxCoopGetState();
  memset(g_ram+0x1628,0,48);g_ram[0x1628]=1;
  g_ram[0x1632]=(uint8_t)kind;g_ram[0x1633]=129;
  unsigned x=coop_x(&s.players[seat]),y=coop_y(&s.players[seat])-16;
  g_ram[0x162d]=(uint8_t)x;g_ram[0x162e]=(uint8_t)(x>>8);
  g_ram[0x1630]=(uint8_t)y;g_ram[0x1631]=(uint8_t)(y>>8);
}
static void coop_pickup_checks(uint8_t *start,uint8_t *expected,uint8_t *actual,size_t cap) {
  MmxCoopSelect(g_ram,1);g_ram[0xbcf]=7|128;MmxCoopSelect(g_ram,0);
  g_ram[0xbcf]=5|128;MmxCoopCapture(g_ram);
  for(unsigned seat=0;seat<2;++seat) {
    coop_spawn_pickup(seat,2);coop_frames(55,0,0);
    MmxCoopState s=MmxCoopGetState();
    fprintf(stderr,"pickup seat=%u HP=%u/%u item=%u/%u/%u owner=%u current=%u\n",seat,
        s.players[0].body[0x27]&127,s.players[1].body[0x27]&127,g_ram[0x1628],g_ram[0x162a],g_ram[0x162b],s.pickup_owner[0],s.current);
    check(!g_ram[0x1628] && (s.players[0].body[0x27]&127)==7 &&
        (s.players[1].body[0x27]&127)==7+seat*2,"native health pickup heals only its collector");
  }
  coop_weapon_select(0,2,1);coop_weapon_select(1,1,7);
  for(unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);MmxWeaponsState w=MmxWeaponsGetState();
    w.energy[(w.page-1)*8+w.weapon-1]=10;MmxWeaponsSetState(w);
  }
  MmxCoopSelect(g_ram,0);coop_spawn_pickup(1,1);coop_frames(5,0,0);
  size_t n=RtlSaveSnapshotToMemory(start,cap);coop_frames(50,0,0);
  size_t en=RtlSaveSnapshotToMemory(expected,cap);
  check(RtlLoadSnapshotFromMemory(start,n),"mid-refill snapshot restores collector ownership");coop_frames(50,0,0);
  size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"collector refill replays exactly");
  MmxCoopState s=MmxCoopGetState();
  check(!g_ram[0x1628] && s.players[0].weapons.energy[8]==10 && s.players[1].weapons.energy[6]==12,
      "native weapon pickup refills only its collector's selected imported weapon");
  g_ram[0x1f83]=0x80;
  MmxCoopSelect(g_ram,1);g_ram[0xbcf]=g_ram[0x1f9a]|128;MmxCoopSelect(g_ram,0);
  coop_spawn_pickup(1,2);coop_frames(55,0,0);s=MmxCoopGetState();
  check((g_ram[0x1f83]&15)==1 && (s.players[0].body[0x27]&127)==7,
      "full-health pickup fills shared subtanks without healing the other player");
  g_ram[0x1f83]=0x83;MmxCoopSelect(g_ram,1);g_ram[0xbcf]=9|128;
  MmxCoopSelect(g_ram,0);
  coop_frames(1,0,SNES_PAD_START);coop_frames(75,0,0);
  g_ram[0x1ed2]=g_ram[0x1ed3]=10; /* Select the shared unlocked first tank. */
  coop_frames(1,0,SNES_PAD_Y);coop_frames(30,0,0);
  s=MmxCoopGetState();
  check(!(g_ram[0x1f83]&15) && (s.players[0].body[0x27]&127)==7 && (s.players[1].body[0x27]&127)>9,
      "P2 menu consumes the shared subtank and heals only P2");
  coop_frames(1,0,SNES_PAD_START);coop_frames(75,0,0);
  check(!(g_ram[0x1f83]&15),"returning to P1 cannot restore spent shared subtank reserves");
}
static void coop_death_checks(unsigned character,const char *fixture,uint8_t *start,
                              uint8_t *expected,uint8_t *actual,size_t cap) {
  for(unsigned fallen=0;fallen<2;++fallen) {
    MmxCoopDisable();check(RtlLoadSnapshot(fixture),"restore survivor fixture");
    check(MmxCoopEnable(character),"survivor check uses chosen roster");coop_frames(8,0,0);
    MmxCoopState s=MmxCoopGetState();
    check(coop_place_fixture_partner(coop_x(&s.players[0])+40,coop_y(&s.players[0])),"place death check partner");
    unsigned survivor=fallen^1,lives=g_ram[0x1f80];
    s=MmxCoopGetState();unsigned before=coop_x(&s.players[survivor]);
    MmxCoopSelect(g_ram,fallen);g_ram[0xbcf]=128;g_ram[0xbaa]=12;g_ram[0xbab]=0;
    MmxCoopSelect(g_ram,0);
    for(unsigned i=0;i<65;++i) coop_frames(1,survivor?0:SNES_PAD_RIGHT,survivor?SNES_PAD_RIGHT:0);
    s=MmxCoopGetState();
    fprintf(stderr,"death fallen=%u status=%u/%u anchor=%u current=%u lives=%u/%u flags=%u/%u/%u HP=%u/%u\n",
        fallen,s.players[0].status,s.players[1].status,s.anchor,s.current,lives,g_ram[0x1f80],g_ram[0x1f13],
        g_ram[0x1f19],g_ram[0x1f0b],s.players[0].body[0x27]&127,s.players[1].body[0x27]&127);
    check(s.players[fallen].status==MMX_COOP_FALLEN && s.players[survivor].status==MMX_COOP_ALIVE &&
        s.anchor==survivor && s.current==survivor && g_ram[0x1f80]==lives,
        "one native death leaves the survivor in control without spending a life");
    check(coop_x(&s.players[survivor])>before+60,"survivor can move during and after partner death");
    coop_frames(1,0,SNES_PAD_SELECT);coop_frames(2,0,0);
    check(MmxCoopGetState().players[fallen].status==MMX_COOP_FALLEN,"Select cannot revive a fallen partner");
    coop_frames(1,survivor?0:SNES_PAD_Y,survivor?SNES_PAD_Y:0);coop_frames(3,0,0);
    s=MmxCoopGetState();bool shot=false;
    for(unsigned i=0;i<8;++i) shot|=s.players[survivor].shots[i*64]!=0;
    if(!shot) fprintf(stderr,"survivor buster missing action=%u/%u nativeweapon=%u page=%u/%u input=%04x edge=%02x%02x held=%02x%02x command=%u flags=%u/%u/%u\n",
        g_ram[0xbaa],g_ram[0xbab],g_ram[0xbdb],s.players[survivor].weapons.page,s.players[survivor].weapons.weapon,
        s.players[survivor].input,g_ram[0xbe3],g_ram[0xbe2],g_ram[0xbdf],g_ram[0xbde],g_ram[0x1f0d],g_ram[0x1f0c],g_ram[0x1f1a],g_ram[0x1f19]);
    check(shot,"surviving controller fires its own buster");
    size_t n=RtlSaveSnapshotToMemory(start,cap);
    coop_frames(10,survivor?0:SNES_PAD_RIGHT,survivor?SNES_PAD_RIGHT:0);
    size_t en=RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n),"survivor snapshot restores world ownership");
    coop_frames(10,survivor?0:SNES_PAD_RIGHT,survivor?SNES_PAD_RIGHT:0);
    size_t an=RtlSaveSnapshotToMemory(actual,cap);same(expected,en,actual,an,"survivor-only world replay matches");
    coop_image(character,survivor?"p2-survives":"p1-survives");
    coop_frames(1,survivor?0:SNES_PAD_START,survivor?SNES_PAD_START:0);
    s=MmxCoopGetState();fprintf(stderr,"survivor pause first owner=%u current=%u trigger=%02x flags=%u/%u stage=%u/%u/%u HP=%u\n",
        s.menu_owner,s.current,g_ram[0xbe3],g_ram[0x1f3b],g_ram[0x1f10],g_ram[0xd1],g_ram[0xd2],g_ram[0xd3],g_ram[0xbcf]);
    coop_frames(75,0,0);
    s=MmxCoopGetState();fprintf(stderr,"survivor pause last owner=%u current=%u trigger=%02x flags=%u/%u stage=%u/%u/%u\n",
        s.menu_owner,s.current,g_ram[0xbe3],g_ram[0x1f3b],g_ram[0x1f10],g_ram[0xd1],g_ram[0xd2],g_ram[0xd3]);
    check(MmxCoopGetState().menu_owner==survivor+1,"survivor opens their own pause menu");
    coop_frames(1,survivor?0:SNES_PAD_START,survivor?SNES_PAD_START:0);coop_frames(75,0,0);
    g_ram[0xbcf]=128;g_ram[0xbaa]=12;g_ram[0xbab]=0;
    bool returned=false;
    for(unsigned i=0;i<1200;++i) {
      coop_frames(1,0,0);s=MmxCoopGetState();
      if(g_ram[0x1f80]+1==lives && !s.stage_pending && !MmxCoopTransitionActive() &&
          s.players[0].status==MMX_COOP_ALIVE && s.players[1].status==MMX_COOP_ALIVE &&
          (s.players[0].body[0x27]&127)==g_ram[0x1f9a] && (s.players[1].body[0x27]&127)==g_ram[0x1f9a]) {
        returned=true;break;
      }
    }
    if(!returned) fprintf(stderr,"team return failed stage=%u/%u/%u anchor=%u current=%u pending=%u status=%u/%u lives=%u/%u\n",
        g_ram[0xd1],g_ram[0xd2],g_ram[0xd3],s.anchor,s.current,s.stage_pending,s.players[0].status,s.players[1].status,lives,g_ram[0x1f80]);
    if(!returned) {uint16_t lx=0,ly=0;bool land=MmxCoopFindLanding(g_ram,&lx,&ly);
      fprintf(stderr,"return body=%u/%u/%u HP=%u xy=%u/%u camera=%u/%u freezes=%u/%u/%u/%u land=%u %u/%u enrolled=%u\n",
          g_ram[0xba9],g_ram[0xbaa],g_ram[0xbab],g_ram[0xbcf],coop_x(&s.players[0]),coop_y(&s.players[0]),
          g_ram[0x1e4d]+256*g_ram[0x1e4e],g_ram[0x1e50]+256*g_ram[0x1e51],g_ram[0x1f0c],g_ram[0x1f10],g_ram[0x1f23],g_ram[0x1f48],land,lx,ly,s.enrolled);}
    check(returned,"either survivor's death loses one life and returns both enrolled players");
  }
  MmxCoopDisable();check(RtlLoadSnapshot(fixture),"restore simultaneous death fixture");
  check(MmxCoopEnable(character),"simultaneous death uses chosen roster");coop_frames(8,0,0);
  MmxCoopState s=MmxCoopGetState();
  check(coop_place_fixture_partner(coop_x(&s.players[0])+40,coop_y(&s.players[0])),"place simultaneous death partner");
  unsigned lives=g_ram[0x1f80];
  for(unsigned seat=0;seat<2;++seat) {
    MmxCoopSelect(g_ram,seat);g_ram[0xbcf]=128;g_ram[0xbaa]=12;g_ram[0xbab]=0;
  }
  MmxCoopSelect(g_ram,0);bool returned=false;
  for(unsigned i=0;i<1200;++i) {
    coop_frames(1,0,0);s=MmxCoopGetState();
    if(g_ram[0x1f80]+1==lives && !s.stage_pending && !MmxCoopTransitionActive() &&
        s.players[0].status==MMX_COOP_ALIVE && s.players[1].status==MMX_COOP_ALIVE &&
        (s.players[0].body[0x27]&127)==g_ram[0x1f9a] && (s.players[1].body[0x27]&127)==g_ram[0x1f9a]) {
      returned=true;break;
    }
  }
  check(returned,"simultaneous fatalities spend one life and restart both players");

}
/* Optional private fixture just before Chill Penguin's two right-facing doors.
 * Also exercises the native enemy-projectile loop after the boss introduction. */
static void coop_scene_checks(const char *fixture,uint8_t *start,uint8_t *expected,
                              uint8_t *actual,size_t cap) {
  for(unsigned driver=0;driver<2;++driver) {
    MmxCoopDisable();check(RtlLoadSnapshot(fixture),"door approach fixture loads");
    check(MmxCoopEnable(0),"door roster enables");coop_frames(8,0,0);
    MmxCoopState s=MmxCoopGetState();
    check(coop_place_fixture_partner(coop_x(&s.players[0])+(driver?40:-40),coop_y(&s.players[0])),
        "door partner placed on approach floor");
    unsigned departures=0,returns=0,last_phase=0;bool replayed=false;
    for(unsigned f=0;f<1200;++f) {
      coop_frames(1,driver?0:SNES_PAD_RIGHT,driver?SNES_PAD_RIGHT:0);s=MmxCoopGetState();
      if(s.scene_phase==1 && !last_phase) {
        ++departures;
        check(s.scene_owner==driver+1 && s.anchor==driver,"touching player owns native door walk");
      }
      if(s.scene_phase==2 && last_phase==1 && departures==2) coop_image(driver,"door-hidden");
      if(s.scene_phase==3 && last_phase==2) {
        check(s.players[driver^1].status==MMX_COOP_ALIVE &&
            (s.players[driver^1].body[0x27]&127),"living partner returns on clear floor");
        if(!replayed) {
          size_t n=RtlSaveSnapshotToMemory(start,cap);check(n!=0,"door arrival saves");
          coop_frames(12,0,0);size_t en=RtlSaveSnapshotToMemory(expected,cap);
          check(RtlLoadSnapshotFromMemory(start,n),"door arrival restores");
          coop_frames(12,0,0);size_t an=RtlSaveSnapshotToMemory(actual,cap);
          same(expected,en,actual,an,"door arrival replays both actors and world exactly");
          s=MmxCoopGetState();replayed=true;
        }
      }
      if(!s.scene_phase && last_phase==3) {
        ++returns;
        if(returns==2) coop_image(driver,"door-return");
      }
      last_phase=s.scene_phase;
    }
    check(departures==2 && returns==2 && replayed,"either seat crosses both doors and rejoins before combat");
    check(!s.scene_owner && !s.door_pass && s.current==s.anchor && g_ram[0xd3]==4,
        "boss combat resumes with balanced native task context");
    coop_image(driver,"boss-combat");
    /* A script ending cannot turn the partner's fallen state into a join. */
    s.players[driver^1].status=MMX_COOP_FALLEN;
    s.scene_owner=(uint8_t)(driver+1);s.scene_phase=2;MmxCoopSetState(&s);
    coop_frames(1,0,0);s=MmxCoopGetState();
    check(s.players[driver^1].status==MMX_COOP_FALLEN && !s.scene_owner,
        "scene return never revives a fallen partner");
  }
  MmxCoopDisable();puts("MMX CO-OP DOOR CHECKS PASSED");
}
static void coop_capsule_checks(const char *fixture) {
  MmxCoopDisable();check(RtlLoadSnapshot(fixture),"capsule approach fixture loads");
  check(MmxCoopEnable(0),"capsule roster enables");coop_frames(8,0,0);
  MmxCoopState s=MmxCoopGetState();
  check(coop_place_fixture_partner(coop_x(&s.players[0])-32,coop_y(&s.players[0])),"capsule partner placed");
  bool hidden=false,returned=false;
  for(unsigned f=0;f<1100;++f) {
    unsigned input=g_ram[0x1f31] ? (f%60==0?SNES_PAD_B:0) : SNES_PAD_RIGHT;
    coop_frames(1,input,0);s=MmxCoopGetState();
    if(s.scene_phase==2) hidden=true;
    if(hidden && !s.scene_phase && !g_ram[0xc16]) {returned=true;break;}
  }
  check(hidden && returned && s.players[1].status==MMX_COOP_ALIVE,
      "P1 capsule dialogue transports the partner out and back");
  coop_image(0,"capsule-return");
  MmxCoopDisable();puts("MMX CO-OP CAPSULE DIALOGUE CHECKS PASSED");
}
static void coop_keyboard_checks(const char *fixture) {
  uint8_t keys[512]={0};keybinds_init(NULL);
  for(int p=1;p<=2;++p) {
    keybinds_set_button(p,11,SDL_SCANCODE_RIGHT);
    keybinds_set_button(p,7,SDL_SCANCODE_RSHIFT);
  }
  g_gamepad[0].joystick_id=1;g_gamepad[1].joystick_id=-1;
  g_config.player_src[0]=2;g_config.player_src[1]=1;
  keys[SDL_SCANCODE_RIGHT]=1;PollKeyboardControls(keys);g_pad_buttons=SNES_PAD_LEFT;
  check(g_input_state==(SNES_PAD_RIGHT<<12) && OverlayNavInputs()==(SNES_PAD_RIGHT<<12|SNES_PAD_LEFT),
      "keyboard P2 and gamepad P1 stay independent through desktop input routing");
  check(GetActiveControllers()==(3u<<30),"P2 keyboard marks the second controller present");
  g_config.player_src[0]=1;g_config.player_src[1]=2;PollKeyboardControls(keys);
  check(g_input_state==SNES_PAD_RIGHT,"changing keyboard assignment clears the previous seat's held input");
  g_config.player_src[0]=g_config.player_src[1]=0;PollKeyboardControls(keys);
  check(!g_input_state,"unassigned seats ignore keyboard bindings");
  g_config.player_src[0]=g_config.player_src[1]=1;PollKeyboardControls(keys);
  check(g_input_state==(SNES_PAD_RIGHT<<12|SNES_PAD_RIGHT),"explicitly assigning both seats to keyboard preserves shared bindings");
  g_config.player_src[0]=2;g_config.player_src[1]=1;g_pad_buttons=0;memset(keys,0,sizeof(keys));
  MmxCoopDisable();check(RtlLoadSnapshot(fixture),"keyboard join fixture loads");
  check(MmxCoopEnable(0),"keyboard co-op enables");coop_frames(8,0,0);
  check(MmxCoopTransitionActive(),"P2 joins automatically with no keyboard press");
  for(unsigned i=0;i<90;++i) {PollKeyboardControls(keys);frame(g_input_state);}
  MmxCoopState s=MmxCoopGetState();unsigned x=coop_x(&s.players[0]);
  check(s.players[1].status==MMX_COOP_ALIVE && !MmxCoopTransitionActive(),"keyboard join finishes");
  keys[SDL_SCANCODE_RSHIFT]=1;
  for(unsigned i=0;i<89;++i) {PollKeyboardControls(keys);frame(g_input_state);}
  check(!MmxCoopTransitionActive(),"keyboard hold shorter than 1.5 seconds does not withdraw");
  PollKeyboardControls(keys);frame(g_input_state);
  check(MmxCoopTransitionActive(),"holding Right Shift for 90 frames starts P2 departure");
  keys[SDL_SCANCODE_RSHIFT]=0;
  for(unsigned i=0;i<90;++i) {PollKeyboardControls(keys);frame(g_input_state);}
  s=MmxCoopGetState();
  check(s.players[1].status==MMX_COOP_ABSENT && coop_x(&s.players[0])==x,
      "keyboard withdrawal finishes without controlling P1");
  keys[SDL_SCANCODE_RSHIFT]=1;PollKeyboardControls(keys);frame(g_input_state);
  check(MmxCoopTransitionActive(),"P2 Right Shift rejoins through the real keyboard poll");
  MmxCoopDisable();puts("MMX CO-OP KEYBOARD ROUTING CHECKS PASSED");
}
static unsigned coop_death_sounds,coop_death_orbs;
static void coop_death_sound_check(CpuState *cpu,uint32_t pc) {
  if(cpu->D!=0xba8) return;
  if((pc&65535)==0x8aa4) ++coop_death_sounds; /* Original $0A sound. */
  else for(unsigned d=0x1928;d<0x1d08;d+=32)
    coop_death_orbs+=g_ram[d]!=0 && g_ram[d+10]==14;
}
static void coop_natural_death_checks(const char *fixture) {
  interp_bridge_set_pre_opcode_hook(0x818aa4,coop_death_sound_check);
  interp_bridge_set_pre_opcode_hook(0x818b09,coop_death_sound_check);
  for(unsigned character=0;character<2;++character)
  for(unsigned pit=0;pit<2;++pit) for(unsigned fallen=0;fallen<2;++fallen) {
    MmxCoopDisable();check(RtlLoadSnapshot(fixture),"natural death fixture loads");
    check(MmxCoopEnable(character),"natural death uses either roster");coop_frames(8,0,0);
    MmxCoopState s=MmxCoopGetState();
    check(coop_place_fixture_partner(coop_x(&s.players[0])+40,coop_y(&s.players[0])),"natural death partner placed");
    unsigned enemy=0;
    if(!pit) {
      for(unsigned i=0;i<400 && !enemy;++i) {
        coop_frames(1,SNES_PAD_RIGHT,SNES_PAD_RIGHT);
        for(unsigned d=0xe68;d<0x1228;d+=64)
          if(g_ram[d] && g_ram[d+14] && (g_ram[d+0x27]&127) && (g_ram[d+0x20]|g_ram[d+0x21])) {enemy=d;break;}
      }
      check(enemy!=0,"native enemy found for lethal contact");
    }
    MmxCoopSelect(g_ram,fallen);
    if(pit) {
      /* Start below the floor with HP intact; native gravity/camera must
       * detect the fall. Do not preselect the death controller. */
      unsigned y=(g_ram[0x1e50]|g_ram[0x1e51]<<8)+240;
      g_ram[0xbb0]=(uint8_t)y;g_ram[0xbb1]=(uint8_t)(y>>8);
      g_ram[0xbaa]=8;g_ram[0xbab]=2;
    } else {
      memcpy(g_ram+0xbad,g_ram+enemy+5,2);memcpy(g_ram+0xbb0,g_ram+enemy+8,2);
      g_ram[0xc30]=g_ram[0xc32]=0;g_ram[0xbcf]=129;
    }
    MmxCoopSelect(g_ram,0);
    unsigned lives=g_ram[0x1f80],tick=g_ram[0xb9c];
    coop_death_sounds=coop_death_orbs=0;
    for(unsigned f=0;f<90;++f) {
      coop_frames(1,0,0);s=MmxCoopGetState();
      if(character==0 && !pit && fallen==1 && f==35) coop_image(character,"p2-death-orbs");
    }
    check(s.players[fallen].status==MMX_COOP_FALLEN && !(s.players[fallen].body[0x27]&127),
        "natural fatality empties its health and completes native death");
    check(coop_death_sounds==1 && coop_death_orbs==8,"first death plays one native sound and spawns all eight native orbs");
    check(s.players[fallen^1].status==MMX_COOP_ALIVE && g_ram[0x1f80]==lives &&
        (uint8_t)(g_ram[0xb9c]-tick)==90,"survivor world keeps running without losing a life");
    if(character==0 && !pit && fallen==1) coop_image(character,"p2-natural-death");
  }
  interp_bridge_set_pre_opcode_hook(0x818aa4,NULL);
  interp_bridge_set_pre_opcode_hook(0x818b09,NULL);
  puts("MMX CO-OP NATURAL DEATH CHECKS PASSED");
}
static void coop_launcher_checks(const char *root,const char *rom,const char *x3) {
  check(x3!=NULL,"launcher check receives original X3 ROM");
  check(snes_mod_runtime_initialize_c(root,"megaman-x-us",
      "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32"),
      "private co-op catalog parses");
  const RecompLauncherCModProvider *p=snes_mod_runtime_launcher_provider_c();
  check(p && p->feature_enable && p->feature_resource_get,"real launcher feature provider exists");
  check(p->feature_enable(p->ctx,"megaman-x.coop","coop",1),"launcher enables co-op");
  check(p->feature_resource_set_path(p->ctx,"megaman-x.coop","coop","x3-rom",x3),
      "launcher accepts original X3 ROM");
  const char *packages[]={"megaman-x.coop","megaman-x.character.zero","megaman-x.weapons.x3"};
  const char *features[]={"coop","zero","weapons"};
  for(unsigned i=0;i<3;++i) {
    RecompLauncherCModResource r={0};
    check(p->feature_resource_get(p->ctx,packages[i],features[i],0,&r) && !strcmp(r.path,x3),
        "co-op, exchange and X3 weapons share one source ROM selection");
  }
  check(p->feature_enable(p->ctx,"megaman-x.character.zero","zero",1),"stage conflicting character mode");
  check(!p->commit(p->ctx,rom) && p->diagnostic_count(p->ctx,"megaman-x.coop","coop")>0,
      "launcher rejects simultaneous co-op and exchange");
  check(p->feature_enable(p->ctx,"megaman-x.character.zero","zero",0),"disable exchange for co-op");
  for(unsigned character=0;character<2;++character) {
    check(p->feature_set_option(p->ctx,"megaman-x.coop","coop","player1",character?"zero":"x"),
        "launcher selects either P1 character");
    check(p->commit(p->ctx,rom),"co-op source validation and plan commit");
    snes_mod_runtime_activate_plugins_c();
    MmxCoopState s=MmxCoopGetState();
    check(MmxCoopEnabled() && MmxZeroEnabled() && s.players[0].character==character &&
        s.players[1].character==(character^1),"real plugin extracts source ROM and activates chosen roster");
  }
  check(p->feature_enable(p->ctx,"megaman-x.coop","coop",0) && p->commit(p->ctx,rom),
      "launcher disables co-op");
  snes_mod_runtime_activate_plugins_c();
  check(!MmxCoopEnabled() && !MmxZeroEnabled(),"plugin reset restores stock mode");
  puts("MMX CO-OP LAUNCHER CHECKS PASSED");
}
static void coop_checks(const char *assets, const char *fixture,
                        uint8_t *start, uint8_t *expected, uint8_t *actual, size_t cap) {
  check(assets && fixture, "co-op check requires private source assets and a grounded fixture");
  check(MmxZeroLoad(assets), "co-op uses original X3 Zero assets");
  MmxZeroRegisterHooks(); MmxCoopRegisterHooks();
  const char *x2=getenv("MMX_COOP_X2_ASSETS"),*x3=getenv("MMX_COOP_X3_ASSETS");
  if (x2 && x3) check(MmxWeaponsLoadPage(x2,1) && MmxWeaponsLoadPage(x3,2),"private original weapon source packs load");
  if(getenv("MMX_COOP_NATURAL_DEATH")) {coop_natural_death_checks(fixture);return;}
  if(getenv("MMX_COOP_KEYBOARD_TEST")) {coop_keyboard_checks(fixture);return;}
  if(getenv("MMX_COOP_CAPSULE_FIXTURE")) {coop_capsule_checks(getenv("MMX_COOP_CAPSULE_FIXTURE"));return;}
  const char *scene_fixture=getenv("MMX_COOP_SCENE_FIXTURE");
  if(scene_fixture) {coop_scene_checks(scene_fixture,start,expected,actual,cap);return;}
  for (unsigned character = 0; character < 2; ++character) {
    MmxCoopDisable();
    check(RtlLoadSnapshot(fixture), "private co-op fixture loads");
    check(MmxCoopEnable(character), "either starting character is accepted");
    coop_frames(8,0,0);
    MmxCoopState s = MmxCoopGetState();
    check(s.initialized && s.players[0].character == character && s.players[1].character == (character ^ 1),
          "P1 choice assigns the counterpart to P2");
    check(s.players[0].zero.active_x == (character == MMX_COOP_X), "native context uses chosen P1");
    unsigned x0 = coop_x(&s.players[0]), y0 = coop_y(&s.players[0]);
    check(coop_place_fixture_partner(x0+32, y0), "test places the second actor on known clear ground");
    unsigned world_tick = g_ram[0xb9c];
    coop_frames(20,0,SNES_PAD_RIGHT);
    s = MmxCoopGetState();
    fprintf(stderr,"co-op roster=%u initial=%u p1=%u p2=%u pass=%u input=%04x native=%02x/%02x\n",
      character,x0,coop_x(&s.players[0]),coop_x(&s.players[1]),s.controller_pass,s.players[1].input,
      s.players[1].body[0x36],s.players[1].body[0x37]);
    check(coop_x(&s.players[0]) == x0 && coop_x(&s.players[1]) > x0+50,
          "P2 moves using port 2 while P1 stays still");
    check((uint8_t)(g_ram[0xb9c]-world_tick) == 20, "world frame counter advances once per two-player frame");
    check(s.current == 0 && !s.controller_pass, "second controller restores native P1 context and stack path");
    coop_image(character,"walk");
    unsigned p2x = coop_x(&s.players[1]);
    coop_frames(8,SNES_PAD_LEFT,SNES_PAD_B);
    s = MmxCoopGetState();
    check(coop_x(&s.players[0]) < x0 && coop_x(&s.players[1]) == p2x && coop_y(&s.players[1]) < y0,
          "P1 walks independently while P2 jumps with original character physics");
    check(MmxCoopValidState(&s), "both native player contexts validate");
    coop_image(character,"jump");
    coop_frames(1,SNES_PAD_Y,SNES_PAD_Y);
    s = MmxCoopGetState();
    for (unsigned seat=0;seat<2;++seat) {
      const MmxCoopPlayer *p=&s.players[seat];
      fprintf(stderr,"seat %u shoot input=%04x buttons=%02x edge=%02x slots=",seat,p->input,p->body[0x37],p->body[0x3b]);
      for(unsigned i=0;i<8;++i) fprintf(stderr," %02x/%02x",p->shots[i*64],p->shots[i*64+10]);
      fprintf(stderr," counts=%u/%u weapon=%u fire=%u\n",p->body[0x35],p->body[0x7d],p->body[0x33],p->body[0x50]);
    }
    check(s.players[0].shots[0] && s.players[1].shots[0], "both players create independent buster projectiles");
    unsigned shot_x[2] = {s.players[0].shots[5]|s.players[0].shots[6]<<8,
        s.players[1].shots[5]|s.players[1].shots[6]<<8};
    coop_frames(3,0,0);
    s = MmxCoopGetState();
    check((s.players[0].shots[5]|s.players[0].shots[6]<<8)!=shot_x[0] &&
          (s.players[1].shots[5]|s.players[1].shots[6]<<8)!=shot_x[1],
          "both native projectile pools advance");
    coop_image(character,"shots");
    size_t n = RtlSaveSnapshotToMemory(start,cap);
    check(n != 0, "both players save in the native snapshot");
    coop_frames(12,SNES_PAD_RIGHT,SNES_PAD_LEFT);
    size_t en = RtlSaveSnapshotToMemory(expected,cap);
    check(RtlLoadSnapshotFromMemory(start,n), "co-op snapshot restores");
    coop_frames(12,SNES_PAD_RIGHT,SNES_PAD_LEFT);
    size_t an = RtlSaveSnapshotToMemory(actual,cap);
    same(expected,en,actual,an,"two-port replay preserves both actors and world state exactly");
    s = MmxCoopGetState(); s.players[1].character = s.players[0].character;
    check(!MmxCoopValidState(&s), "malformed duplicate-character roster is rejected");
    coop_contact_checks(start,cap);
    coop_join_checks(character,fixture,start,expected,actual,cap);
    coop_camera_checks(character);
    if (x2 && x3) coop_menu_checks(character,start,expected,actual,cap);
    if (x2 && x3) {
      MmxCoopDisable();check(RtlLoadSnapshot(fixture),"restore independent weapon fixture");
      check(MmxCoopEnable(character),"restore chosen roster for weapons");coop_frames(8,0,0);
      s=MmxCoopGetState();check(coop_place_fixture_partner(coop_x(&s.players[0])+40,coop_y(&s.players[0])),"place weapon test partner");
      coop_pickup_checks(start,expected,actual,cap);
      coop_hud_checks(character);
      coop_weapon_checks(character);
    }
    coop_death_checks(character,fixture,start,expected,actual,cap);
  }
  MmxCoopDisable();
  puts("MMX CO-OP CONTROLLER FOUNDATION CHECKS PASSED");
}

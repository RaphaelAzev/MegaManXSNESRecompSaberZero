#include "mmx_coop_view.h"

static bool online;
static MmxCoopViewWorldState world;
MmxCoopViewWorldState MmxCoopViewsGetWorldState(void) { return world; }
void MmxCoopViewsSetWorldState(const MmxCoopViewWorldState *s) { world=*s; }
void MmxCoopViewsResetWorld(void) { memset(&world,0,sizeof(world)); }
void MmxCoopViewsActorReturn(unsigned player) { world.actor_return=(uint8_t)player; }
void MmxCoopViewsContactPlayer(unsigned player) { world.contact_player=(uint8_t)player; }
void MmxCoopViewsBeginStage(unsigned stage) {
  if(!world.initialized || world.stage!=stage) {
    MmxCoopViewsResetWorld();world.stage=(uint8_t)stage;world.initialized=1;
  }
}
bool MmxCoopViewsSeen(unsigned flag) {
  return flag>=0xf800 && flag<=0xffff && (world.seen[(flag-0xf800)>>3]&(1u<<(flag&7)));
}
void MmxCoopViewsMark(unsigned flag,bool seen) {
  if(flag<0xf800 || flag>0xffff) return;
  unsigned n=(flag-0xf800)>>3,bit=1u<<(flag&7);
  if(seen) world.seen[n]|=bit;else world.seen[n]&=~bit;
}
void MmxCoopViewsSetOnline(bool active) { online=active; }
bool MmxCoopViewsOnline(void) { return online; }
static int word(const uint8_t *r) { return r[0]|r[1]<<8; }
static int clamp(int v,int lo,int hi) { if(hi<lo) hi=lo;return v<lo?lo:v>hi?hi:v; }
static bool living(const MmxCoopPlayer *p) {
  return p->status==MMX_COOP_ALIVE && (p->body[0x27]&127) && p->body[2]!=12;
}
MmxCoopView MmxCoopViewForPlayer(const uint8_t *r,const MmxCoopState *s,unsigned seat) {
  MmxCoopView v={word(r+0x1e4d),word(r+0x1e50),s->anchor};
  if(!online || !s->initialized || seat>1) return v;
  /* A transported partner watches the native script from its first frame,
   * including the teleport beams. Fallen/withdrawn seats watch the survivor. */
  if(s->scene_owner || s->stage_pending || r[0xd3]!=4) return v;
  const MmxCoopPlayer *p=&s->players[seat];
  if(!living(p) || p->zero.swap_phase) seat=s->anchor;
  v.player=seat;
  if(seat==s->anchor) return v;
  const uint8_t *body=seat==s->current?r+0xba8:s->players[seat].body;
  /* Use the authored stage/room camera limits, not the other player's view.
   * X1's feet lie in the lower dead zone of the 224px native viewport. */
  v.x=clamp(word(body+5)-128,word(r+0x1e56),word(r+0x1e58));
  v.y=clamp(word(body+8)-160,word(r+0x1e5a),word(r+0x1e5c));
  return v;
}
bool MmxCoopViewContains(const uint8_t *r,const MmxCoopState *s,int x,int y,
                         int left,int right,int top,int bottom,int margin) {
  for(unsigned seat=0;seat<2;++seat) {
    if(!living(&s->players[seat]) || s->players[seat].zero.swap_phase) continue;
    if(s->scene_owner && seat!=s->anchor) continue;
    MmxCoopView v=MmxCoopViewForPlayer(r,s,seat);
    int dx=(int16_t)(x-v.x),dy=(int16_t)(y-v.y);
    if(dx>=-left-margin && dx<right+margin && dy>=-top && dy<bottom) return true;
  }
  return false;
}

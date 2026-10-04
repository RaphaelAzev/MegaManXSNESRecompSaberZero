#pragma once
#include "mmx_coop.h"

/* Session policy is identical on both peers. The local seat affects only
 * presentation; it must never select a simulation camera or active region. */
typedef struct MmxCoopView { int x, y; unsigned player; } MmxCoopView;
void MmxCoopViewsSetOnline(bool online);
bool MmxCoopViewsOnline(void);
typedef struct MmxCoopViewWorldState {
  uint8_t seen[256],stage,initialized;
  /* Balanced native world/contact continuations must survive rollback. */
  uint8_t actor_return,contact_player;
} MmxCoopViewWorldState;
MmxCoopViewWorldState MmxCoopViewsGetWorldState(void);
void MmxCoopViewsSetWorldState(const MmxCoopViewWorldState *state);
void MmxCoopViewsResetWorld(void);
bool MmxCoopViewsSeen(unsigned flag);
void MmxCoopViewsMark(unsigned flag,bool seen);
void MmxCoopViewsBeginStage(unsigned stage);
void MmxCoopViewsActorReturn(unsigned player_plus_one);
void MmxCoopViewsContactPlayer(unsigned player);
MmxCoopView MmxCoopViewForPlayer(const uint8_t *ram, const MmxCoopState *state, unsigned seat);
bool MmxCoopViewContains(const uint8_t *ram, const MmxCoopState *state,
                         int x, int y, int left, int right, int top, int bottom, int margin);

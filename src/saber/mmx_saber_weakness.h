#pragma once

#include "mmx_saber_input.h"

typedef enum MmxSaberWeaknessMove {
  MMX_SABER_WEAKNESS_NONE,
  MMX_SABER_WEAKNESS_STORM_UPPERCUT,
  MMX_SABER_WEAKNESS_TORNADO,
  MMX_SABER_WEAKNESS_ROLLING_SLASH,
  MMX_SABER_WEAKNESS_DOWNWARD_THRUST,
  MMX_SABER_WEAKNESS_ICE_CLONE,
  MMX_SABER_WEAKNESS_THUNDER_ORB,
  MMX_SABER_WEAKNESS_CUTTER,
  MMX_SABER_WEAKNESS_MOVE_COUNT
} MmxSaberWeaknessMove;

/* Pure mapping from a Saber attack kind to its theme weakness row. */
MmxSaberWeaknessMove MmxSaberWeaknessMoveForAttackKind(
    MmxSaberPadKind kind);

/* Returns original for every non-matching actor or invalid move. A match
 * changes only the low contact-class byte. */
unsigned MmxSaberWeaknessContactClass(MmxSaberWeaknessMove move,
                                      unsigned enemy_actor,
                                      unsigned original);

#include "mmx_saber_weakness.h"

typedef struct MmxSaberWeaknessRow {
  MmxSaberWeaknessMove move;
  uint8_t class_id;
  uint8_t boss_actor;
} MmxSaberWeaknessRow;

static const MmxSaberWeaknessRow kSaberWeaknessRows[] = {
  {MMX_SABER_WEAKNESS_STORM_UPPERCUT, 0x09, 0x0c},
  {MMX_SABER_WEAKNESS_TORNADO, 0x09, 0x0c},
  {MMX_SABER_WEAKNESS_ROLLING_SLASH, 0x08, 0x07},
  {MMX_SABER_WEAKNESS_DOWNWARD_THRUST, 0x0a, 0x02},
  {MMX_SABER_WEAKNESS_ICE_CLONE, 0x07, 0x31},
  {MMX_SABER_WEAKNESS_THUNDER_ORB, 0x0c, 0x14},
  {MMX_SABER_WEAKNESS_CUTTER, 0x0d, 0x0a},
};

MmxSaberWeaknessMove MmxSaberWeaknessMoveForAttackKind(
    MmxSaberPadKind kind) {
  switch (kind) {
    case SABER_KIND_UPPERCUT:
      return MMX_SABER_WEAKNESS_STORM_UPPERCUT;
    case SABER_KIND_ROLLING:
      return MMX_SABER_WEAKNESS_ROLLING_SLASH;
    case SABER_KIND_THRUST:
      return MMX_SABER_WEAKNESS_DOWNWARD_THRUST;
    default:
      return MMX_SABER_WEAKNESS_NONE;
  }
}

unsigned MmxSaberWeaknessContactClass(MmxSaberWeaknessMove move,
                                      unsigned enemy_actor,
                                      unsigned original) {
  for (unsigned i = 0; i < sizeof(kSaberWeaknessRows) /
      sizeof(kSaberWeaknessRows[0]); ++i) {
    const MmxSaberWeaknessRow *row = &kSaberWeaknessRows[i];
    if (row->move == move && row->boss_actor == enemy_actor)
      return (original & 0xff00) | row->class_id;
  }
  return original;
}

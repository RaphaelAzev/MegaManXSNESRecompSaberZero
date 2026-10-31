#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SABER_SPECIAL_UPPERCUT,
  SABER_SPECIAL_ROLLING,
  SABER_SPECIAL_THRUST,
  SABER_SPECIAL_ICE_CLONE,
  SABER_SPECIAL_THUNDER_SLAM,
  SABER_SPECIAL_CUTTER_SLASH,
  SABER_SPECIAL_COUNT
} MmxSaberSpecialMove;

unsigned MmxSaberDefeatedMavericks(const uint8_t *ram);
bool MmxSaberSpecialBossDefeated(const uint8_t *ram,
                                 MmxSaberSpecialMove move);

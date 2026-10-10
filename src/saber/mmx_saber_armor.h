#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  MMX_SABER_ARMOR_HEAD_BIT = 0x01,
  MMX_SABER_ARMOR_ARMS_BIT = 0x02,
  MMX_SABER_ARMOR_BODY_BIT = 0x04,
  MMX_SABER_ARMOR_LEGS_BIT = 0x08,
  MMX_SABER_ARMOR_MASK = 0x0f,
  MMX_SABER_ARMOR_RAM_OFFSET = 0x1f99
};

typedef struct MmxSaberArmorFlags {
  bool head;
  bool arms;
  bool body;
  bool legs;
  bool black;
} MmxSaberArmorFlags;

MmxSaberArmorFlags MmxSaberArmorDecode(uint8_t upgrades);
MmxSaberArmorFlags MmxSaberArmorCurrent(void);
void MmxSaberArmorApplyStageStart(uint8_t *ram);

#ifdef __cplusplus
}
#endif

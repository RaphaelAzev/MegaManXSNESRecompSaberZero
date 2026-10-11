#include "mmx_saber_armor.h"

#include <stdio.h>

static const uint8_t *test_ram;
static bool test_saber_enabled;
static bool test_zero_active;
static bool test_start_all_upgrades;

bool MmxSaberEnabled(void) {
  return test_saber_enabled;
}

bool MmxZeroActive(void) {
  return test_zero_active;
}

const uint8_t *MmxSaberFrameRam(void) {
  return test_ram;
}

bool MmxSaberTuningStartAllUpgrades(void) {
  return test_start_all_upgrades;
}

static int failures;

static void check(bool condition, const char *message) {
  if (condition) return;
  fprintf(stderr, "FAIL: %s\n", message);
  ++failures;
}

static void check_low_nibble(void) {
  for (unsigned value = 0; value < 16; ++value) {
    const MmxSaberArmorFlags flags = MmxSaberArmorDecode((uint8_t)value);
    check(flags.head == ((value & 0x01) != 0),
          "head bit decodes for every low-nibble value");
    check(flags.arms == ((value & 0x02) != 0),
          "arms bit decodes for every low-nibble value");
    check(flags.body == ((value & 0x04) != 0),
          "body bit decodes for every low-nibble value");
    check(flags.legs == ((value & 0x08) != 0),
          "legs bit decodes for every low-nibble value");
    check(flags.black == (value == 0x0f),
          "black requires all four low-nibble bits");
  }
  check(!MmxSaberArmorDecode(0xf7).black,
        "$F7 is not Black Zero because its low nibble is $07");
  check(MmxSaberArmorDecode(0x1f).black,
        "$1F is Black Zero because its low nibble is $0F");
  {
    const MmxSaberArmorFlags low = MmxSaberArmorDecode(0x07);
    const MmxSaberArmorFlags high = MmxSaberArmorDecode(0xf7);
    check(low.head == high.head && low.arms == high.arms &&
              low.body == high.body && low.legs == high.legs &&
              low.black == high.black,
          "high bits do not change decoded armor flags");
  }
}

static void check_live_accessor(void) {
  uint8_t ram[0x20000] = {0};
  MmxSaberArmorFlags flags;

  test_ram = ram;
  ram[MMX_SABER_ARMOR_RAM_OFFSET] = MMX_SABER_ARMOR_MASK;
  test_saber_enabled = false;
  test_zero_active = true;
  flags = MmxSaberArmorCurrent();
  check(!flags.head && !flags.arms && !flags.body && !flags.legs && !flags.black,
        "live armor accessor is empty when Saber is disabled");
  test_saber_enabled = true;
  test_zero_active = false;
  flags = MmxSaberArmorCurrent();
  check(!flags.head && !flags.arms && !flags.body && !flags.legs && !flags.black,
        "live armor accessor is empty when Zero is inactive");
  test_zero_active = true;
  flags = MmxSaberArmorCurrent();
  check(flags.head && flags.arms && flags.body && flags.legs && flags.black,
        "live armor accessor decodes active Saber Zero RAM");
}

static void check_damage_bonus(void) {
  check(MmxSaberArmorDamageBonus(false, false) == 0,
        "damage bonus is zero without arms against normal enemies");
  check(MmxSaberArmorDamageBonus(false, true) == 0,
        "damage bonus is zero without arms against bosses");
  check(MmxSaberArmorDamageBonus(true, false) == 2,
        "damage bonus is two with arms against normal enemies");
  check(MmxSaberArmorDamageBonus(true, true) == 1,
        "damage bonus is one with arms against bosses");
}

static void check_stage_write(void) {
  uint8_t ram[0x20000] = {0};

  test_saber_enabled = true;
  test_zero_active = true;
  test_start_all_upgrades = false;
  ram[MMX_SABER_ARMOR_RAM_OFFSET] = 0x80;
  MmxSaberArmorApplyStageStart(ram);
  check(ram[MMX_SABER_ARMOR_RAM_OFFSET] == 0x80,
        "stage write leaves native armor unchanged when option is off");
  test_start_all_upgrades = true;
  MmxSaberArmorApplyStageStart(ram);
  check(ram[MMX_SABER_ARMOR_RAM_OFFSET] == 0x8f,
        "stage write ORs all four armor bits and preserves high bits");
  test_zero_active = false;
  ram[MMX_SABER_ARMOR_RAM_OFFSET] = 0;
  MmxSaberArmorApplyStageStart(ram);
  check(ram[MMX_SABER_ARMOR_RAM_OFFSET] == 0,
        "stage write leaves native armor unchanged when Zero is inactive");
  test_saber_enabled = false;
  test_zero_active = true;
  ram[MMX_SABER_ARMOR_RAM_OFFSET] = 0;
  MmxSaberArmorApplyStageStart(ram);
  check(ram[MMX_SABER_ARMOR_RAM_OFFSET] == 0,
        "stage write leaves native armor unchanged when Saber is disabled");
}

int main(void) {
  check_low_nibble();
  check_live_accessor();
  check_damage_bonus();
  check_stage_write();
  if (failures != 0) return 1;
  puts("PASS: Saber armor decoder");
  return 0;
}


#include "mmx_zero.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t ram[0x20000], before[0x20000], rom[0x180000], clean[0x180000];
static void asset(const char *path) {
  FILE *f = fopen(path, "wb"); assert(f);
  const uint8_t header[] = {'M','M','X','Z','E','R','O','4',128,0,128,0,64,0,64,0,117,0,35,0};
  uint8_t page[16384] = {0};
  assert(fwrite(header, sizeof(header), 1, f) == 1);
  assert(fwrite(page, 256, 1, f) == 1);
  uint8_t bounds[40];
  for (unsigned i = 0; i < 40; i += 4) { bounds[i] = 0; bounds[i+1] = 248; bounds[i+2] = 12; bounds[i+3] = 10; }
  assert(fwrite(bounds, sizeof(bounds), 1, f) == 1);
  assert(fwrite(page, 160, 1, f) == 1);
  for (unsigned i = 0; i < MMX_ZERO_POSES; ++i) assert(fwrite(page, sizeof(page), 1, f) == 1);
  assert(!fclose(f));
}
static void player(void) {
  memset(ram, 0, sizeof(ram));
  ram[0xd1] = 2; ram[0xd2] = 4; ram[0xba9] = 2; ram[0xbcf] = 16;
  ram[0xbd3] = 4; ram[0xc11] = 64; ram[0xbb9] = 0x62;
  MmxZeroCancel(ram);
}
static void tick(unsigned held, unsigned pressed) {
  ram[0xbdf] = held; ram[0xbe3] = pressed; MmxZeroPlayerTick(ram);
}
int main(void) {
  player(); memcpy(before, ram, sizeof(ram));
  tick(0,0); assert(!memcmp(before, ram, sizeof(ram)));
  assert(MmxZeroUpgradeBits(0x81971c, 0) == 0);
  asset("zero-test.bin"); assert(MmxZeroLoad("zero-test.bin"));
  assert(MmxZeroUpgradeBits(0x81971c, 2) == 10);
  assert(MmxZeroUpgradeBits(0x8197da, 0) == 0); /* Special charge stays upgrade-gated. */
  const uint8_t normal[] = {0,255,6,14,0,0,255,7,17,8};
  const uint8_t dash[] = {0,5,6,8,0,0,255,9,17,8};
  memcpy(rom+0x32552,normal,10); memcpy(rom+0x33b38,dash,10);
  memset(rom+0x37fb0,255,40); memcpy(clean,rom,sizeof(rom));
  MmxZeroSetCollisionRom(rom,sizeof(rom));
  assert(rom[0x32555] == 18 && rom[0x3255a] == 21);
  assert(rom[0x33b3b] == 11 && rom[0x37fb2] == 12);
  assert(!MmxZeroLoad("missing-zero-test.bin") && MmxZeroEnabled());
  FILE *f = fopen("zero-test-bad.bin","wb"); assert(f); fputs("MMXZERO3",f); fclose(f);
  assert(!MmxZeroLoad("zero-test-bad.bin") && MmxZeroEnabled());
  player();
  for (int i=0;i<179;++i) tick(64,i==0?64:0);
  tick(0,0); assert(!MmxZeroGetState().combo); /* Almost charged cannot chain. */
  for (int i=0;i<180;++i) tick(64,i==0?64:0);
  tick(0,0); assert(MmxZeroGetState().combo == 1 && ram[0xc01] == 8);
  assert(ram[0x1f99] == 0); /* Dash/charge never grant equipment. */
  ram[0xc01] = 0;
  /* Native first shot is live. Zero must allocate another slot despite X1's lock. */
  ram[0x1228] = 1; ram[0x1229] = 2; ram[0x1232] = 3; ram[0xbdd] = ram[0xc25] = 1;
  for(int i=0;i<20;++i) tick(0,0);
  tick(64,64);
  assert(MmxZeroGetState().combo == 2 && ram[0x1268] == 1 && ram[0x1272] == 3 && ram[0xbdd] == 2);
  assert(ram[0xc25] == 1); /* Native initializer, not us, increments that counter. */
  for(int i=0;i<20;++i) tick(0,0);
  tick(64,64); MmxZeroState s = MmxZeroGetState();
  assert(s.slash == 1 && s.projectile == 0x12a8 && !s.combo);
  assert(!ram[s.projectile+0x20] && !ram[s.projectile+0x21]); /* Windup cannot hit. */
  assert(!MmxZeroWeaponTick(ram,s.projectile,1));
  for(int i=0;i<6;++i) tick(0,0);
  assert(ram[s.projectile+0x20] == 0xb0 && ram[s.projectile+0x21] == 0xff);
  assert(MmxZeroDamage(ram,0xe68,s.projectile,0) == 0);
  assert(MmxZeroDamage(ram,0xe68,s.projectile,128) == 128);
  assert(MmxZeroDamage(ram,0xe68,s.projectile,3) == 16);
  assert(MmxZeroHitbox(ram,0xe68,s.projectile,0xffb0) == 0);
  assert(MmxZeroHitbox(ram,0xea8,s.projectile,0xffb0) == 0xffb0);
  assert(MmxZeroDamage(ram,0xe68,s.projectile,3) == 0);
  assert(MmxZeroDamage(ram,0xea8,s.projectile,3) == 16);
  s = MmxZeroGetState(); MmxZeroResetState(); MmxZeroSetState(s);
  assert(MmxZeroGetState().hit_slots == s.hit_slots && MmxZeroGetState().slash == s.slash);
  MmxZeroState invalid = s; invalid.air = 2; MmxZeroSetState(invalid);
  assert(!MmxZeroGetState().slash); MmxZeroSetState(s);
  /* Changing weapon cancels only our melee slot; native shots remain alive. */
  ram[0xbdb] = 2; tick(0,0);
  assert(!MmxZeroGetState().slash && !ram[s.projectile] && ram[0x1228] && ram[0x1268] && ram[0xbdd] == 2);
  player(); s = (MmxZeroState){.combo=2}; MmxZeroSetState(s);
  for(unsigned d=0x1228;d<0x1428;d+=64) ram[d] = 1;
  tick(64,64); assert(MmxZeroGetState().combo == 2 && !MmxZeroGetState().slash);
  ram[0xbaa] = 0x0e; tick(0,0); assert(!MmxZeroGetState().combo);
  MmxZeroDisable(); MmxZeroSetCollisionRom(rom,sizeof(rom));
  assert(!memcmp(rom,clean,sizeof(rom)));
  remove("zero-test.bin"); remove("zero-test-bad.bin");
  puts("Zero: asset validation, collision restoration, combo, damage, cancellation and state tests passed");
  return 0;
}

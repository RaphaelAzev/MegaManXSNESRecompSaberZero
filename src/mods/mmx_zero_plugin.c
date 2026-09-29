#include "mod_runtime.h"
#include "host_paths.h"
#include "recomp_launcher.h"
#include "snes/interp_bridge.h"
#include "mmx_renderer.h"
#include "mmx_zero.h"
#include <stdio.h>

extern uint8_t g_ram[0x20000];
static void hook(CpuState *cpu, uint32_t pc) {
  if (!MmxZeroEnabled()) return;
  switch (pc & 0x7fffff) {
    case 0x00d6a7: MmxRendererObserveObject(g_ram, (uint16_t)(cpu->D + cpu->X)); break;
    case 0x00d76a: MmxRendererRecordPiece(g_ram, cpu->D); break;
    case 0x01971f: case 0x019796: case 0x0198ff: cpu->A |= 8; break;
    case 0x01815c: MmxZeroPlayerTick(g_ram); break;
    case 0x048f07: MmxZeroAnimationStart(cpu->D, cpu->A & 255); break;
    case 0x048eea: MmxZeroAnimationAdvance(cpu->D); break;
    case 0x01a57d: case 0x038b6f: case 0x038d87: case 0x038ed7:
    case 0x03951d: case 0x039841: case 0x039993: case 0x03a3cd:
    case 0x01a589: case 0x038b7b: case 0x038d93: case 0x038ee3:
    case 0x039529: case 0x03984d: case 0x03999f: case 0x03a3d9: {
      unsigned axis = (pc & 0xffff) == 0xa589 || (pc & 0xffff) == 0x8b7b ||
          (pc & 0xffff) == 0x8d93 || (pc & 0xffff) == 0x8ee3 ||
          (pc & 0xffff) == 0x9529 || (pc & 0xffff) == 0x984d ||
          (pc & 0xffff) == 0x999f || (pc & 0xffff) == 0xa3d9;
      unsigned value = MmxZeroMuzzle(g_ram,cpu->D,cpu->X,axis,cpu->A & 255);
      cpu->A = (cpu->A & 0xff00) | value;
      cpu->_flag_Z = !value; cpu->_flag_N = (value & 128) != 0;
      cpu->P = (cpu->P & ~0x82) | (cpu->_flag_Z ? 2 : 0) | (cpu->_flag_N ? 128 : 0);
      break;
    }
    case 0x00d3e7: {
      unsigned value = MmxZeroWeaponTick(g_ram, cpu->D, cpu->A & 255);
      cpu->A = (cpu->A & 0xff00) | value;
      cpu->_flag_Z = !value; cpu->_flag_N = (value & 128) != 0;
      cpu->P = (cpu->P & ~0x82) | (cpu->_flag_Z ? 2 : 0) | (cpu->_flag_N ? 128 : 0);
      break;
    }
    case 0x049e76: {
      unsigned original = cpu_read8(cpu, cpu->DB, (uint16_t)(0xef37 + cpu->Y));
      unsigned damage = MmxZeroDamage(g_ram, cpu->D, cpu->X, original);
      if (damage == original) break;
      /* Post-SBC: retain the interpreter's instruction timing, but recompute
       * its value and all arithmetic flags with our damage operand. The HP
       * store has not happened yet and SEC immediately precedes the SBC. */
      unsigned hp = g_ram[cpu->D + 0x27] & 127, result;
      if (cpu->_flag_D) {
        unsigned complement = damage ^ 255;
        int decimal = (hp & 15) + (complement & 15) + 1;
        if (decimal < 16) decimal = (decimal - 6) & (decimal < 6 ? 15 : 31);
        decimal += (hp & 240) + (complement & 240);
        cpu->_flag_V = ((hp & 128) == (complement & 128)) && ((complement & 128) != (decimal & 128));
        if (decimal < 256) decimal -= 96;
        cpu->_flag_C = decimal > 255; result = (unsigned)decimal & 255;
      } else {
        result = (hp - damage) & 255;
        cpu->_flag_C = hp >= damage;
        cpu->_flag_V = ((hp ^ damage) & (hp ^ result) & 128) != 0;
      }
      cpu->A = (cpu->A & 0xff00) | result;
      cpu->_flag_Z = result == 0; cpu->_flag_N = (result & 128) != 0;
      cpu->P = (cpu->P & ~0xc3) | cpu->_flag_C | (cpu->_flag_Z ? 2 : 0) |
          (cpu->_flag_V ? 64 : 0) | (cpu->_flag_N ? 128 : 0);
      break;
    }
    case 0x049c19:
      cpu->Y = (uint16_t)MmxZeroHitbox(g_ram, cpu->D, cpu->X, cpu->Y);
      cpu->_flag_Z = cpu->Y == 0; cpu->_flag_N = (cpu->Y & 0x8000) != 0;
      cpu->P = (cpu->P & ~0x82) | (cpu->_flag_Z ? 2 : 0) | (cpu->_flag_N ? 128 : 0);
      break;
  }
}
void MmxZeroRegisterHooks(void) {
  const unsigned pcs[] = {0x00d6a7, 0x00d76a, 0x01971f, 0x019796, 0x0198ff,
                          0x01815c, 0x00d3e7, 0x049e76, 0x049c19, 0x048f07, 0x048eea,
                          0x01a57d,0x038b6f,0x038d87,0x038ed7,0x03951d,0x039841,0x039993,0x03a3cd,
                          0x01a589,0x038b7b,0x038d93,0x038ee3,0x039529,0x03984d,0x03999f,0x03a3d9};
  for (unsigned i = 0; i < sizeof(pcs) / sizeof(pcs[0]); ++i)
    interp_bridge_set_pre_opcode_hook(pcs[i], hook);
}
static void activate(void) {
  char path[4096];
  const RecompLauncherCModProvider *provider = snes_mod_runtime_launcher_provider_c();
  RecompLauncherCModResource resource = {0};
  if (provider && provider->feature_resource_get &&
      provider->feature_resource_get(provider->ctx, "megaman-x.character.zero", "zero", 0, &resource) && resource.path[0])
    snprintf(path, sizeof(path), "%s", resource.path);
  else if (!snesrecomp_exe_dir_path("zero-x3.bin", path, sizeof(path))) return;
  if (!MmxZeroLoad(path)) {
    fprintf(stderr, "[mmx-zero] Cannot load extracted Zero assets: %s\n", path); return;
  }
  MmxZeroRegisterHooks();
  fprintf(stderr, "[mmx-zero] Experimental original-size Zero enabled\n");
}
static void reset(void) { MmxZeroCancel(g_ram); MmxZeroDisable(); }
SNES_MOD_CONSTRUCTOR(mmx_register_zero_plugin) {
  (void)snes_mod_register_reset_callback(reset);
  (void)snes_mod_register_activation_plugin("megaman-x.zero", activate);
}

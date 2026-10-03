#include "mod_runtime.h"
#include "mmx_coop.h"

static int g_mmx_tier2_diagnostics_active;

static void mmx_tier2_diagnostics_reset(void) {
  g_mmx_tier2_diagnostics_active = 0;
  MmxCoopSetDiagnosticsEnabled(false);
}

static void mmx_tier2_diagnostics_activate(void) {
  g_mmx_tier2_diagnostics_active = 1;
}
static void mmx_coop_diagnostics_activate(void) {
  MmxCoopSetDiagnosticsEnabled(true);
}

int mmx_tier2_diagnostics_enabled(void) {
  return g_mmx_tier2_diagnostics_active;
}

SNES_MOD_CONSTRUCTOR(mmx_register_tier2_diagnostics_plugin) {
  (void)snes_mod_register_reset_callback(mmx_tier2_diagnostics_reset);
  (void)snes_mod_register_activation_plugin(
      "megaman-x.tier2-diagnostics", mmx_tier2_diagnostics_activate);
  (void)snes_mod_register_activation_plugin(
      "megaman-x.coop-diagnostics", mmx_coop_diagnostics_activate);
}

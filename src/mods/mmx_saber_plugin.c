#include "mod_runtime.h"
#include "host_paths.h"
#include "recomp_launcher.h"
#include "mmx_source_assets.h"
#include "mmx_zero.h"
#include "saber/mmx_saber_plugin.h"
#include "sdl_compat.h"

#include <stdio.h>
#include <string.h>

static bool g_mmx_saber_enabled;

bool MmxSaberEnabled(void) {
  return g_mmx_saber_enabled;
}

static int prepare(char path[4096]) {
  const RecompLauncherCModProvider *provider =
      snes_mod_runtime_launcher_provider_c();
  RecompLauncherCModResource resource = {0};
  char error[512];
  if (!provider || !provider->feature_resource_get ||
      !provider->feature_resource_get(provider->ctx,
          "megaman-x.character.saber-zero", "saber-zero", 0, &resource) ||
      !resource.path[0])
    return 0;
  if (!snesrecomp_exe_dir_path("cache/mmx-source/x3-zero-v7.bin",
                              path, 4096))
    return 0;
  if (MmxSourceAssetsBuild(resource.path, 3, 1, path, error, sizeof(error)))
    return 1;
  fprintf(stderr, "[mmx-source] %s\n", error);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                           "Cannot prepare Mega Man mod", error, NULL);
  return 0;
}

static void activate(void) {
  char path[4096];
  char start[16] = {0};
  if (!prepare(path)) return;
  snes_mod_runtime_feature_option_value_c(
      "megaman-x.character.saber-zero", "saber-zero", "start",
      start, sizeof(start));
  MmxZeroSetStartCharacter(strcmp(start, "zero") != 0);
  MmxZeroSetModern(false);
  MmxZeroResetState();
  if (!MmxZeroLoad(path)) {
    fprintf(stderr, "[mmx-saber-zero] Cannot load extracted Zero assets: %s\n",
            path);
    return;
  }
  MmxZeroRegisterHooks();
  g_mmx_saber_enabled = true;
  fprintf(stderr, "[mmx-saber-zero] Saber Zero 0.0.1 enabled; starting as %s\n",
          strcmp(start, "zero") ? "X" : "Zero");
}

static void reset(void) {
  g_mmx_saber_enabled = false;
}

SNES_MOD_CONSTRUCTOR(mmx_register_saber_zero_plugin) {
  (void)snes_mod_register_reset_callback(reset);
  (void)snes_mod_register_activation_plugin("megaman-x.saber-zero", activate);
}

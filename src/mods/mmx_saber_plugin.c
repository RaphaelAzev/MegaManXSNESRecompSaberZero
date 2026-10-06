#include "mod_runtime.h"
#include "host_paths.h"
#include "recomp_launcher.h"
#include "mmx_source_assets.h"
#include "saber/mmx_saber_assets.h"
#include "saber/mmx_saber_wave.h"
#include "saber/mmx_saber_wave_assets.h"
#include "mmx_zero.h"
#include "saber/mmx_saber_plugin.h"
#include "sdl_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool g_mmx_saber_enabled;
static MmxSaberAssets *g_saber_assets;
static MmxSaberAssets *g_ride_assets;
static MmxSaberWave *g_saber_wave;

static const uint8_t kSaberManifestSha[32] = {
  0x4e, 0x29, 0x1e, 0x5f, 0x03, 0x57, 0xaf, 0xa0,
  0x35, 0x76, 0x14, 0xe0, 0xc4, 0x97, 0xf9, 0xb3,
  0x65, 0xee, 0x99, 0xe3, 0x70, 0x64, 0x5d, 0x84,
  0x99, 0xb2, 0xe4, 0xfd, 0x07, 0xf8, 0x0f, 0xd0,
};

static const uint8_t kRideManifestSha[32] = {
  0x49, 0x96, 0x7c, 0x80, 0x01, 0x9c, 0x16, 0x94,
  0xa5, 0xcd, 0x04, 0x72, 0x03, 0x7c, 0xab, 0x5b,
  0xdd, 0x7c, 0x2f, 0xab, 0x0b, 0x0e, 0x9d, 0xe4,
  0xf9, 0x06, 0xb9, 0x0d, 0x0d, 0xf8, 0xa7, 0xbd,
};

bool MmxSaberEnabled(void) {
  return g_mmx_saber_enabled;
}

bool MmxSaberAssetsLoaded(void) {
  return g_saber_assets != NULL;
}

bool MmxSaberRideAssetsLoaded(void) {
  return g_ride_assets != NULL;
}

bool MmxSaberWaveLoaded(void) {
  return g_saber_wave != NULL;
}

static int cache_path(const char *name, char path[4096]) {
  const char *test_cache = getenv("MMX_SABER_TEST_CACHE");
  char leaf[128];
  int written;
  if (!name || !path) return 0;
  if (test_cache && test_cache[0]) {
    written = snprintf(path, 4096, "%s/mmx-source/%s", test_cache, name);
    return written >= 0 && written < 4096;
  }
  written = snprintf(leaf, sizeof(leaf), "cache/mmx-source/%s", name);
  return written >= 0 && (size_t)written < sizeof(leaf) &&
      snesrecomp_exe_dir_path(leaf, path, 4096);
}

static int resolve_saber_rom(char path[4096]) {
  const RecompLauncherCModProvider *provider =
      snes_mod_runtime_launcher_provider_c();
  RecompLauncherCModResource resource = {0};
  int written;
  if (!provider || !provider->feature_resource_get ||
      !provider->feature_resource_get(provider->ctx,
          "megaman-x.character.saber-zero", "saber-zero", 0, &resource) ||
      !resource.path[0])
    return 0;
  written = snprintf(path, 4096, "%s", resource.path);
  return written >= 0 && written < 4096;
}

static int prepare_zero(char path[4096], const char rom[4096]) {
  char error[512];
  if (!path || !rom || !rom[0]) return 0;
  if (!snesrecomp_exe_dir_path("cache/mmx-source/x3-zero-v7.bin",
                              path, 4096))
    return 0;
  if (MmxSourceAssetsBuild(rom, 3, 1, path, error, sizeof(error)))
    return 1;
  fprintf(stderr, "[mmx-source] %s\n", error);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                           "Cannot prepare Mega Man mod", error, NULL);
  return 0;
}

static void report_wave_prepare_failure(const char *wave_path,
                                        const char *reason) {
  char message[2048];
  snprintf(message, sizeof(message),
      "Saber Zero wave cache is missing or invalid:\n"
      "  wave: %s\n"
      "  reason: %s\n",
      wave_path && wave_path[0] ? wave_path : "<unresolved>",
      reason && reason[0] ? reason : "wave extraction failed");
  fprintf(stderr, "[mmx-saber-zero] %s\n", message);
  if (!getenv("MMX_SABER_TEST_CACHE_ONLY"))
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot enable Saber Zero",
                             message, NULL);
}

static int prepare_wave(const char rom[4096], char wave_path[4096]) {
  char error[512];
  if (!rom || !rom[0] || !cache_path("x3-saber-wave-v1.bin", wave_path)) {
    report_wave_prepare_failure(wave_path, "cannot resolve wave cache path");
    return 0;
  }
  if (MmxSaberWaveAssetsBuild(rom, wave_path, error, sizeof(error))) return 1;
  report_wave_prepare_failure(wave_path, error);
  return 0;
}

static void release_saber_assets(void) {
  MmxSaberWaveFree(g_saber_wave);
  MmxSaberAssetsFree(g_saber_assets);
  MmxSaberAssetsFree(g_ride_assets);
  g_saber_wave = NULL;
  g_saber_assets = NULL;
  g_ride_assets = NULL;
}

static void report_saber_asset_failure(const char *saber_path,
                                       const char *ride_path,
                                       const char *wave_path,
                                       const char *reason) {
  char message[2048];
  snprintf(message, sizeof(message),
      "Saber Zero assets are missing or invalid:\n"
      "  saber: %s\n"
      "  ride: %s\n"
      "  wave: %s\n"
      "  reason: %s\n\n"
      "Regenerate them with:\n"
      "python -I tools/saber/convert_saber_zero.py --manifest "
      "tools/saber/saber_zero_manifest.json --source-dir SaberSprites "
      "--out <exe-dir>/cache/mmx-source/saber-v1.bin\n"
      "python -I tools/saber/convert_saber_zero.py --manifest "
      "tools/saber/ride_zero_manifest.json --source-dir SaberSprites "
      "--out <exe-dir>/cache/mmx-source/ride-zero-v1.bin",
      saber_path && saber_path[0] ? saber_path : "<unresolved>",
      ride_path && ride_path[0] ? ride_path : "<unresolved>",
      wave_path && wave_path[0] ? wave_path : "<unresolved>",
      reason && reason[0] ? reason : "invalid sidecar");
  fprintf(stderr, "[mmx-saber-zero] %s\n", message);
  /* The real plugin uses the upstream message-box style. The isolated ROM
   * runner opts out so a deliberate missing-cache test cannot block MinGW. */
  if (!getenv("MMX_SABER_TEST_CACHE_ONLY"))
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Cannot enable Saber Zero",
                             message, NULL);
}

static int load_saber_assets(const char *saber_path, const char *ride_path,
                             char reason[256]) {
  MmxSaberAssets *saber;
  MmxSaberAssets *ride;
  char local_reason[128];

  release_saber_assets();
  saber = MmxSaberAssetsLoadFile(saber_path, kSaberManifestSha,
                                 local_reason, sizeof(local_reason));
  if (!saber) {
    snprintf(reason, 256, "saber-v1.bin: %s",
             local_reason[0] ? local_reason : "parse failed");
    return 0;
  }
  ride = MmxSaberAssetsLoadFile(ride_path, kRideManifestSha,
                                local_reason, sizeof(local_reason));
  if (!ride) {
    MmxSaberAssetsFree(saber);
    snprintf(reason, 256, "ride-zero-v1.bin: %s",
             local_reason[0] ? local_reason : "parse failed");
    return 0;
  }
  g_saber_assets = saber;
  g_ride_assets = ride;
  reason[0] = '\0';
  return 1;
}

static int load_saber_wave(const char *wave_path, char reason[256]) {
  MmxSaberWave *wave;
  char local_reason[128];

  MmxSaberWaveFree(g_saber_wave);
  g_saber_wave = NULL;
  wave = MmxSaberWaveLoadFile(wave_path, local_reason, sizeof(local_reason));
  if (!wave) {
    snprintf(reason, 256, "x3-saber-wave-v1.bin: %s",
             local_reason[0] ? local_reason : "parse failed");
    return 0;
  }
  g_saber_wave = wave;
  reason[0] = '\0';
  return 1;
}

static void activate(void) {
  char path[4096] = {0}, rom[4096] = {0}, wave_path[4096] = {0};
  char saber_path[4096] = {0}, ride_path[4096] = {0};
  char reason[256] = {0};
  char start[16] = {0};
  g_mmx_saber_enabled = false;
  release_saber_assets();
  if (!resolve_saber_rom(rom) || !prepare_zero(path, rom)) return;
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
  if (!prepare_wave(rom, wave_path)) return;
  if (!cache_path("saber-v1.bin", saber_path) ||
      !cache_path("ride-zero-v1.bin", ride_path) ||
      !load_saber_assets(saber_path, ride_path, reason)) {
    report_saber_asset_failure(saber_path, ride_path, wave_path, reason);
    return;
  }
  if (!load_saber_wave(wave_path, reason)) {
    release_saber_assets();
    report_wave_prepare_failure(wave_path, reason);
    return;
  }
  g_mmx_saber_enabled = true;
  fprintf(stderr, "[mmx-saber-zero] saber-v1.bin, ride-zero-v1.bin, and "
                  "x3-saber-wave-v1.bin loaded\n");
  fprintf(stderr, "[mmx-saber-zero] Saber Zero 0.0.1 enabled; starting as %s\n",
          strcmp(start, "zero") ? "X" : "Zero");
}

static void reset(void) {
  g_mmx_saber_enabled = false;
  release_saber_assets();
}

SNES_MOD_CONSTRUCTOR(mmx_register_saber_zero_plugin) {
  (void)snes_mod_register_reset_callback(reset);
  (void)snes_mod_register_activation_plugin("megaman-x.saber-zero", activate);
}

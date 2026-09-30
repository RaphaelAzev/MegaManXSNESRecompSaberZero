/* Real catalog + launcher policy checks, using a caller-owned scratch root. */
#include "mmx_password_save.h"
static void netplay_policy_checks(const char *root, const char *rom, const char *x3) {
  const char *digest = "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32";
  const char *coop = "megaman-x.coop", *zero = "megaman-x.character.zero";
  const char *wide = "megaman-x.enhancement.widescreen";
  check(x3 && snes_mod_runtime_initialize_c(root, "megaman-x-us", digest), "netplay private catalog initializes");
  const RecompLauncherCModProvider *base = snes_mod_runtime_launcher_provider_c();
  check(base->feature_resource_set_path(base->ctx, coop, "coop", "x3-rom", x3), "local X3 source selected");
  base->feature_enable(base->ctx, coop, "coop", 0);
  base->feature_enable(base->ctx, zero, "zero", 1);
  base->feature_enable(base->ctx, wide, "widescreen", 1);
  base->feature_enable(base->ctx, "megaman-x.enhancement.password-save", "password_save", 1);
  base->feature_set_option(base->ctx, wide, "widescreen", "aspect", "adaptive");
  check(base->commit(base->ctx, rom), "offline Zero and Adaptive plan commits");
  char offline[2048], after[2048];
  snes_mod_runtime_effective_set_c(offline, sizeof(offline));
  static const RecompLauncherCNetplayCallbacks net = {0};
  RecompLauncherCGameInfo gi = {.mods = base, .netplay = &net, .netplay_supported = 1};
  MmxNetplayConfigureLauncher(&gi);
  LauncherModel m = {0};
  m.netplay_supported = true; m.netplay_mode_changed = gi.netplay_mode_changed;
  launcher_model_set_view(&m, LNG_VIEW_NETPLAY_MODE);
  const RecompLauncherCModProvider *p = gi.mods;
  check(MmxNetplayActive() && snes_mod_runtime_feature_enabled_c(coop, "coop") &&
      !snes_mod_runtime_feature_enabled_c(zero, "zero"), "entering Netplay replaces exchange with co-op");
  check(!p->feature_enable(p->ctx, coop, "coop", 0) &&
      !p->feature_enable(p->ctx, zero, "zero", 1), "required character mode cannot be undone in the room");
  RecompLauncherCModChoice choice = {0};
  check(p->feature_choice_get(p->ctx, wide, "widescreen", "aspect", 0, &choice) &&
      !strcmp(choice.value, "16:9") &&
      !p->feature_choice_get(p->ctx, wide, "widescreen", "aspect", 3, &choice), "netplay offers exactly three fixed view ratios");
  check(!p->feature_set_option(p->ctx, wide, "widescreen", "aspect", "adaptive"), "Adaptive is unavailable online");
  check(!gi.netplay->allow_spectators_get && !gi.netplay->allow_spectators_set &&
      !gi.netplay->host_can_spectate(NULL), "spectator controls are unavailable");
  check(p->feature_set_option(p->ctx, coop, "coop", "player1", "zero"), "host chooses the P1 character");
  const char *ratios[] = {"16:9", "21:9", "32:9"};
  for (int ratio = 0; ratio < 3; ++ratio) {
    check(p->feature_set_option(p->ctx, wide, "widescreen", "aspect", ratios[ratio]) &&
        p->commit_netplay(p->ctx, rom), "fixed view commits with local ROM validation");
    snes_mod_runtime_activate_plugins_c();
    memset(g_ram + 0x1e60, 0, 12);
    MmxPasswordPrefill(g_ram);
    check(!g_ram[0x1e60], "online password screen keeps synchronized defaults");
    check(MmxCoopEnabled() && MmxCoopGetState().players[0].character == MMX_COOP_ZERO, "netplay activates the selected roster");
    int expected_width = 0;
    for (int pixels = 0; pixels < 3; ++pixels) {
      g_config.display_aspect = pixels;
      int w, h; MmxPrepareFrame(800, 600, &w, &h);
      if (!pixels) expected_width = w;
      check(w == expected_width, "offline pixel preference cannot change match geometry");
      MmxPrepareFrame(3200, 900, &w, &h);
      check(w == expected_width && h == 224, "window resizing preserves the agreed logical field");
    }
  }
  /* A peer's malformed plan also goes through validation, not just the UI. */
  base->feature_set_option(base->ctx, wide, "widescreen", "aspect", "adaptive");
  check(!p->commit_netplay(p->ctx, rom), "launch rejects an Adaptive peer plan");
  const uint64_t generation = RtlStateGeneration();
  g_netplay_session = true;
  HandleCommand(kKeys_Reset, true);
  HandleCommand(kKeys_Load, true);
  HandleCommand(kKeys_Pause, true);
  HandleCommand(kKeys_Turbo, true);
  HandleCommand(kKeys_SaveStateMenu, true);
  HandleCommand(kKeys_Rewind, true);
  check(RtlStateGeneration() == generation && !g_paused && !g_turbo &&
      !g_savestate_menu_hotkey && !g_rewind_hotkey, "local state shortcuts cannot change an online match");
  HandleCommand(kKeys_Controls + 8, true);
  check(g_input_state != 0, "SNES controls remain available during netplay");
  g_input_state = 0;
  g_netplay_session = false;
  launcher_model_set_view(&m, LNG_VIEW_DASHBOARD);
  snes_mod_runtime_effective_set_c(after, sizeof(after));
  check(!MmxNetplayActive() && !strcmp(offline, after), "Back restores all offline selections");
  check(snes_mod_runtime_initialize_c(root, "megaman-x-us", digest), "reload persisted offline plan");
  snes_mod_runtime_effective_set_c(after, sizeof(after));
  check(!strcmp(offline, after), "netplay commits never overwrite the offline plan on disk");
  puts("MMX NETPLAY POLICY CHECKS PASSED");
}

#if defined(SNES_HAS_LOBBY_CLIENT)
static uint16_t netplay_test_pad(unsigned tick, int seat) {
  if (tick < 40) return seat ? SNES_PAD_RIGHT : 0;
  if (tick < 70) return seat ? SNES_PAD_B : SNES_PAD_RIGHT;
  if (tick < 100) return tick % 12 == 0 ? SNES_PAD_Y : 0;
  if (tick < 280) return SNES_PAD_Y;
  if (tick < 310) return tick % 10 == 0 ? SNES_PAD_Y : 0;
  if (tick == 320 || tick == 340) return seat ? SNES_PAD_START : 0;
  return 0;
}
static void netplay_pair_checks(const char *root, const char *rom, const char *x3,
                                const char *fixture, int guest) {
  const char *coop = "megaman-x.coop", *wide = "megaman-x.enhancement.widescreen";
  check(x3 && fixture && snes_mod_runtime_initialize_c(root, "megaman-x-us",
      "b8f70a6e7fb93819f79693578887e2c11e196bdf1ac6ddc7cb924b1ad0be2d32"), "pair catalog initializes");
  const RecompLauncherCModProvider *base = snes_mod_runtime_launcher_provider_c();
  check(base->feature_resource_set_path(base->ctx, coop, "coop", "x3-rom", x3), "pair supplies local X3 ROM");
  const char *x2 = getenv("MMX_COOP_X2_ROM");
  check(x2 && base->feature_resource_set_path(base->ctx, "megaman-x.weapons.x2", "weapons", "x2-rom", x2), "pair supplies local X2 ROM");
  base->feature_enable(base->ctx, coop, "coop", 0);
  base->feature_enable(base->ctx, "megaman-x.character.zero", "zero", 0);
  base->feature_enable(base->ctx, wide, "widescreen", 0);
  check(base->commit(base->ctx, rom), "pair offline plan commits");
  char offline[2048]; snes_mod_runtime_effective_set_c(offline, sizeof(offline));
  host_lobby_ensure_init();
  RecompLauncherCGameInfo gi = {.mods=base, .netplay=snes_host_lobby_callbacks(), .netplay_supported=1};
  MmxNetplayConfigureLauncher(&gi); gi.netplay_mode_changed(1);
  const RecompLauncherCNetplayCallbacks *np = gi.netplay;
  const RecompLauncherCModProvider *mods = gi.mods;
  if (!guest) {
    mods->feature_set_option(mods->ctx, coop, "coop", "player1", "zero");
    mods->feature_enable(mods->ctx, wide, "widescreen", 1);
    mods->feature_set_option(mods->ctx, wide, "widescreen", "aspect", "21:9");
    mods->feature_enable(mods->ctx, "megaman-x.weapons.x2", "weapons", 1);
    mods->feature_enable(mods->ctx, "megaman-x.weapons.x3", "weapons", 1);
  }
  np->set_player_name(np->ctx, guest ? "GuestTest" : "HostTest");
  char endpoint[64] = "127.0.0.1:17970", bind[64] = "0.0.0.0:17971";
  RecompLauncherCSettings settings = {0};
  if (!guest) check(np->create(np->ctx, "MMX private test", endpoint, "", &settings, 1, 2)==0, "host creates a two-seat LAN room");
  else { SDL_Delay(250); check(np->join(np->ctx, "lan:127.0.0.1:17970", "", bind)==0, "guest joins LAN room"); }
  RecompLauncherCNetplayLaunch launch = {0};
  uint32_t started = SDL_GetTicks(); int requested = 0;
  while (SDL_GetTicks() - started < 15000) {
    np->pump(np->ctx);
    if (!guest && !requested && np->member_count(np->ctx)==2 && SDL_GetTicks()-started>1000) {
      check(np->request_start(np->ctx, &settings)==0, "host starts agreed room"); requested=1;
    }
    if (np->launch_pending(np->ctx) && np->fill_launch(np->ctx, &launch)) break;
    SDL_Delay(2);
  }
  check(launch.enabled && launch.local_slot==guest && launch.max_slots==2, "lobby launch maps each peer to its own seat");
  check(mods->commit_netplay(mods->ctx, rom), "room plan validates before activation");
  snes_mod_runtime_activate_plugins_c();
  check(MmxCoopEnabled() && MmxCoopGetState().players[0].character==MMX_COOP_ZERO &&
      g_mmx_custom_aspect==MMX_ASPECT_21_9 && MmxWeaponsPageEnabled(1) && MmxWeaponsPageEnabled(2),
      "guest adopts host roster, fixed viewport and both weapon sets");
  check(RtlLoadSnapshot(fixture), "private grounded fixture loads before connecting");
  check(MmxCoopEnable(MMX_COOP_ZERO), "fixture uses the agreed roster");
  coop_frames(8,0,0);
  MmxCoopState s=MmxCoopGetState();
  check(coop_place_fixture_partner(coop_x(&s.players[0])+32,coop_y(&s.players[0])), "private P2 fixture placement");
  coop_frames(40,0,0);
  /* Both imported weapon pools must survive actual rollback replay too. */
  coop_weapon_select(0,1,4); coop_weapon_select(1,2,5); g_ram[0x1f99]|=2;
  coop_frames(2,0,0);
  SnesHostLaunchResult result;
  snes_host_app_apply_launch(&launch, &result);
  char plan[2048]; snes_mod_runtime_effective_set_c(plan, sizeof(plan));
  snes_netplay_rb_set_modset(plan, snes_mod_runtime_check_set_c, snes_mod_runtime_adopt_set_c);
  snes_netplay_rb_set_replay_frame(NetplayReplayFrame);
  check(snes_netplay_start(&result.net_cfg)==0, "real rollback session starts");
  unsigned frames=0; int delayed=0;
  started=SDL_GetTicks();
  while (frames<480 && SDL_GetTicks()-started<30000) {
    if (guest && frames==40 && !delayed) { SDL_Delay(110); delayed=1; }
    if (snes_netplay_needs_local_sample())
      snes_netplay_stage_local(netplay_test_pad(snes_netplay_sim_tick(),guest));
    if (snes_netplay_poll_admit()) {
      NetplayReplayFrame(snes_netplay_published_inputs() | snes_netplay_active_mask(),frames);
      snes_netplay_finish_frame(); ++frames;
    } else SDL_Delay(1);
    if (snes_netplay_refusal()) {
      fprintf(stderr, "pair refused: %s\n", snes_netplay_refusal());
      check(0, "match remains admitted");
    }
    if (snes_netplay_rb_desync_count()) break;
  }
  check(frames==480 && !snes_netplay_rb_desync_count(), "two peers advance 480 frames without a digest fork");
  s=MmxCoopGetState();
  check(MmxCoopValidState(&s) && s.players[0].status==MMX_COOP_ALIVE && s.players[1].status==MMX_COOP_ALIVE,
      "both players remain alive with independent contexts");
  printf("PAIR_RESULT seat=%d frames=%u confirmed=%u episodes=%u replay=%llu x=%u,%u hp=%u,%u\n",
      guest,frames,snes_netplay_rb_confirmed_through(),snes_netplay_rb_episode_count(),
      (unsigned long long)snes_netplay_rb_resim_ticks(),coop_x(&s.players[0]),coop_x(&s.players[1]),
      s.players[0].body[0x27]&127,s.players[1].body[0x27]&127);
  fflush(stdout);
  snes_netplay_shutdown(); snes_host_lobby_shutdown();
  gi.netplay_mode_changed(0);
  snes_mod_runtime_effective_set_c(plan,sizeof(plan));
  check(!strcmp(plan,offline), "peer restores its own offline plan");
  puts("MMX NETPLAY PAIR CHECKS PASSED");
}
#endif

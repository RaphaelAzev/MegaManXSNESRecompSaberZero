# X3 Zero in X1: current implementation and handoff

Audited 2026-09-29 against `feat/x3-zero-port`, through `686dd61`/`bdbb9b7`.
Worktree: `F:/Projects/snesrecomp/_wt_mmx_zero`. This is the authoritative
current Zero document. [zero-spike.md](zero-spike.md) preserves the original
feasibility experiment and older findings; its early state sizes and paths
are historical. Scope: [roadmap](zero-weapons-coop-roadmap.md).

Central Beads: `beads-8wg.1.31` full Zero port (still in progress), `.30`
Select exchange (closed), `.33` independent HP (closed), `.32` X2/X3 weapons
(in progress), `.34` later co-op (not implemented).

**Work order requested by the owner:** finish this Zero documentation first,
commit it, then resume the remaining X2/X3 weapons. Keep updating the source
findings as implementation proceeds. Work solo, with clean bounded commits.

## Implemented behavior and limits

| Area | Current implementation | Validation / remaining limit |
| --- | --- | --- |
| Body | Original-size X3 Zero, original body/saber art and animation records | No resizing needed in tested areas; complete campaign clearance is unverified |
| Movement | X1 native movement with Zero's innate dash; original X3 base values retained | Five measured trajectories match X3; water/walls/ladders and special terrain need broader coverage |
| Collision | X3 normal/dash body and terrain geometry, translated to X1's foot origin | Actual collision geometry changes, not only a sprite replacement; disabled/X identity restores X1 geometry |
| Buster combo | Original charge tiers; ground/air first shot, second shot and final saber | Body timing measured against X3; first two beams retain X1 projectile art/travel/effect lifetimes |
| Saber | Original ground/air body and blade frames; phase-dependent hitboxes | Native enemy/boss responses exercised; positive saber damage 16 is an adaptation, not universal X3 damage equivalence |
| X1 specials | All eight native normal/charged attacks, Zero muzzle offsets and selected effect adjustments | Activation, energy, upgrade gate and both directions checked; remaining visual/state combinations need playtesting |
| Hurt/fades | Replacement tied to submitted OAM, including hidden blink frames and pause fades | Renderer checks cover the reported X flashes and menu transitions |
| HUD | Original X3 Z badge with its frame/palette | ROM-to-render comparison matched all 218 nontransparent badge pixels |
| Pause/title | Original Zero body in pause and title cursor; title confirmation still shoots | Upgraded/unupgraded menus and native title shot checked |
| Life heads | **Original X 1-up artwork**, both menu and pickups | Owner explicitly rejected the custom Zero head; do not restore any custom version |
| Select exchange | Grounded idle X/Zero exchange using original blue/red teleport art; game tasks freeze | Both directions, held/midair Select, frozen live projectile and deterministic replay checked |
| Health | Independent current HP, shared maximum; pickups/subtanks heal active character only | Native pickups, exchange, old-save migration, death/life loss and both-pool respawn refill checked |
| Persistence | Host character state included in saves, rollback, rewind and captures | Legacy Zero states supported; public mod/save incompatibility UX remains unfinished |

The original question was whether this required disassembling nearly all of
both games. It did not: the implementation uses bounded source investigation,
asset tables and integration hooks. This does not establish perfect X3 fidelity
or a complete campaign playthrough.

## Public distribution and asset provenance

The [source-ROM release contract](mod-source-roms.md) is mandatory for **all**
Zero content and the later weapon/co-op mods. Users provide the supported X1
ROM and their own X3 ROM for Zero; the complete weapon set additionally needs
X2. Extract assets locally. Public packages must not bundle ROMs, `zero-x3.bin`,
`x-weapons.bin`, decoded art/audio, private fixtures or research captures.
Public source-ROM selection, cache provenance/versioning and packaging audits
are release prerequisites, not completed features.

`tools/extract_zero.py` validates original USA X3 SHA-256
`65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7`
after accepting an optional 512-byte copier header. Its current `MMXZERO6`
cache contains:

- 117 body poses (group `$4A`), 21 saber-body poses (`$4B`) and 14 blade poses
  (`$50`), indexed at 128x128 with origin `(64,64)`. This canvas is transparent
  padding around the original pixels, not a 128-pixel-tall character.
- Original body/saber palettes; 40 bytes of ground/air saber collision bounds
  translated eight pixels upward into X1 coordinates.
- Four original 8x8 HUD badge tiles and their palette.
- `$474` bytes containing 136 original body-animation directory entries and
  records, plus 196 bytes of pose-specific firing data.

The loader rejects a wrong header, dimensions/counts, truncated/trailing data,
invalid pixels, invalid bounds and invalid animation/muzzle entries. Missing
or invalid assets leave the mod inactive with a log message. Source addresses
below and extraction code are durable; extracted assets remain local.

## Mechanics and integration

`src/mmx_zero.c` owns the asset cache and 40-byte `MmxZeroState`.
`MmxZeroEnabled()` means assets are loaded; `MmxZeroActive()` additionally means
the current player is Zero. Keep that distinction: choosing X must restore his
native progression/capabilities while the shared mod remains loaded.

X1 still owns stage scripts, collision resolution, inventory, boss progression
and normal enemy behavior. Zero changes the playable actor; story NPC Zero is
separate. No simultaneous second player exists in this mod.

The 180-byte base movement tables are identical in the two original ROMs.
Innate dash is granted at three capability reads by returning upgrade bit
`$08`; the equipment byte is not changed. **Arms are bit `$02`**, so innate dash
does not unlock charged special weapons for either character.

The normal body damage half-size is `(6,18)`, center Y `-5`; dash is `(6,11)`,
center Y `+2`. Both use terrain half-height 21 and center Y `-5`. Geometry
comes from X3 with a `-8` Y translation to align feet. Expected-byte guards
patch only the live cartridge copy and restore X1 bounds for X/disabled mode;
source ROM files are never written.

The animation adapter maps all 81 X1 sequence entries to X3 group `$4A`
records, preserving interior sequence aliases. It mirrors body presentation
without replacing X1's gameplay animation/event fields. X1's Hadouken entries
use adapted Zero forward-firing/recovery poses. Complete mapping is not proof
that every capsule, ride-armor, death or scripted state has been playtested.

### Charge and saber sequence

Held-gameplay-frame thresholds are 21 (half), 81 (single full), 141 (two
full busters) and 201 (two busters plus saber). At 60 Hz, the highest tier takes
about 3.35 seconds. Release starts the first shot; a later press after its
body recovery starts the second. Another press starts the saber only when the
preceding beams **and their native disappearance effects** have retired.
There is no invented fixed delay between the second beam and saber readiness.

The original grounded first shot emits on frame 8 and reaches idle on 18;
the second emits on 10 and reaches idle on 29. Original body records drive
emission, facing lock, ground/air phase changes and the small airborne motion
holds. Native jump/gravity/terrain logic continues during airborne actions.
Saber recovery returns on frame 44, with idle visible on 45. The blade is
visible during slash ages 7-31; the damaging arc has five shorter phases and
ends before the visible recovery finishes.

The first two beams use native X1 full-charge class 3. A tracked slot mask
allows both to coexist without replacing their native initialization or
retirement. Saber occupies an ordinary projectile slot with ownership tag
`$5A53` at `+$3E`; its native class update is suppressed. Original ground/air
arc bounds feed X1's enemy collision routine. Positive eligible damage becomes
16, while native zero/immunity/reflection paths remain. An enemy-slot mask
prevents repeat hits against the same ordinary enemy slot during one swing.

Hurt, incompatible player actions, weapon changes and menu entry cancel stored
combat. Cancellation preserves body animation, active identity and HP pools;
full reset/load is separate. Charge cleanup sends native command `$17` once
when RAM `$0C2F` bit `$40` is set, then clears charge counters. Buster emissions
send sound `$02` once each through X1's sound ring. This fixed the persistent
charge noise; original X3 saber/audio-bank import is still pending.

### X1 weapons and presentation

Homing Torpedo, Chameleon Sting, Rolling Shield, Fire Wave, Storm Tornado,
Electric Spark, Boomerang Cutter and Shotgun Ice keep X1's real weapon logic
and progression. Normal/charged forms use original X3 muzzle coordinates;
delayed projectiles recover the firing pose through their saved native index.
X1 continues to apply facing, spread, trajectories and energy use.

Additional origin corrections affect the actual projectile coordinate, so
art and collision move together: Storm Tornado six pixels forward, Boomerang
Cutter eight pixels up, charged Rolling Shield six pixels up. The shield was
shifted rather than resized. Charging motes are spread 25% farther from the
player center and six pixels upward, retaining their original pixel art and
timing. Charged Sting remaps Zero's material/shade roles into the native live
cycling palette while retaining readable skin/outline colors. Other charge,
hit and weapon-palette fidelity remains part of visual acceptance.

`src/mmx_renderer.c` replaces submitted player/menu/title pieces, suppresses
X armor overlays and draws the original Zero pixels. Original OAM attribution
is retained across repeated pause-fade frames. Live OAM matching prevents a
visible old X frame from leaking through while also honoring truly hidden
invulnerability frames. The compositor runs at native width whenever Zero is
enabled; widescreen remains an independent setting.

Pause body is centered at `(128,152)`; title selection retains X1's real player
cursor animation and confirmation projectile. The HUD badge is independent of
body blinking. Original X life-head pixels and native collection behavior are
deliberate final requirements, superseding every earlier custom icon attempt.

### Select and independent HP

Select must be a new press during grounded, stationary, idle gameplay, outside
menus, hurt, ladders, ride armor and scripted transitions. Burst/saber actions
block exchange. The original player feet remain stationary; only presentation
moves vertically. Five one-frame morphs plus a two-frame energy ball precede
the outgoing column; departure speed is `$0AA6` in 8.8 units. After identity
changes offscreen, wait X3's 30-frame exchange interval; arrival descends at
eight pixels/frame and reverses the seven-frame morph. Native sounds are
`$0F` out and `$0E` in. Held Select cannot repeatedly exchange.

`src/mmx_rtl.c` calls `MmxZeroSwapTick` after NMI/input and returns before the
cooperative game scheduler while exchange is active. Enemies, projectiles,
items, camera and stage scripts stop; NMI, input, audio and rendering continue.
Save/restore includes the frozen sprite queues and complete exchange state.
Input edges are cleared before gameplay resumes.

HP index 0 is Zero, 1 is X. Native RAM `$0BCF` always represents the active
character. Health pickups **and subtanks heal only that character**. Heart
tanks update the shared maximum at `$1F9A`; this does not mean ordinary pickups
heal the reserve. Old saves seed both pools once from their single current HP.
Active death follows native life loss; native stage/checkpoint HP initialization
refills both pools to the shared maximum. It does not automatically switch to
the living reserve. Single-player HUD shows the active character's bar.
Weapon selection/energy are shared; checkpoint death preserves weapon energy.

## Source and hook address map

Hex addresses are SNES CPU LoROM addresses unless marked RAM or normalized
ROM offset. `$04`/`$84`, for example, may name mirrored ROM banks. These findings
come from the local original ROMs and measured recomp runs.

| Source | Address / purpose |
| --- | --- |
| X3 sprite data | Layout root `$8D:8000`; group `$4A` DMA `$85:D6A8`, `$4B` `$85:DB47`, `$50` `$85:E6E0` |
| X3 palettes | List root `$86:8180`, keys `$D0/$D2`; saber palette `$8C:B5A0` |
| X3 animations | Group `$4A` directory/records `$3F:CC74`, length `$474`; normal body selection `$04:A63C`, transfers `$04:BCA2` |
| X3 muzzle | Pose map `$39:9161`, signed Y/left-facing-X pairs `$39:91D9`; source helper `$81:8BA9` |
| X3 geometry | Normal/dash `$86:B40E/B422`; ground/air saber `$86:B837/B84B` |
| Shared movement | X1 `$86:B9B1`, X3 `$86:B272`: 30 six-byte velocity/acceleration records |
| X3 HUD | Selector `$84:DCE6`, DMA list `$5C` at `$86:9AB3`; CHR `$2C:8D20/$2C:8DE0`, palette `$8C:B0E0` |
| Teleport art | X3 group `$4A` poses `$3C-$42`; X1 layouts `$8D:8000`, DMA `$85:A597`, buster palette list `$0100` |
| Teleport sequence | X1 `$49/$48`, X3 `$7F/$7E`; X3 exchange delay `$84:8DBF` |
| X1 player RAM | `$0BA8`; X/Y `+$05/+$08`, facing `+$11`, action `+$02`; native active HP `$0BCF`, ground flag `$0BD3 & 4` |
| X1 combat RAM | Projectile pool `$1228..1427`, stride `$40`; count `$0BDD`; selected weapon `$0BDB`; upgrade byte `$1F99` |
| X1 sound | Ring `$0B72`, producer `$0BA3`; original queue routine `$80:88CD`; charge stop `$81:9890` |
| Live collision patches | Normalized ROM `$32552` normal, `$33B38` dash, `$37FB0..37FD7` saber bounds; extended-weapon bounds use separate `$37F80..37F9F` |
| X1 body animation hooks | Start `$84:8F07`, advance `$84:8EEA` |
| X1 player hooks | Tick `$81:815C`, end `$81:8165`; dash capability reads `$81:971C/9793/98FC` |
| X1 projectile hooks | Active read `$00:D3E5`; positive-damage path `$84:9E6E..9E76`; hitbox read `$84:9C16` |
| X1 native origins | Buster `$81:A578..A5B5`; full special-weapon sites are `MUZZLE_PCS`/`ORIGIN_PCS` in `tools/apply_zero_hooks.py` |
| X1 HP initialization | Compiled block `$00:9DA6`, interpreted post-store `$00:9DCA` |

`tools/apply_zero_hooks.py` validates 23 generated-code integration sites.
`src/mods/mmx_zero_plugin.c` supplies matching interpreter hooks, including
correct instruction flags/timing. Generated block addresses and interpreted
post-instruction addresses are intentionally different. Edit the patcher,
not generated bank files. `src/main.c` reapplies live collision bounds before
each frame; `src/mmx_rtl.c` owns scheduler/save integration.

## Persistence and developer reproduction

| Game chunk | Zero state prefix | Capture version introduced at that checkpoint |
| --- | --- | --- |
| v3 stock | None | v2 |
| v4 | 12-byte combat | v3 |
| v5 | 18-byte combat + body animation | v4 |
| v6 | 30-byte extended combo timing | v5 |
| v7 | 36-byte identity + exchange | v6 |
| v8 current Zero-only | 40-byte state including HP | v7 |
| v9 extended inventory | 40-byte Zero + weapon inventory | v8 |
| v10 extended combat | Same Zero + inventory + owned projectile simulation | v9 |
| v11 current extended inventory | Same Zero + 40-byte fractional inventory + projectile simulation | v10 |

Later weapon-format changes may advance the last rows; consult `mmx_rtl.c`
and the [weapon notes](x-weapons-port.md). The current capture writer always
writes v10, including Zero-only captures; the earlier capture rows describe
readable historical layouts. Game saves still choose v3/v8/v11 according to
enabled assets. Old prefixes initialize new fields
safely. Asset availability and mod identity matter when loading; the package's
`requires-same-mods` declaration does not yet enforce public save isolation.

Build/extract in the worktree, following the repository README and using the
existing `build-zero` configuration. Example local asset command:

```powershell
python tools/extract_zero.py ../MegamanX3SNESRecomp/mmx3.sfc build-zero/port-work/zero-x3.bin
```

Package: `mods/preloaded/packages/megaman-x.character.zero/0.1.0/manifest.toml`.
Activation plugin is `megaman-x.zero`; the resource picker can select a cache,
otherwise it reads `zero-x3.bin` beside the executable. The plugin optionally
loads `x-weapons.bin` there. The development executable is
`build-zero/port-work/MegaManXSNESRecomp.exe`. The owner's preserved swap/HP
playtest is `build-zero/playtest-combat/MegaManXSNESRecomp.exe`; it predates
the new weapon attacks. Do not confuse these builds.

**Never automatically load a save state into the owner's game.** Launch owner
playtests with a fresh boot and the ROM only. Private headless fixtures are
separate and must not be copied into an owner session or release package.

For focused ROM checks, build `mmx_state_tests` with `MMX_STATE_TESTS=ON`.
Run from a scratch directory with absolute `MMX_ZERO_TEST_ASSETS` and
`MMX_ZERO_TEST_FIXTURE` paths and X1 ROM argument. Fixture: complete standing,
unupgraded Highway save with buster selected. Unset `MMX_WEAPONS_TEST_ASSETS`
for the Zero suite; otherwise the harness selects weapon checks. Optional
`MMX_ZERO_SWAP_ONLY` / `MMX_ZERO_HEALTH_ONLY` narrow scope. `MMX_ZERO_TEST_CAPTURE`
sets a capture prefix. A separate `MMX_ZERO_TITLE_FIXTURE` enables title checks.
Use complete `savestate`/`loadstate`, not the older machine-only debugger
`save_state`/`load_state`, for host-state validation. None of this authorizes
loading the owner's game.

Compositor captures replay with `mmx_render_capture`, the X1 ROM, requested
aspect and Zero cache. Native debug screenshots omit replacement art. Capture
tool exit 1 means differences from stock PPU (expected); exit 2 is an error.
Existing test sources: `tests/mmx_zero_test.c`, `tests/mmx_renderer_test.c`,
`tests/mmx_adaptive_state_test.c`; measured references:
[movement](../tests/data/zero_x3_motion.md) and
[combat](../tests/data/zero_x3_combat.md). Their CSVs contain measurements,
not sprites or ROM bytes.

## Validation record, commits and outstanding work

Prior checkpoints passed five CTests, the full ROM-backed Zero suite,
248-frame movement reference and original ground/air combat traces; snapshot,
rollback, rewind and old-state migrations are included. Dedicated probes
exercised a highway wheel enemy and Armored Armadillo through native damage
responses. All eight X1 normal/charged special classes and energy paths were
checked in both firing directions. Menu fade coverage includes 480 opening/
closing frames across unupgraded and upgraded cases. Separate HP and swap
checks include native pickups, life loss/respawn and frozen-projectile replay.
These are focused checks, not an exhaustive campaign certification. This
documentation audit does not pretend to be a new gameplay test run.

| Commit | Durable checkpoint |
| --- | --- |
| `0c5d415` | Playable bounded prototype |
| `da73a88` | Original X3 HUD badge replaces approximate letter |
| `bce2fd7` | Submitted-sprite visibility fixes hurt flashes |
| `4e92e02` | Original animation mirroring and pause body; its life-head change later superseded |
| `dacce26` | Original Zero muzzle offsets for X1 weapons |
| `bdca72b` | Pause fade attribution; custom life-head art later superseded |
| `dee8405` | Weapon effect placement/palette adjustments; custom life-head art later superseded |
| `9cef5fa` | Measured charge tiers and ground/air firing recovery |
| `f8843c6` | Title cursor/shot and looping-charge sound fix; its life icon later superseded |
| `8a50ab0` | Grounded original-art exchange and restoration of X life heads |
| `e26055a` | Independent current HP and active-only healing |

Open Zero acceptance work after the weapon priority:

1. Broader animation/state validation: wall slide/jump, ladders, hurt/death,
   capsules, ride armor, stage teleports and scripted player poses. Do not
   claim all states validated just because every sequence is mapped.
2. Remaining presentation fidelity: X3 buster projectile/effect differences,
   saber/source audio, charge/hit palettes and X1 special effects during
   movement. Keep the owner's accepted X life head.
3. Representative moving platforms, tight spaces, water, doors, bosses and
   native/widescreen campaign playthrough. Alter dimensions only for an
   observed clearance failure.
4. Public source-ROM setup, all-asset packaging audit, mod/save compatibility
   UX and release validation. See the mandatory distribution contract above.

Next implementation remains the twelve unimplemented X2/X3 boss weapons and
the documented polish on Spinning Blade/Acid Burst/Ray Splasher/Sonic Slicer; see the
[weapon notebook](x-weapons-source-notes.md). Co-op is a later, separate,
mutually exclusive mod after weapons are complete, not part of this Zero
documentation checkpoint.

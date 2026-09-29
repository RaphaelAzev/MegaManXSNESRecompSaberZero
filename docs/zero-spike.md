# X3 Zero in X1: bounded feasibility experiment

Implemented on `experiment/x3-zero-spike`, based on X1 commit `975b126`.
Tracking: central Beads `beads-110f`, under the X1 game epic and related to
the X3 game epic. This is an experimental playable prototype, not the finished
full-character port.

**Verdict:** the important integration points work with targeted investigation.
Neither game needed a near-complete disassembly or an engine rewrite. The spike
uses the existing X1 host/compositor and the local X3 project's original USA ROM.
Zero keeps his original dimensions; no resizing was needed in the tested area.
Full-stage clearance remains a playtest task, with changes justified by actual
failures rather than an assumption that he will not fit.

## Run it

Use the normal X1 source-build procedure in the repository README. The CMake
build installs the disabled-by-default **X3 Zero Experiment** package alongside
the existing mods. The package targets the original X1 USA ROM only.

Extract the local asset cache from the original X3 USA ROM:

```powershell
python tools/extract_zero.py ../MegamanX3SNESRecomp/mmx3.sfc build-zero/zero-x3.bin
```

The extractor accepts an optional 512-byte copier header and checks the
normalized X3 SHA-256:
`65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7`.
It extracts 117 body, 21 saber-body and 14 blade poses, palettes, the ground/air
saber bounds, and Zero's original X3 health-bar badge and palette. The current
cache format is `MMXZERO6`; rerun extraction to replace older caches. It also
extracts the 136 original body-animation sequences, including Zero's additional
hair, dash and jump poses, plus the original pose-specific firing offsets.
`--sheet path.png` also writes a labeled contact sheet if Pillow
is installed. ROMs, generated code and extracted graphics stay local.

In the launcher's Mods page, enable **Play as Zero (experimental)**. The asset
picker can point to the cache elsewhere; its default is `zero-x3.bin` beside the
executable. Missing/invalid assets leave Zero inactive and produce a log message.
Enable the existing widescreen mod separately if desired. The native-width view
also uses the compositor while Zero is enabled.

The local spike executable is `build-zero/MegaManXSNESRecomp.exe`. Its development
mod state already enables Zero. It uses that build directory's config and saves.
Use experimental saves separately from normal play: enabled saves contain Zero
state and require the same mod/assets. The package's `requires-same-mods` label
is descriptive; the shared runtime does not enforce save isolation.

Controls use X1's button mappings:

- Dash is immediately available; no equipment is granted.
- With the buster selected, charge tiers occur at 21, 81, 141 and 201 gameplay
  updates. Hold for 201 (~3.35 seconds at 60 Hz), release for the first buster,
  press again after its recovery for the second, then press again after both
  beams/effects clear for the saber. Releasing at 141–200 gives two busters
  without the saber. Ground and aerial swings are supported.
- Switching to a special weapon cancels the stored combo and uses X1's weapon
  logic. Charged specials still require the actual arm upgrade.

## What the spike implements

`src/mmx_zero.c` owns a small character state, asset cache and combat adapter.
The renderer replaces only the player object's pieces and suppresses X1's armor
overlays; story NPC Zero remains a separate actor. Stage scripts, inventory,
boss progression and special-weapon behavior continue through X1.

X3's normal/dash body bounds are translated eight pixels vertically to X1's
player origin, aligning the feet. Terrain and damage bounds retain X3's sizes.
The normal and dash terrain heights are both 21 pixels; their damage half-heights
are 18 and 11 pixels. The live cartridge copy is patched only after matching
the expected X1 bytes, and disabling restores those bytes. Source ROM files
are never modified.

The first two attacks are X1's native full-charge projectile class. A narrowly
scoped second-shot allocation bypasses X1's one-charge-shot restriction while
leaving projectile initialization, travel and retirement to X1. The saber has
X3's body/blade sequences and changing ground/air hitboxes. It uses a transient
projectile slot and X1's collision/damage response. Positive damage becomes 16;
native immunity/reflection paths remain. A target slot can be damaged only once
per swing. This damage value is a prototype balance choice, not a claim that
every X3/X1 enemy interaction is equivalent.

Interpreter hooks support the normal faithful execution path. A checked,
idempotent generated-code patcher covers the same seven capability/combat sites.
The mod adds no framework changes. Optional 12-byte character state is appended
to enabled saves (game chunk v4); disabled saves retain v3. Mid-swing renderer
captures also retain the character state, with old capture v2 still readable.

## Validation

Release build and all five CTests pass. The ROM-backed state test additionally
passes these checks using an unupgraded standing-highway fixture:

| Check | Observed result |
| --- | --- |
| Player replacement | Original-size Zero at 256px native and 342px widescreen; captures inspected |
| Dash | Native dash speed without changing the equipment byte |
| Ground combo | Two full-charge native projectiles coexist, then a timed saber swing |
| Aerial saber | Jump/gravity continue; Zero returns to the same floor height |
| Homing Torpedo | Native weapon selection, projectile creation and energy consumption |
| Charged special gate | Unavailable without arms; five charged Torpedo projectiles with arms |
| Snapshot | Complete immediate mid-saber restore and byte-identical 10-frame replay |
| Rollback | Byte-identical eight-frame replay from a mid-saber rollback state |
| Rewind | The actual rewind path restores the complete mid-saber state |
| Cleanup | Saber/projectile count retires; disabling returns to stock presentation |

Separate live combat probes use copied fixtures, position the player and seed a
stored saber; they do not write enemy HP. In the Armored Armadillo fixture, HP
goes from 8 to 0 at the active arc and X1 enters its boss-death sequence. On the
highway wheel enemy, HP goes from 2 to 1 and X1 enters its shell-break response;
the same swing does not repeatedly hit its exposed core. These prove damage
integration, not a complete boss battle or campaign playthrough.

Asset extraction is deterministic; passing X1 to the X3 extractor is rejected.
The generated hooks were checked for coverage and idempotence. Local evidence
is in `_research/` (ignored): `state-check/run.log`, `boss-proof-final.log`,
`enemy-proof-final.log`, `zero-saber-native.bmp`, `zero-saber-wide.bmp`,
`zero-air-final.bmp`, `zero-dash-final.bmp`, and `zero-torpedo-final.bmp`.

To rerun the ROM-backed checks, build with `-DMMX_STATE_TESTS=ON` and launch
`mmx_state_tests` from an empty scratch directory. Supply absolute paths:

```powershell
$env:MMX_ZERO_TEST_ASSETS = '.../build-zero/zero-x3.bin'
$env:MMX_ZERO_TEST_FIXTURE = '.../build-zero/saves/save0.sav'
$env:MMX_ZERO_TEST_CAPTURE = '.../artifacts/saber.cap' # optional
& '.../build-zero/mmx_state_tests.exe' '.../mmx.sfc'
```

The fixture must be a complete runtime save at a safe standing highway position,
using the buster and no upgrades. The test grants Torpedo inventory and arms only
inside its special-weapon fixtures. Without these environment variables, the
existing state tests run normally. The debugger's `savestate`/`loadstate` commands
use the complete state format; its differently named `save_state`/`load_state`
commands contain only emulated-machine state and cannot validate this mod's
host-side combo state.

Replay a capture with the assets to see the replacement character:

```powershell
build-zero/mmx_render_capture.exe artifacts/saber.cap mmx.sfc 16:9 0 artifacts/saber.bmp build-zero/zero-x3.bin
```

The replay tool returns 1 for differences from the stock PPU, which are expected
for Zero, and 2 for an error. The debug server's plain `screenshot` command captures
the stock PPU before character replacement; use compositor captures or the
presented frame for visual proof.

## Remaining work for the full port

Full-port progress on `feat/x3-zero-port`:

- The original Zero body animation now advances from X3's sequence records,
  alongside X1's existing gameplay animation/events. All 81 X1 sequence entries
  have mappings; X1's Hadouken poses use adapted Zero forward-firing poses.
- The 248-frame [X3 movement reference](../tests/data/zero_x3_motion.md) matches
  position, velocity and animation phase for run, full/short jump, dash and
  dash-jump. X1/X3's 180-byte base movement tables are identical.
- Hurt blinking follows the submitted sprite list, fixing the visibility-epoch
  mismatch that let X's tiles reappear. A renderer regression covers both the
  visible transition and the actual hidden frame.
- The pause menu uses Zero's original standing body, including with all armor
  upgrades owned. Menu and pickup life heads use a new front-facing 16x16 sprite
  shaded with Zero's original palette; vanilla X3 itself retains X's life icon.
  See [the life-head asset](../assets/zero/life-head.png). Native 1-up
  collection increments the life count normally. Menu pixel comparisons change
  only the character and head areas, including the fully upgraded menu.
- Menu fades retain player attribution while X1 repeats OAM without submitting
  a fresh sprite list. Exact matches to live OAM preserve real hidden blinks.
  A 480-frame opening/closing check covers unupgraded and fully upgraded menus;
  both show the same Zero body, including the first visible entry frame.
- Native projectile initializers use Zero's original pose-specific X/Y firing
  offsets from X3 `$39:9161/$39:91D9`, translated to the shared foot position.
  X1 still owns facing, spread patterns, trajectories and weapon effects.
  All eight special weapons produce their expected normal/charged native
  projectile classes and spend energy; both firing directions are checked.
- Saves now use game chunk v6 for Zero's firing phase and stored charge tier,
  retaining v4/v5 saves and older formats. Captures use v5, with v2/v3/v4 still readable. Disabled
  saves remain v3. Complete snapshot, replay, rollback and rewind checks pass.

Owner playtest follow-up: projectile origins and the reported X tiles during
invulnerability are fixed as described above.
These issues are tracked in `beads-8wg.1.29`. Full-port work is on `feat/x3-zero-port`, tracked in
`beads-8wg.1.31`. The earlier hand-drawn red HUD letter was an approximation and
has been replaced with the original X3 Zero badge, including its original frame
and palette. X3's `$84:DCE6` selects DMA list `$5C` at `$86:9AB3`: four tiles from
`$2C:8D20/$2C:8DE0`, with palette `$8C:B0E0`. An independent ROM-to-compositor
comparison matches all 218 nontransparent pixels exactly. The badge remains
visible while Zero blinks; X1's health amount and weapon icon are retained.
The current development executable is `build-zero/port-work/MegaManXSNESRecomp.exe`;
the owner's earlier screenshot session remains in `build-zero/hud-update`.

An optional Select-button X/Zero swap is a longer-term direction, tracked in
`beads-8wg.1.30`. That feature will need an explicit active-character state
separate from loaded assets, with the body, abilities, collision and HUD changing
together. Swapping is not implemented by this HUD update.

1. Audit the complete animation/state mapping: wall slide/jump, ladders, damage,
   death, teleport, capsules, ride armor and scripted player poses. The full-port
   branch maps all 81 native sequence entries to X3's original animations.
   Extracting all 152 poses does not prove that every gameplay state selects
   the correct pose or timing.
2. Match remaining projectile presentation where required. Original charge
   tiers, first/second body-animation events, air/ground phase transitions and
   44/45-frame saber recovery are implemented; see the measured
   [combat reference](../tests/data/zero_x3_combat.md). The first two projectile
   graphics and travel/effect lifetimes remain X1's. Saber readiness follows
   their real retirement, including disappearance effects, without a fixed
   delay. Charge/hit palette effects and saber audio still need adaptation.
3. Adapt and validate all eight X1 weapons in their normal and charged forms,
   including body/arm poses, emission points and weapon palettes. All eight now
   pass normal/charged activation and energy checks; visual effects need playtesting.
4. Play through representative tight spaces, moving platforms, doors, water,
   ride armor and all bosses, then the full campaign in native and widescreen.
   Story/NPC behavior is preserved by the scope of the hooks but has not received
   an end-to-end playthrough with Zero enabled. Tune geometry only where those
   tests demonstrate a problem.
5. Finish mod/save UX, incompatible-mod handling and packaging before release.

This result supports a staged full port. Exact fidelity and campaign acceptance
remain substantial work, but the experiment did not uncover a requirement to
reverse-engineer both games in their entirety.

## Targeted research trail

The investigation was limited to asset tables, player update/input gates,
projectile allocation and damage handling. Useful X3 islands were `$04:A63C`
(body selection), `$04:BCA2` (graphics transfers), `$01:805B` (palettes), and
`$86:B40E/B422/B837/B84B` (body/saber bounds). X1 integration sites are recorded
in `tools/apply_zero_hooks.py` and `src/mods/mmx_zero_plugin.c`.

Public [MMX3 Zero Project](https://github.com/justin3009/MMX3-ZeroProject) and
[MegaED X](https://github.com/rbrummett/megaedx_v1.3) sources supplied format/address
leads. The asset decoder here is independently implemented and verifies the
original ROM; this mod does not require installing the Zero Project ROM hack.

# X1 character, weapon, and co-op roadmap

Owner requirements recorded 2026-09-28. This is the durable scope document;
implementation status and findings are tracked in the central Beads database
at `F:\Software\beads\issues`, under `beads-8wg.1` (Mega Man X / SNES).
Work branch: `feat/x3-zero-port`, worktree: `../_wt_mmx_zero`.

## Order of work

1. Preserve and finish the X3 Zero backport and Select exchange foundation.
2. Finish separate character HP, then implement and validate all X2/X3 boss
   weapons and charged attacks. Weapons are the next major feature.
3. Only after the weapons are complete, build a separate simultaneous co-op mod.

Use clean, bounded commits. Work solo unless the owner explicitly authorizes
agents. Use focused tests for meaningful risks and the existing regression
suite; do not turn this into an exhaustive testing campaign. User playtests
start with a fresh boot: **never automatically load a save state into the
owner's game**. Private test fixtures are separate.

## Existing Zero backport and Select exchange

Tracking: `beads-8wg.1.31` (full port), `beads-8wg.1.30` (exchange, closed).

- Faithfully adapt X3 Zero's original animation, movement, collision bounds,
  ground/air charged-buster combo and third-attack saber to X1. Keep original
  dimensions unless an actual clearance problem calls for adjustment.
- Keep all X1 weapons and charged attacks, with Zero-specific firing origins
  and effects where needed. Charged specials require the actual X1 arm upgrade.
- Use Zero's original X3 health-bar Z badge and original body poses in the
  pause screen and title cursor. Preserve the title selection's actual shot.
- Keep **X's original 1-up head** in both the pause screen and world pickups.
  The custom Zero head was rejected; do not bring it back.
- Select exchanges X and Zero while standing still on solid ground. Use the
  original blue/red stage teleport art, with gameplay tasks frozen throughout.
- Preserve X1 equipment/progression. X retains his native capabilities; Zero
  retains his ported base abilities.

Landed checkpoints:

| Commit | Result |
| --- | --- |
| `9cef5fa` | Original X3 charge tiers, firing and combo recovery timing |
| `f8843c6` | Title cursor/shot and looping charge-audio fix; its custom life head was subsequently removed |
| `8a50ab0` | Grounded Select exchange, original teleport assets, restored original X life heads |

The owner reports that switching looks flawless at first glance. The swap has
focused freeze, state/replay, collision/capability and visual checks. This is
not a claim that every stage and boss has been exhaustively playtested.

## Separate HP in the exchange mod

Tracking: `beads-8wg.1.33`.

Implemented and validated on `feat/x3-zero-port`: native pickups, independent
HP on exchange, shared maximum, native life loss/refill, save/replay and legacy
state migration. Five CTests and the existing ROM-backed Zero regression suite
pass. The HUD uses the active character's existing bar.

- X and Zero have separate **current HP**; changing characters restores that
  character's pool. Damage affects only the active character.
- **Health pickups heal only the active character.** This explicitly supersedes
  the earlier request to heal both pools. Native overflow/subtank behavior
  remains applicable; no inactive-character healing is added.
- Heart tanks raise the shared maximum HP for both characters. This shared
  maximum is separate from the two current-HP pools.
- If the active character dies, lose a life normally. Refill both characters
  on respawn; do not automatically switch to the surviving reserve character.
- Saves, rollback and rewind preserve both pools and the active character.
- Working HUD default: show the active character's health in the existing bar
  and update it on exchange. The question of showing both bars in single-player
  exchange mode is still open; the two-bar co-op layout below is explicit.

## X2 and X3 boss weapons

Tracking: `beads-8wg.1.32`.

Menu/state checkpoint `12c0e1e` implements all three pause pages, original menu
icons, native cursor selection and independent energy/save state. The first
combat checkpoint adds source-based normal/charged Spinning Blade and shared
projectile/collision/save plumbing. The other fifteen attacks, gameplay energy
HUD and pickups remain in progress; this is not a completed weapon release.
See [port findings](x-weapons-port.md) for concrete validation and limitations.

Add all sixteen boss weapons, each with normal and charged behavior, usable by
both X and Zero in X1. Consult the local `../MegamanX2Recomp` and
`../MegamanX3SNESRecomp` projects and original ROM assets/behavior. Zero's base
X3 combat port does not itself supply these boss weapons.

| Source | Weapons |
| --- | --- |
| X2 | Crystal Hunter, Bubble Splash, Silk Shot, Spin Wheel, Sonic Slicer, Strike Chain, Magnet Mine, Speed Burner |
| X3 | Acid Burst, Parasitic Bomb, Triad Thunder, Spinning Blade, Ray Splasher, Gravity Well, Frost Shield, Tornado Fang |

- Unlock the sixteen new weapons immediately. **Charged attacks still require
  X1's arm upgrade**, for either character. Existing X1 weapon progression stays.
- Port actual attack behavior and original visuals to the best practical
  fidelity, including meaningful special effects such as Magnet Mine behavior.
  Adapt X2/X3 X firing poses to Zero where needed.
- Use ordinary enemy damage initially. No new boss-weakness tables or extra
  damage/balance project is included.
- Add X1/X2/X3 pages to the existing pause weapon screen. L/R bumpers cycle
  pages. No instructional UI text; at most left/right caret symbols.
- Include usable selection, charge/release, weapon energy, projectile lifetime,
  terrain/enemy interaction, sound cleanup and persistent state. A menu entry
  or a visual-only projectile does not count as a finished weapon.
- Armor abilities, enhancement chips and secret attacks are outside this pass.
- Commit menu/state infrastructure and completed weapon groups separately;
  validate the important normal/charged behaviors and let the owner playtest.

## Later: separate simultaneous co-op mod

Tracking: `beads-8wg.1.34`, dependent on completing `beads-8wg.1.32`.

Create a second mod that is **mutually exclusive with the Select-exchange
mod**. Share the completed character/weapon implementation where appropriate,
but do not allow both gameplay modes to activate together.

- Add a second controller/player input assignment. Working roster: player 1 is
  X, player 2 is Zero. Both spawn and act simultaneously in every stage.
- Each player has independent movement, combat, collision and current HP.
- HUD order from left to right: **X health, Zero health, then weapon energy
  when applicable**. Move the existing weapon energy display to make room.
- Keep both surviving players on the same screen. A player moving right cannot
  scroll the other player off the left edge; apply the shared-screen constraint
  consistently when movement would separate the pair.
- During forced scene transitions/cutscenes, Zero teleports out using his
  original red stage-exit effect, then returns with the original arrival effect
  when normal gameplay resumes. Preserve native stage/script progression.
- When one player dies, that player remains absent for the rest of the stage.
  They return when the stage ends, or when the other player also dies and both
  restart. Cutscene return must never resurrect an already fallen player.
- Shared maximum-HP progression remains consistent with the character mode.
  Normal health pickups should heal their collector, not the other player
  (working extension of the latest single-player pickup rule).
- Save/replay and mod activation must retain the mode, both players and their
  alive/absent state without contaminating stock or Select-exchange play.

Resolve these details before co-op implementation, after weapons are finished:

- Independent weapon energy/selection and how to display two simultaneous
  special-weapon energy meters within the requested HUD order.
- Pause ownership, subtank use, controller disconnects and input assignment UI.
- Vertical camera constraints and forced scrolling/platform sections.
- Story scripts when X is dead and only Zero survives; how the surviving actor
  drives triggers without reviving the dead partner.
- Shared life-count consumption on a full-team wipe and checkpoint selection.

These are implementation/design questions, not additions to the weapon scope.

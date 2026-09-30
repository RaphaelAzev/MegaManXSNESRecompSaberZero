# X1 character, weapon, and co-op roadmap

Owner requirements recorded 2026-09-28 and updated 2026-09-29. This is the durable scope document;
implementation status and findings are tracked in the central Beads database
at `F:\Software\beads\issues`, under `beads-8wg.1` (Mega Man X / SNES).
Zero 0.0.1 is released separately to main. Weapon branch: `feat/x2-x3-weapons`,
worktree: `../_wt_mmx_zero`. The owner approved merging the current weapon mods
after investigating the reported Penguin canister clipping, then starting couch co-op.
Remaining source-audio and contact-audit refinements are separate follow-ups.

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

Add all sixteen boss weapons, each with normal and charged behavior, usable by
both X and Zero in X1. Consult the local `../MegamanX2Recomp` and
`../MegamanX3SNESRecomp` projects and original ROM assets/behavior. Zero's base
X3 combat port does not itself supply these boss weapons.

The fidelity reference is **X using each weapon in its original game**. Zero
using X1/X2/X3 boss weapons here is an intentional adaptation, not a claim of
native X3 Zero functionality. Preserve source projectile art, attack timing,
relative damage and effects; adapt Zero's poses, muzzle positions and body-driven
actions to fit this mod. Do not use Zero firing boss weapons in X3 as a reference
requirement (owner clarification, 2026-09-29).

| Source | Weapons |
| --- | --- |
| X2 | Crystal Hunter, Bubble Splash, Silk Shot, Spin Wheel, Sonic Slicer, Strike Chain, Magnet Mine, Speed Burner |
| X3 | Acid Burst, Parasitic Bomb, Triad Thunder, Spinning Blade, Ray Splasher, Gravity Well, Frost Shield, Tornado Fang |

- Unlock the sixteen new weapons immediately. **Charged attacks still require
  X1's arm upgrade**, for either character. Existing X1 weapon progression stays.
- Port actual attack behavior and original visuals to the best practical
  fidelity, including meaningful special effects such as Magnet Mine behavior.
  Adapt X2/X3 X firing poses to Zero where needed.
- The first combat pass used X1 buster damage as a placeholder. The owner's
  subsequent explicit goal includes all remaining weapon animations and stats:
  compare source damage and restore each weapon's relative strength, with
  normalization to X1's HP scale as explicitly selected by the owner. The
  subsequent explicit exceptions map Speed Burner to X1 Fire Wave reactions
  and Triad Thunder to Electric Spark reactions, including Armadillo's armor
  break. Other new boss weaknesses remain out of scope. Resolve per-hit, repeated-hit and effect-only behavior
  from source code/runtime; do not count generic buster damage as completed stats.
- Add X1/X2/X3 pages to the existing pause weapon screen. L/R bumpers cycle
  pages. No instructional UI text; at most left/right caret symbols.
- Include usable selection, charge/release, weapon energy, projectile lifetime,
  terrain/enemy interaction, sound cleanup and persistent state. A menu entry
  or a visual-only projectile does not count as a finished weapon.
- Armor abilities, enhancement chips and secret attacks are outside this pass.
- Commit menu/state infrastructure and completed weapon groups separately;
  validate the important normal/charged behaviors and let the owner playtest.

Current checkpoint: all sixteen weapons have normal and charged gameplay
implemented, including source-relative damage and the three utility weapons.
See [port status and remaining fidelity work](x-weapons-port.md) for remaining
refinements. On 2026-09-29 the owner accepted this checkpoint for integration
and authorized co-op next; original SPC sounds and the broader timing audit
remain tracked separately in `beads-8wg.1.46` and `.47`.

## Next: separate simultaneous couch co-op mod

Tracking: `beads-8wg.1.34`, dependent on completing `beads-8wg.1.32`.

Create a second mod that is **mutually exclusive with the Select-exchange
mod**. Share the completed character/weapon implementation where appropriate,
but do not allow both gameplay modes to activate together.

- Add a second controller/player input assignment in recomp-ui. The existing
  SNES profile already supports two ports; first verify whether MMX only needs
  to advertise `num_players=2` before changing shared UI code.
- Player 1 starts alone. A connected/mapped player 2 presses **Select** to join;
  the character not currently on screen joins (X or Zero). Joining enrolls P2
  for the rest of the current game session: automatically spawn both at later
  stage entries and full-team restarts. A fresh session starts with P1 alone.
- Holding **P2 Select for about three seconds** during normal gameplay
  voluntarily despawns P2. Keep their current HP and weapon selection/energy;
  a later Select joins them at safe nearby ground with those same stats.
  Shared subtank contents remain unchanged by joining or withdrawal. Voluntary withdrawal does not clear session enrollment. It must
  never bypass the fallen-player lockout: only the next stage or a team restart
  returns a player who died. This replaces the earlier Start-to-join rule.
- Briefly freeze gameplay during joining and use the character's original
  stage-teleport assets. Find clear ground to the left or right of player 1
  within the current screen. If no safe space fits the incoming body, reject
  the join and play the existing wrong-save/password sound. Do not force the
  character into terrain or relocate player 1.
- Each player has independent movement, combat, collision and current HP.
- Independent weapon selections and weapon energy are confirmed. Each player's
  L/R gameplay cycling stays within that player's selected X1/X2/X3 set.
- HUD order from left to right is **P1 health, P1 weapon, P2 health, P2 weapon**.
  Reserve the weapon-bar column as an empty gap when that player uses the
  buster; do not shift the following bars. This supersedes the earlier HUD order.
- Either present player may pause using Start and operate their own equipment
  menu. Start does not join an absent P2; Select owns join/withdrawal.
- Players can overlap each other; there is no player collision or friendly fire.
- Keep both surviving players on the same screen. A player moving right cannot
  scroll the other player off the left edge; apply the shared-screen constraint
  consistently when movement would separate the pair.
- For a boss door, the character who touches the trigger owns the original
  scripted walk. The other living player teleports out and back near the
  triggering player before the fight. Use the correct blue/red original
  character teleport assets. Apply the same ownership principle to forced
  cutscenes; keep native stage/script progression authoritative.
  Later scope clarification: P1-only door/capsule/script triggers are acceptable
  where this materially reduces complexity; either player is preferred when
  the native trigger can be extended with a small, contained change.
- When one player dies, that player remains absent for the rest of the stage.
  They return when the stage ends, or when the other player also dies and both
  restart. Cutscene return must never resurrect an already fallen player.
- Shared maximum-HP progression remains consistent with the character mode.
  Health and weapon-energy pickups affect only their collector. No inactive
  or partner healing. Fallen players cannot be revived by a scene transition.
- Save/replay and mod activation must retain the mode, both players and their
  alive/absent state without contaminating stock or Select-exchange play.

Confirmed in the owner's up-front replies: co-op remains mutually exclusive
with Select exchange, SELECT switching is disabled, fallen players cannot
rejoin mid-stage, weapon energy/selection are independent, and either player
can pause for their own equipment.

Implementation defaults to document and validate:

- Preserve shared unlocks/progression and a shared team life count; spend one
  life on a full team wipe and restart at the native checkpoint.
- Keep pause input owned by its opener until that player resumes. Subtank use
  heals only the player whose menu is open. **Subtank contents and unlocks are
  shared**, per the later owner correction; this supersedes separate reserves.
  Health pickup overflow from either full-health player fills that shared pool.
  Other upgrades remain shared. Pause safely if an active controller disconnects.
- The owner confirmed a configured P1 character, with the other character
  reserved for P2. The brief fixed-X/Zero answer was explicitly withdrawn;
  either X or Zero must be selectable for P1.
- Vertical camera constraints and forced scrolling/platform sections.
- Story scripts when X is dead and only Zero survives; how the surviving actor
  drives triggers without reviving the dead partner.

Scope is couch co-op first. Consume the engine's two-port simulation inputs and
serialize all player/weapon/join/camera state so future snesrecomp netplay can
reuse the same deterministic simulation. Network compatibility must be tested
later; it is not guaranteed merely by using the existing transport.

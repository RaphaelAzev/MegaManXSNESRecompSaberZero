# Couch co-op implementation notes

Tracking: `beads-8wg.1.34`. Branch: `feat/couch-coop`, worktree
`F:/Projects/snesrecomp/_wt_mmx_coop`. Base: weapon mods merged through PR #53
at `7e2dcc3`. Scope and owner decisions are in
[the roadmap](zero-weapons-coop-roadmap.md#next-separate-simultaneous-couch-co-op-mod).

## Roster and release boundary

P1 chooses X or Zero; P2 receives the counterpart. The brief fixed P1=X/P2=Zero
answer was withdrawn. Character choice belongs to the co-op settings. SELECT
exchange is disabled. Co-op and the single-player exchange package must be
mutually exclusive. The original X3 ROM path is shared with the Zero/X3 weapon
packages; extraction happens internally. No source ROM or extracted art belongs
in Git or the eventual downloadable mod.

Latest join controls: P2 **Select** joins; Start remains pause/menu. Once P2
joins, keep them enrolled for the session and automatically spawn them at
later stage entries/team restarts. Holding P2 Select for approximately 180
gameplay frames voluntarily withdraws P2, retaining HP, energy, selections
and subtanks for a later rejoin. Withdrawal keeps session enrollment. Fallen
players cannot use Select to rejoin until a new stage/team restart. These
rules supersede Start-to-join. The private join/withdrawal checkpoint below
validates these controls; public activation remains pending the other systems.

The initial code is a **development foundation**, not a playable co-op release.
It has no launcher activation package yet. A private ROM-backed test enables
it directly. Do not announce joining, dual rendering, damage, pickups, camera,
death, menus or transitions as complete based on the controller test.

## Source findings (X1 USA, 2026-09-29)

The player controller is `$81:812E`. It saves P and D, sets DP=$0BA8, and begins
its body at `$81:8136`. Its common epilogue is `$81:819C` (PLD, PLP, RTL).
Re-entering at `$81:8136` from that epilogue can run the second player's
controller under the existing guest stack frame. Restore P1's post-controller
registers before the one shared epilogue; preserve elapsed guest cycles.
Do not invent a nested interpreter call without a matching guest return frame.

This runs only the player routine twice. It does **not** run the scheduler,
enemy AI, stage events, DMA, or audio frame twice. Additional ownership passes
are still required for collisions, projectiles, player effects and drawing.
The boundary hooks use the interpreter. `apply_coop_hooks.py` routes generated
entries for this one routine through the existing paired/dispatch bridge when
co-op is enabled, preserving the caller's real JSL frame. Child routines keep
their normal generated implementations. Stock and exchange mode retain their
original path.

| Native location | Ownership and handling |
| --- | --- |
| `$0BA8..0C37` | Player body/controller, 144 bytes; includes current HP, charge and input |
| `$0C38..0C97` | Player armor objects |
| `$0C98..0E17` | Player charge/small effect objects |
| `$0E18..0E67` | Ride armor; stage object, not copied into player context |
| `$0E68..1227` | Enemy pool; world state, not copied |
| `$1228..1427` | Eight 64-byte native player projectiles |
| `$1F83..86` | Four subtanks; low nibble is reserve, high bits shared unlocks |
| `$1F87..96` | Eight X1 weapon energy words; retain shared high unlock bits |
| `$1F99`, `$1F9A` | Shared upgrades and maximum HP |
| `$1F0D`, `$1F12` | Per-player firing command and weapon HUD state |
| `$7E:FFC0..FFC5` | Native configurable action button masks |

In particular, copying eighteen bytes beginning at `$1F85` as “buster plus
weapons” would overwrite two subtanks. Native buster selection has no weapon
energy word. Subtank UI source: `$00:CD16..CD37`; pickup unlock source:
`$81:E643..E65D`.

Native action input conversion is `$00:E543..E5F6`. It copies previous actions
from `$0BDE..DF` to `$0BE0..E1`, maps the buttons through the six configured
masks, and writes new press edges to `$0BE2..E3`. The co-op adapter applies
that mapping to seat 2, preserving native in-game control configuration.
Seat inputs come from `RtlGetPadState`, with the engine's 12-bit button format;
they must not be read directly from SDL. `RtlRunFrame` packs P2 at bit 12.

`$00:D1F3..D206` prepares previous X/Y coordinates and clears the firing
command before entering the player routine. P2 needs the same preparation;
without it, a P1 shot leaves `$1F0D` set and prevents P2 shooting that frame.
The native `$00:D21A` post-controller `$0BD4` clear is also per player.

The second context now runs these native object loops once after P1's pass:

| Pool | Entry | Common return |
| --- | --- | --- |
| Armor | `$00:D2BD` | `$00:D2DD` |
| Player projectiles | `$00:D3DD` | `$00:D3F9` |
| Projectiles while frozen | `$00:D3FA` | `$00:D422` |
| Charge/small effects | `$00:D43A` | `$00:D456` |
| Charge/small effects while frozen | `$00:D457` | `$00:D47F` |

Native `$82:80B4` still determines visibility, but P2 skips its draw-queue
insertion at `$82:80DF`: a second queued pointer to `$0BA8` would display P1
again after projection restores P1. The compositor receives an immutable P2
snapshot instead. Native X sprite arrangements and CHR draw X; extracted X3
poses draw Zero. Co-op suppresses only Zero body/armor CHR transfers at
`$84:8FCB` and returns through the original PLP/RTL, preserving the one X
actor's native dynamic tiles. The renderer keeps original OBJ/BG priorities.

Enemy contact entries are `$84:9B03` (body) and `$84:9B43` (player shots).
They retain the enemy in DP and return through shared RTL boundaries. The
co-op adapter remembers the entry stack/DP, then retries the other player's
context at the balanced return. Projectile scanning retries only after the
native miss at `$84:9B7D`; a hit, immune contact, or reflection at `$84:9EE9`
still consumes that enemy's native first-contact opportunity for the frame.
Body contact checks both players, preserving the caller's successful result
if either overlapped. Enemy AI itself still runs once. Native visibility
`enemy+$0E` must be active: a just-spawned offscreen enemy is intentionally
noninteractive, even when a test moves a player onto it.

Join terrain uses the live collision map already decoded for imported weapons.
At floor dispatch `$84:961C`, `$34..36` and `$3B..3D` call the ordinary solid
handler `$84:96B5` (the highway floor is `$35`). `$33/$3E/$3F` dispatch to
hurt/spike handlers; `$37/$38` are conveyors, and `$39/$3A` are one-way tops.
Joining currently accepts clear ordinary ground/slopes within the native
viewport and conservatively rejects special floors. The failed-password cue
is command `$74`, verified at `$00:F1E4..F1EA`; failed landing uses that cue.
The original morph poses and beam velocities are shared with SELECT exchange.

Player setup requires the native action pointers `+$31=$A597` and
`+$5F=$FA80` plus the constants from `$81:81A3..825A`. Clearing the entire
player tail without restoring those pointers permits movement but disables
firing. New joins clear transient movement/hurt/charge state, preserve personal
inventory on voluntary re-entry, and let native idle initialization resume.

The post-enemy player terrain pass `$81:9D67..9D79` must also run for P2;
it is separate from the movement controller. Camera helpers `$00:DE9D`
(horizontal) and `$00:DEBC` (vertical) retain the original room bounds and
scroll-rate logic, but read the pair's midpoint after each original coordinate
load. Horizontal separation is capped at 224 native pixels. A private check
holds P1 still while P2 reaches the right edge, verifies both remain visible,
then moves both and confirms scrolling resumes. Vertical extremes, forced
scrolling and boss transitions still need further handling.

Native pause entry `$00:9E68` runs against the requesting player's projected
context, including their HP, subtanks and weapon inventory. `$00:9EAC` rejects
an invalid request; `$00:C579` returns after the menu commits its selection.
The owner remains projected during the menu and is serialized in snapshots.
Input mapping `$00:E543..E57F` runs inside the game scheduler, so P2 input must
replace the native P1 mapping at its return, not only before each frame.
Private checks for both rosters verify that P1 cannot change P2's open menu,
P2 can change imported weapon pages/selection, exit returns both controllers,
and saving/replaying the menu produces an identical full snapshot. Menu
captures were visually reviewed. Subtank consumption/pickups remain pending.

The existing shared SNES launcher profile already allows two players.
MMX's desktop-host descriptor omitted `num_players`, so it advertised one.
USA now advertises two; JP remains unchanged. No recomp-ui fork is needed for
the controller assignment cards.

## State and remaining integration

Validated controller checkpoint: private ROM-backed checks pass with scheduler
bounce both disabled and enabled. Each setting checks both roster orders,
P2 walking while P1 stays still, P1 walking while P2 jumps, one world-counter
advance per frame, and byte-exact full snapshot replay with both input streams.
The existing Zero unit test also passes. These checks do not cover the remaining
systems listed below and do not make co-op ready for playtesting.

Second checkpoint: independent buster creation and movement pass for both
roster orders with scheduler bounce on and off. Private captures were visually
reviewed for walking, jumping, and shots with both characters visible. A render
check verifies P2 contributes sprite pixels for either roster; Zero and custom
renderer unit checks pass. Render captures use MMXC v13 when they contain the
co-op snapshot; ordinary captures retain MMXC v12. Enemy damage and imported
weapon presentation are not covered by this checkpoint.

Third checkpoint: in both roster orders, the real highway enemy takes one
native buster hit from P2; contact hurts P2 alone when P1 is elsewhere and
hurts both when both overlap. Each return restores the caller's original
context. These checks exercise the native enemy routine, not a replacement
damage calculation. Pickup ownership and special scripted enemy reactions
still require integration.

Imported-weapon checkpoint: each player can select/fire a different imported
weapon and spends only their own energy. Both source-art projectile/effect
pools render, including P2 X's source weapon palette and adapted casting poses.
The same native enemy has one damage-fraction ledger across attackers; both
serialized player copies synchronize on projection. Frozen/captured enemies
are excluded from ordinary AI/contact for either player's ownership. Both
charged Crystal Hunter effects age every display frame, but share one
half-speed cadence so staggered effects cannot freeze every alternating frame.
Private checks pass for both rosters with generated bounce on/off, and the
Zero/custom-renderer regression checks pass. The coexistence captures were
visually reviewed. This is not an exhaustive two-player audit of all weapons.

Join checkpoint: P2 Start does not join; P2 Select performs the original
arrival while the world counter stays frozen. A 179-frame hold leaves P2
present; frame 180 starts the original departure. Withdrawal/rejoin retain HP,
weapon energy and subtank reserves and never heal P1. Mid-arrival save/replay
is byte-identical. Fallen status rejects Select re-entry. A real native P1
death with P2 already fallen consumes one life, runs the checkpoint restart,
and automatically returns enrolled P2 with both HP pools full. Both rosters
pass these checks. Actual one-player death while the other survives, boss-door
ownership, and later-stage arrival placement still need their own integration.

`MmxCoopPlayer` owns native body/effects/projectiles, per-player weapon energy,
subtank reserves, Zero combat/animation state, imported weapon state, and input.
World progression and unlocks remain in native RAM. Switching the projected
player also updates the existing character collision-table patch.

The game save tail advances to MMXT v14 only when co-op is enabled. Stock,
exchange-only and weapon saves retain their existing v3/v8/v13 layouts. The
tail includes both players and the controller continuation metadata. Legacy
saves reset the co-op context; normal public loading must still enforce the
mod-set compatibility policy. State storage alone does not establish netplay
compatibility.

Remaining integration, in order:

1. Independent native movement and exact save/replay for both roster orders:
   **controller checkpoint passed**, including generated dispatch.
2. Native body, armor, effect and projectile pool passes plus basic dual-body
   drawing: **checkpoint passed**, including basic imported-weapon coexistence,
   shared enemy fractional damage and cross-player time/freeze effects. Audit
   remaining shared native palette/effect resources during playtesting.
3. Check body damage and pickups for either player without duplicating enemy
   AI. Collector alone receives HP/energy; shared unlocks remain shared.
4. Select-to-join, session enrollment, three-second voluntary withdrawal with
   retained stats, original teleports/world freeze: **private checkpoint
   passed**, including automatic checkpoint return. Validate later-stage
   arrivals alongside scene-transition work. No voluntary rejoin after death.
5. Draw both characters with source art and the fixed four-column HUD. Preserve
   native foreground priority; no blanket sprite priority override.
6. Independent pause inventory and subtanks; shared camera and boundaries;
   one-player death, team wipe/checkpoint, boss doors and cutscene ownership.
7. Add the mutually exclusive launcher package and P1 character choice,
   build a playtest executable, and run focused two-controller acceptance.

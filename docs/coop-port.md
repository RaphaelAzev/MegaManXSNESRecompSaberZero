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
gameplay frames voluntarily withdraws P2, retaining HP, energy and selections
for a later rejoin; shared subtanks are unchanged. Withdrawal keeps session enrollment. Fallen
players cannot use Select to rejoin until a new stage/team restart. These
rules supersede Start-to-join.

This branch provides a **development playtest build**, not an end-to-end
campaign certification. The launcher package is `megaman-x.coop` 0.0.1,
disabled by default. Enable Couch co-op, choose P1's character, select your
original X3 USA ROM, and assign both controllers in Controls. P2 receives
the other character. The real launcher provider rejects simultaneous co-op
and exchange activation, propagates the shared X3 ROM path, extracts assets
internally, and restores stock behavior after disabling the mod. Both roster
choices pass activation checks. No source assets are distributed.

Current executable: `build-coop/MegaManXSNESRecomp.exe` in this worktree.
Normal launches start through the launcher; owner playtests must never
implicitly load a save. Private fixture tests below use separate directories.

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
| `$1F83..86` | Four shared subtanks; reserve and unlock bits stay in world RAM |
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
context, including their HP and weapon inventory; subtanks remain shared. `$00:9EAC` rejects
an invalid request; `$00:C579` returns after the menu commits its selection.
The owner remains projected during the menu and is serialized in snapshots.
Input mapping `$00:E543..E57F` runs inside the game scheduler, so P2 input must
replace the native P1 mapping at its return, not only before each frame.
Private checks for both rosters verify that P1 cannot change P2's open menu,
P2 can change imported weapon pages/selection, exit returns both controllers,
and saving/replaying the menu produces an identical full snapshot. Menu
captures were visually reviewed. Subtank consumption/pickups are covered by the later checkpoint below.

The co-op HUD uses fixed columns at native X coordinates 8/24/40/56:
P1 health, P1 weapon, P2 health, P2 weapon. Buster selections leave their
weapon column empty. Native `$00:D82C` / `$00:D94A` supply the overlapping
16-pixel strip placement, partial values and cap position. X keeps X1's badge;
Zero keeps the original X3 badge. Imported weapons use their previously
validated gameplay footers, never pause icons. The native HUD visibility and
optional widescreen edge anchoring remain applicable.

Independent X1 weapon graphics use `$86:98C5`, indexed by `$3E + weapon*2`,
with seven-byte bulk DMA records. Palette directory `$86:8133` is read twice:
list `$40 + weapon*2` with destination offset `$30` for weapon art, and list
`$100 + weapon*2` without an offset for X's body. These privately decoded
resources prevent one player's selection from recoloring the other. In both
rosters, all eight X1 health/weapon meter comparisons matched original
renderer pixels at partial HP/energy. Four-column and buster-gap captures
were visually reviewed, along with an imported weapon pair. Co-op runtime
checks passed with scheduler bounce on/off, plus Zero/renderer regressions.

Native collectible contact `$84:9C0E` retries its original hitbox check for
P2 after P1 misses. Item kinds 1/2/4/5/11 are energy, health, life, Sub Tank and
Heart Tank; other actors in that pool are not treated as pickups. Scheduler
boundaries `$00:D2E6/D308` select the saved collector for an ongoing refill;
`$00:D2ED/D31B` restore the world player after each item. Item movement and
refill tasks still run once. Owner metadata resets when a slot is initialized
or empty, and travels in saves/rollback. Native collision returns at
`$84:9C15/9C1D/9D06` are matched by stack/DP before retrying.

The owner changed subtanks to **shared contents** during this work. `$1F83..86`
now remain in native world RAM and are never projected with a player. Ordinary
HP pickups heal only the collector. Full-health overflow fills the common
tanks; using a tank in either player's menu spends the common contents and
heals that menu's owner. X1's small health pickup stores one native tank unit
at full HP (versus healing two HP when hurt); preserve that original behavior.

Pickup checkpoint: both rosters pass collector-only native HP and imported
weapon-energy collection, with byte-identical save/replay during refill.
Full-health overflow fills shared subtanks without healing the partner. P2's
native pause action consumes the common tank, heals only P2, and returning to
P1 cannot restore spent reserves. These checks pass with generated bounce on
and off. Heart/Sub Tank unlocks retain shared native RAM; collecting those
stage upgrades and native boomerang retrieval still need playtest coverage.

The existing shared SNES launcher profile already allows two players.
MMX's desktop-host descriptor omitted `num_players`, so it advertised one.
USA now advertises two; JP remains unchanged. No recomp-ui fork is needed for
the controller assignment cards.

## Independent deaths and stage ownership

The native controller's death initializer is `$81:8A5C`; it sets global
freeze flags at `$1F13..19`, waits 30 frames at `$81:8A92`, then creates the
original expanding death orbs at `$81:8ADD`. With a living partner, preserve
those shared flags around initialization/countdown and stop the dead actor
at `$81:8B0B`, after the first orb allocation. Mark that seat fallen and clear
its personal attacks. Enemy logic and the survivor continue normally.

The main stage loop at `$00:9AC7` otherwise changes to mode 6 on zero HP.
Mode 6 omits controller polling and pause handling, so suppress that change
when another player is alive. The surviving seat becomes the native world
anchor: enemy/pickup passes, camera, menus and draw submission restore it.
P2 cannot voluntarily withdraw while P1 is fallen, because that would leave
no living player to advance the stage. Select never revives a fallen seat.

When the last survivor dies, retain X1's original stage mode 6, life decrement
and checkpoint. Simultaneous fatalities use one death controller and spend
one life. `$00:9D9E` runs after native actor pools have been cleared: adopt
that cleared body as configured P1 rather than copying an old corpse over
it. Reset personal combat, then let native stage initialization create P1;
enrolled P2 arrives on safe ground with both HP pools full.

Private ROM-backed checks cover either death order and both rosters, continued
movement/shooting/pause, no mid-stage revival, exact survivor-only snapshot
replay, full team restart, and simultaneous fatalities. Generated bounce
on/off both pass the survivor checks; the simultaneous case also passes.
The P2-survivor capture was visually reviewed. These do not yet cover script
ownership at boss doors or a death during a scripted scene.

## Doors and scene transport

The ordinary door contact routines are `$81:E70D` (right-facing) and
`$81:EC98` (left-facing). Try the current world actor first, then the other
living player only after a miss at `$E724` / `$ECC6`. Match the guest stack
and direct-page owner before retrying. A hit at `$E725` / `$ECC7` makes that
seat the world actor, so the retail forced walk, door objects, scrolling,
boss introduction and unlock sequence remain authoritative.

The counterpart uses the original character departure and arrival art/timing.
Only the short teleport animations freeze the world; the native script runs
while the counterpart is hidden. Clear their projectiles and temporary combat
effects, retain HP/inventory, and resume only after the script releases its
body lock and a clear landing is available. Hidden partners cannot collect,
attack, take contact damage or pull the shared camera. Fallen partners never
return from a scene. Other scripts currently follow the world actor (normally
P1); the owner permits this simpler trigger policy for capsules and complex
cutscenes, rather than requiring a second contact implementation everywhere.

`$1F10` is also the boss health HUD state: values 2/4 do not mean the pause
menu is open. Treating all nonzero values as menus prevented the partner from
returning after a boss introduction and stopped imported weapon frame ticks.
The co-op gate now reserves the pause restriction for values >=6.

With generated bounce enabled, the private P2-driven Chill Penguin encounter
exposed an incorrect return from the enemy-projectile loop `$00:D48D`: the
stage resumed at `$80:9B01` with DP=$15E8 and an unbalanced task stack. The
same encounter passes in the interpreter. Co-op now routes this loop through
the existing paired interpreter bridge, as it already does for the duplicated
player/contact boundaries. This change is co-op-only; the shared engine and
stock generated path are unchanged. No synthetic guest return frames are added.

Private door checks drive each seat through both Chill Penguin doors, verify
counterpart departure/return, run subsequent boss combat, compare a snapshot
replay during arrival byte-for-byte, and reject revival of a fallen partner.
They pass with generated bounce on and off. Hidden/returned partner captures
were visually reviewed. Existing controller, weapons, pickup, menu, shared
tank, camera and independent-death checks also still pass in both modes.
A private Storm Eagle capsule approach also checks P1 triggering Dr. Light's
dialogue, the partner leaving, ordinary dialogue advancement, and the partner
returning when control resumes. Capsule interaction follows the current world
actor (normally P1); the second player's touch is not separately retried. This
check does not cover the later upgrade-acquisition animation, every boss door,
or every story scene.

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
pass these checks. Actual one-player death is covered by the later checkpoint above; boss-door
ownership and later-stage arrival placement still need integration.

`MmxCoopPlayer` owns native body/effects/projectiles, per-player weapon energy,
Zero combat/animation state, imported weapon state, and input. Subtank reserves
and unlocks are shared world state, following the owner's later correction.
World progression and unlocks remain in native RAM. Switching the projected
player also updates the existing character collision-table patch.

The game save tail advances to MMXT v14 only when co-op is enabled. Stock,
exchange-only and weapon saves retain their existing v3/v8/v13 layouts. The
tail includes both players and the controller continuation metadata. Legacy
saves reset the co-op context; normal public loading must still enforce the
mod-set compatibility policy. State storage alone does not establish netplay
compatibility.

## Playtest coverage still needed

The focused milestones above cover both roster orders, native/generated
execution, two-player rendering/combat, collector pickups, shared subtanks,
independent pause inventories, horizontal separation/camera, Select enrollment,
withdrawal/rejoin, death/team restart, boss doors, capsule dialogue and launcher
activation. They are bounded checks, not a complete two-player campaign.

Prioritize these during owner playtests:

- Vertical shafts, moving platforms and forced scrolling. Horizontal separation
  is limited to 224 native pixels and the camera follows the pair's midpoint;
  there is no artificial midair support to stop a player's fall.
- Vile/highway ending, fortress story sequences, left-facing doors and scripted
  deaths. Complex scripts currently use one world actor, while doors retry
  either living player. The single surviving player owns the world after a death.
- Ride armor ownership, boomerang-carried pickups, and overlapping native charged
  effects or moving weapon platforms. These share retail world resources and
  have not received a full pairwise audit.
- Physical controller assignment/hotplug and both players using the real pause
  screen. Port 2 is exposed in the shared launcher; automated checks inject both
  input streams but cannot establish physical-controller behavior.

Future netplay integration is separate. Deterministic snapshots include both
players and scene continuation, but this does not certify online compatibility.

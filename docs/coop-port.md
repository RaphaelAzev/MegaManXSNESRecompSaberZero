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

Latest join controls: enabling co-op automatically enrolls P2 and spawns the
counterpart when gameplay has a safe landing. Both players also return at later
stage entries/team restarts. Hold P2 **Select** for 90 gameplay frames (1.5
seconds) to withdraw, retaining HP, weapon energy and selections. P2 stays out
until Select is pressed to rejoin or a new stage/team restart begins. Start
remains pause/menu. Shared subtanks are unchanged, and fallen players cannot
use Select to rejoin until a new stage/team restart.

This branch provides a **development playtest build**, not an end-to-end
campaign certification. The launcher package is `megaman-x.coop` 0.0.1,
disabled by default. Enable Co-op, choose P1's character, select your
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

### Boss damage after a teammate dies

The owner's slot 11 (`save11.sav`) has P1 X fallen and P2 Zero alive, with
Chill Penguin stuck at 29 HP. Penguin's native combat tail at `$81:B6E4..B6ED`
sets its `.30` flag to 1 when `$0BCF & $7F` is zero. This is a permanent
player-dead latch: the shared projectile scan `$84:9B43..9B4F` skips damage
while it is set. It is separate from Penguin's normal `.35` post-hit timer
and damage-row changes at `$81:B63B..B649`.

Previously the co-op world actor changed only after the original 30-frame
death countdown emitted its orbs and marked that seat fallen. Native boss
logic could therefore see zero player HP while the partner remained alive.
World ownership now passes to the living partner at the frame/controller
boundary, and immediately after both native contact passes if contact itself
delivers the fatal hit. The dying actor continues its own controller pass,
death sound and orbs. The last player's death retains the native team restart.

For existing affected saves, a living survivor also clears Penguin's `.30=1`
in its active combat state (kind `$02`, primary state `$04`, positive boss HP).
That flag has no other writer in Penguin's active combat routine. This repair
does not clear its ordinary immunity timer, alter damage rows, or revive a
defeated boss. No save-layout changes or source-ROM modifications are needed.

The private `MMX_COOP_BOSS_SURVIVOR_FIXTURE` regression verifies real damage
from Zero's selected weapon in the reported save, preserves the native
post-hit immunity window, and reconstructs fatal Penguin contact for each
seat. Both cases hand ownership over before the death countdown ends and
finish the native death animation without latching boss immunity. The
existing full co-op suite also passes, including both death orders/rosters,
survivor input and menus, snapshot replay, and one-life team restarts.

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
check covers dialogue only; the full acquisition/demo regression below covers
Chill Penguin's boots capsule. Other capsules and story scenes still need
campaign playtesting.

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

Join checkpoint (updated controls): P2 joins automatically; after withdrawal,
P2 Start does not rejoin and Select performs the original arrival while the
world counter stays frozen. An 89-frame hold leaves P2 present; frame 90
starts the original departure. Withdrawal/rejoin retain HP,
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

## Owner playtest follow-ups: keyboard seats and pit deaths

The desktop host polled both keyboard maps regardless of the launcher's input
source assignment. With identical default maps, assigning P1 to Gamepad and P2
to Keyboard therefore drove both actors. Shared engine commit `b403ec5` reads
only the keyboard-assigned seats and replaces the entire keyboard word each
poll, also releasing held keys after a source change. Controller presence now
uses those same source assignments. The ROM-backed host harness covers source
changes and P2 joining/withdrawing through the real keyboard polling function.
Default Select is **Right Shift**; hold it for 90 gameplay frames (1.5 seconds) to withdraw.
Older saves with a partially completed 180-frame hold still deserialize.

The missing P2 death was reproduced as a **pit fall**, rather than ordinary
enemy damage. Retail's camera bottom clamp at `$00:E11E..E152` checks only the
projected actor. Once it clamps, `$E12D` compares `(player_y - 32)` to the bottom
edge in scratch `$0000`, then deals a lethal `$7F` hit through `$84:9F2F`.
The co-op hook applies the same signed comparison and fatal-hit state to the
other living actor. The normal controller subsequently supplies the death
pose, `$0A` sound, eight original orb objects, and the existing survivor/team
restart handling. `$00:DE40` is an authoritative interpreter boundary so its
embedded bottom-clamp hook also runs with generated dispatch enabled.

Natural-death regression checks start with live players: either a lethal enemy
contact or an airborne player below the floor. Both seats and both rosters
must reach zero HP, emit one sound and eight orbs, and leave the survivor's
world running without spending a life. Orbs are counted at emission because
retail culls a pit death's offscreen objects before the rendered frame.
These checks pass with generated dispatch enabled and disabled. The existing
co-op checks also pass after the fix, including either survivor's final death,
one-life checkpoint restart, both rosters, menus, pickups and withdrawal.

### Highway falling-platform freeze: reproduced and fixed

The owner reproduced the freeze with P1 X already fallen and P2 Zero walking
right while charging on Highway's collapsing platforms. Both read-only captures
had the first item slot at `$1628`, native item kind `$09`, with the platform's
`.2F` countdown just set to `$1E`. The CPU eventually reached the retail panic
loop at `$80:8097`; repeatedly trying to run that damaged task corrupted more
of its stack. Private captures remain under `_research/owner-softlock/` and
`_research/owner-softlock-2/`; none are distributed.

A bounded regression reconstructs the first captured scene's main task record
and stack from a healthy fixture, then rearms that platform's countdown. The
old generated-dispatch build freezes on the very first frame; interpretation
alone succeeds. This is diagnostic fixture setup, not a runtime recovery patch.
The second capture had accumulated additional corruption and could not serve
as this regression: the same reconstruction failed with either execution mode.

The native path is `$82:E777 -> JSR $E9ED`. Ground contact through `$84:9C0E`
sets the 30-frame countdown and calls positional sound `$80:88A2` at `$82:EA27`.
In the failing mixed execution, returning from that helper also interpreted the
caller's continuation and popped its JSR frame. The outer interpreter then ran
`$82:EA2B` a second time, over-popped, and continued in the wrong bank. Excluding
only the sound helper moved the same failure to effect allocation `$82:EA34`
(`$82:82D3`), so the sound itself was not the cause.

The co-op wrapper previously entered the interpreter before the generated
function's prologue consumed an inherited JMP/JML return context. The next
compiled helper could adopt that stale context as its own. `apply_coop_hooks.py`
now inserts the co-op boundary after the normal prologue, uses its inherited
`_entry_s`/`_hrv`, and balances its host stack entry on return. All 23 existing
co-op boundaries use the corrected placement. Native platform code, physics,
and charge behavior are unchanged; the shared engine needs no new patch.

Set `MMX_COOP_PLATFORM_CAPTURE` to the first private frozen snapshot alongside
the ordinary ROM-backed co-op test variables to run the regression. It verifies
the original 30-frame activation, 60 uninterrupted world ticks, the subsequent
falling phase, and restoration of the task's direct page. An explicit panic
hook fails immediately instead of waiting for the interpreter instruction cap.
The platform regression passes with generated dispatch enabled and disabled.
The full existing co-op suite plus focused keyboard routing and natural
enemy/pit-death checks also pass with generated dispatch enabled. The owner
still needs to play through this area and the rest of the campaign; a bounded
regression does not establish complete stage coverage.

### Stage change after finishing Highway as the surviving P2

The owner's slot 05 had native Chill Penguin stage `$1F7A=8`, but co-op's stored
stage remained Highway (`0`). P1's native body already had 16 HP; its stored
status was still `FALLEN`, causing the empty HUD and stale presentation. P2 was
`ABSENT` from native stage reset and never received the automatic arrival.

`MmxCoopFrameTick` included the stage-ID mismatch in its early return. That
made the subsequent stage-adoption block unreachable for every different
stage, including later checkpoint deaths. Stage changes now request pending
initialization independently of the native readiness guard. Once entry is
ready, the existing block adopts the native P1 body, clears fallen/scene state,
refills the partner, and performs the normal safe-ground arrival.

Stage/checkpoint entry also resets P2's native selection (`body+$33`) and
both players' imported selection/pause page to the X1 buster. Previously the
partner refill cleared charge and projectiles but retained the preceding
stage's weapon (reported with Speed Burner after Penguin). Voluntary Select
withdrawal/rejoin and cutscene transport still preserve selections. The stage
fixture and checkpoint regression check this distinction (`beads-8wg.1.51`).

The private `MMX_COOP_STAGE_FIXTURE` check loads the reported slot, verifies
both full HP pools and identities, P2's completed automatic return, then kills
both actors through the native death controller and requires a one-life
checkpoint restart with both players restored. The fix also repairs the
already-saved slot without editing its file or granting HP during ordinary
play. Save layout is unchanged. The accompanying Zero rescue pose adaptation
is documented in `zero-port.md`; `MMX_COOP_DIALOGUE_FIXTURE` exercises 120
idle dialogue frames and saves a private capture for visual review.

### Capsule acquisition and recorded-input demonstration

The owner's slot 8 (the local `save8.sav`, standing on Chill Penguin's capsule)
reproduced a missed scene handoff. The acquisition routine `$87:CCC9..CD23`
sets `$1F48` and deliberately clears the player body lock `$0C16`. The old
co-op trigger required that body lock, so P2 remained beside the capsule.
After the demonstration, shared-camera separation kept the leading actor
tethered to that old position.

Use the native `$1F48` lifetime to begin scene transport and defer the return.
`$87:CDC5` clears `$1F3B` before the demonstration begins; it is too early to
restore P2. `$87:CDED..CE3F` plays the stage-specific recorded input from
`$87:D36B`, with the player's button mapping temporarily saved at `$7F:F008`.
Only the terminator at `$87:CE02..CE1F` clears `$1F48` and restores that mapping.
The existing scene path now hides P2 throughout, leaves native camera/script
control intact, and finds a safe landing beside the actor's final position.
No timeout, new save fields, or changes to the original capsule script are used.

`MMX_COOP_CAPSULE_FULL_FIXTURE` accepts the private pre-acquisition save. The
bounded check runs the original boots grant and dash demo, requires P2 to stay
withdrawn after `$1F3B` clears, verifies HP and both native/imported weapon
reserves survive transport, then moves both actors and checks camera progress.
Hidden-demo and returned-partner captures were visually reviewed. The source
ROM, owner save, and captures stay private and are not distributed.

### P2 X pose/CHR mismatch (2026-10-01)

The owner's netplay test with host/P1 Zero and P2 X exposed garbled frames on
Highway. This also reproduces offline. The partner compositor uses the current
body's sprite arrangement, while native X CHR changes are queued until the
following NMI. In the reproduction, a pose's first frame reads the preceding
pose's tiles; subsequent frames are correct. Sparse still captures missed it.

Native `$84:8FCA` appends eight-byte records to WRAM `$0500`, with byte length
at `$A3`. NMI `$80:8332..8373` transfers those records and clears the length.
Each record contains VMAIN, VRAM word destination, byte count, source address,
and source bank. The renderer now previews pending contiguous OBJ transfers
(`VMAIN=$80`, VRAM `$6000..7FFF`) in a private raster used only for partner X.
ROM and captured `$7E/$7F` source data are supported. The anchor's latched OAM,
world raster, actual guest VRAM/WRAM, and native DMA timing are unchanged.
The preview is derived anew from each immutable frame, including when loading
an existing renderer capture; it adds no save or rollback state.

`MMX_COOP_X_GRAPHICS_TEST=1` alongside the ordinary co-op fixture variables
runs a bounded 96-frame walk/jump/turn/fire regression. An independent reference
decodes the current X pose's five-byte ROM CHR list rather than the pending
queue. The rendered X region must match on every frame, including 28 deferred
pose transitions. It also checks that drawing leaves native VRAM/WRAM intact
and that a captured transition restores with identical pixels. Before/after
walking and jumping captures were visually inspected. Tracking: `beads-8wg.1.59`.


### Thunder Slimer puddles and Mammoth conveyor returns (2026-10-02)

Tracking: `beads-8wg.1.65`; branch `fix/coop-mandrill-miniboss-camera`, based
on main `0f287a2` after the Zero dash-clearance fix. Hunter HQ remains parked.
These changes apply to the shared offline/netplay simulation.

Thunder Slimer's puddle is enemy projectile `$19`, native routine `$83:A8BD`,
in one of eight `$40`-byte slots at `$1428..1627`. `$83:A93D` calls the native
contact test; the co-op retry correctly found P2 but restored the world actor
before subsequent frames. Capture states `$0C/$0E` at `$83:AA6A/AAB5` then
pinned that world actor instead. `$83:AB38` also writes the victim's position.
The reproduction had Zero touched and X pinned (`body+$2C` bit 3).

Each puddle now retains its actual capture seat. Only the capture-state call
at `$83:A934..A939` projects that seat; the world actor is restored before the
ordinary collision/projectile scan. Native escape input, timeout, movement,
and pop animation still run. A successful first contact cannot also capture
the other actor. A lone survivor also records ownership. The formerly reserved
last word of `MmxCoopState` stores eight P2 bits, preserving its 4,648-byte
layout and including ownership in save/rollback state. Older saves load with
the old P1 default; they cannot reconstruct an already-misassigned victim.
The generated-dispatch wrapper additionally covers `$83:A8BD` (24 boundaries).

Mammoth's final door and scrolling intro completed normally, but partner return
rejected solid conveyor classes `$37/$38`. In the private stage-4 replay,
boss `$0C` reached combat state 4 and cleared `$0C16`, while co-op remained in
hidden scene phase 2 for the rest of the run. `MmxCoopFindLanding` now accepts
these solid conveyors through its existing floor-height, headroom, screen and
enemy-clearance checks. It still rejects spikes and transient/one-way floors.
This permits either X or Zero to return without a timeout or script rewrite.

`MMX_COOP_SLIME_TEST=1` with the ordinary private co-op fixture variables checks
both capture seats and lone survivors, continued pin ownership, and identical
snapshot replay. `MMX_COOP_SCENE_FIXTURE` also accepts the private Mammoth
final-door approach: both possible door drivers must finish the native intro,
return a visibly rendered partner on the conveyor and replay arrival exactly.
The original two-door Penguin regression remains available. Fixtures, ROMs,
extracted art and captures stay private.

A separate Storm Eagle checkpoint-2 replay reached native boss `$52` combat
with both players present. No Storm Eagle-specific patch was indicated.
The original report of a camera lock after Slimer's defeat remains open:
a private post-intro native defeat sequence restored the authored camera
limits and allowed both actors to leave. The suspected early-kill path was
ruled out for the native buster: Slimer's `$30` immunity stays set until
`$84:B255`, immediately before the intro unlocks the player at `$84:B25D`.
Camera limits are saved at `$84:AECA..AEE2` to `$7F:D384..D38A` and restored
to the camera targets at `$84:B3DD..B3F6`. No speculative camera bounds or
forced unlock patch is included. A capture of the reported stuck state would
allow that remaining condition to be traced directly.


The owner confirmed both the puddle-targeting and Mammoth partner-return fixes
in offline co-op using the prepared slots on 2026-10-02 and approved shipping
them in `2.0.1`. The separate Slimer camera-lock report above remains
unreproduced; that report is not claimed as fixed by this release.

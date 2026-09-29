# X2/X3 boss-weapon port

Scope and sequencing: [character/weapons/co-op roadmap](zero-weapons-coop-roadmap.md).
Tracking: central Beads `beads-8wg.1.32`, branch `feat/x2-x3-weapons`.
Addressed source findings and remaining uncertainties:
[X1/X2/X3 source notebook](x-weapons-source-notes.md).
Public release contract: [user-supplied source ROMs](mod-source-roms.md).

## Asset extraction foundation

`tools/extract_x_weapons.py` builds a local `MMXWEAP5` cache from both original
USA ROMs. It validates normalized ROM hashes, accepts copier headers, reads
the original sprite layouts and DMA lists, and preserves original palettes.
The source-controlled descriptor contains addresses only; ROMs and extracted
graphics remain local.

```powershell
python tools/extract_x_weapons.py ../MegamanX2Recomp/mmx2.sfc ../MegamanX3SNESRecomp/mmx3.sfc build-zero/port-work/x-weapons.bin
```

The current cache contains 600 projectile/effect poses, sixteen original pause-menu
icons, sixteen gameplay HUD footers and the original animation sequences
(440,617 bytes). This is an asset foundation, not a
claim that the weapons are already playable in X1.

Sources were checked in the local recomp projects using private, paused
reference runs. The owner's playtest was never loaded from these fixtures.
Normal and charged releases were recorded for each native weapon ID. Artwork
samples were rendered directly from the extracted indexed pixels and inspected.

| Native ID | X2 weapon / sprite group | X3 weapon / sprite group |
| --- | --- | --- |
| 1 | Crystal Hunter / `$10` | Acid Burst / `$05` |
| 2 | Bubble Splash / `$44` | Parasitic Bomb / `$06` |
| 3 | Silk Shot / `$48` | Triad Thunder / `$0B` |
| 4 | Spin Wheel / `$46` | Spinning Blade / `$0C` |
| 5 | Sonic Slicer / `$41`, charged `$87` | Ray Splasher / `$0D` |
| 6 | Strike Chain / `$47` | Gravity Well / `$0F` |
| 7 | Magnet Mine / `$0F`, charged `$13` | Frost Shield / `$10` |
| 8 | Speed Burner / `$25`, charged `$26` | Tornado Fang / `$13` |

Both games use the sprite layout root at `$8D:8000`. Weapon-selection graphics
are the original seven-byte bulk DMA records: X2's pointer table is
`$86:9664`, X3's is `$86:97AD`, indexed by `$3E + weapon * 2`. Per-pose graphics
use the six-byte DMA records in bank `$85`. Palette addresses and group DMA
roots are in `tools/data/x_weapon_assets.json`.

Triad Thunder also loads its normal lightning graphics from `$86:9976` when
fired; the ground-wave frames replace that region through their own pose DMA.
Crystal Hunter's inherited frames use its original setup CHR transfer.
Silk Shot currently extracts its original scrap form (poses `$13-$1D` plus the
icon). Its other forms borrow stage graphics and require a separate X1 terrain
adaptation. Tornado Fang includes the 36 frames covered by its player weapon
DMA table; the additional layouts do not use that table.

The binary stores sixteen weapon entries, each with game/weapon IDs, original
X body and weapon palettes, the original 16x16 menu icon and its palette, then
groups of cropped indexed sprite frames.
Every frame retains its signed position relative to the actor origin. Empty
entries retain native pose numbering for explicitly omitted Silk Shot forms.

The menu icons come from each game's compressed graphics resource `$4C`.
Its five-byte record is in X2 `$86:FA01` / X3 `$86:F732`; the extractor decodes
the original literal/backreference format and checks output bounds. It does
not use projectile pose zero as a substitute for menu art. Both menus follow
native weapon IDs: X3 Parasitic Bomb is ID 2 and Frost Shield is ID 7.

## Pause selection and persistence

`mmx_weapons.c` validates and owns local caches, separate selection,
and sixteen energy pools. The launcher exposes independent X2/X3 weapon options
with source ROM pickers and native automatic extraction; see [setup](mod-source-roms.md).
The X3 ROM path is shared with Zero. Either weapon pack also works with X alone.
Disabled packs are skipped when
L/R cycles X1/X2/X3 pages within native pause navigation. The compositor uses
X1's font and energy-bar tiles with the source games' actual menu icons.
No instructional UI text is added. Both X and Zero can select the new entries.

Outside pause, L/R cycles the eight weapons and buster within the selected
game's set. Switching games requires changing the pause page. X1 retains its
native unlock filtering; both shoulders return to the current set's buster.
Selection respects native projectile/cutscene locks and clears charge state.
The buster now retains page 1/2 with weapon 0 in the existing 40-byte state;
new builds accept old saves, but older builds reject this newly valid pairing.
Focused runtime checks cover both characters and both extended pages, all
entries in both directions, held buttons, buster save/load and menu reopening,
and X1 ownership filtering after returning to its page.

Bounded generated/interpreter hooks virtualize the pause inventory reads and
selection. X1 progression/energy stays untouched. An extended selection uses
native buster resources as a safe underlying actor. The current
implementation includes eight combat weapons; remaining entries
are still unfinished.

The game save chunk is version 12 when extended weapons are enabled; legacy
saves initialize full energy without changing X1 inventory. Zero-only saves
remain version 8 and stock saves version 3. Renderer capture version 11 also
stores the displayed weapon page and the active projectile simulation. Existing
older captures still load. Version 9 game states initialize an empty projectile
simulation; their weapon selection and energy remain intact.
Game versions 9/10 and capture versions 8/9 retain their original 24-byte
inventory prefix; new loads initialize the sixteen appended fraction bytes
to zero. New inventory state is 40 bytes and preserves source 8.8 precision.

The eight playable weapons now use source ordinary-enemy damage divided by
their source buster value (3), multiplied by the X1 target's buster damage.
Fractions carry between contacts to preserve ratios, including weak individual
pellets and strong sustained weapons. Special encounter/armored profiles keep
neutral buster damage; native immunity remains unchanged. No boss weaknesses
are added. See the source notebook for the exact per-phase values. Combat state
is 388 bytes; v10/v11 game saves and v9/v10 captures migrate their old 328-byte
projectile prefix with empty damage carry. Earlier checkpoint references to
one-HP placeholder damage below describe the original implementation only.

Focused ROM-backed checks exercise all sixteen menu choices while X1 weapons
are locked, forward/backward page cycling, X/Zero selection, native cleanup,
partial-energy save/load and deterministic menu replay. Original icon renders
were inspected for both pages; the five existing CTests pass.

## First combat checkpoint: Spinning Blade

`mmx_weapon_combat.c` integrates host-side attack movement and original source
animation records with X1's existing projectile allocation, collision, damage,
firing poses, charge effects and sound cleanup. The projectile state is saved
alongside the guest RAM, including mid-flight animation and charged rotation.
Generated-code and interpreter hooks apply the same behavior.

The first supported attack is X3 Spinning Blade. Its twin normal blades follow
the measured deceleration/vertical separation and original animation. Normal
enemy contact uses X1's positive buster damage and its existing immunity rules,
then plays the original blade impact. The charged blade extends to 80 pixels
from the muzzle, rotates when commanded, and retracts. Its tether and muzzle
use the original X3 sprite poses. One normal pair costs one energy; a charged
release costs three, as measured in the original X3 runtime. X1's actual arm
upgrade bit ($1F99 & $02) is required. The charging press can also emit an
ordinary pair, as it does in the source game.

The cache additionally stores original animation directories and records from
X2 root $2F:A000 and X3 root $3F:8000: duration, flags, pose and relative loops.
MMXWEAP4 is 433,323 bytes for the current descriptor. Only addresses and the
extractor are committed, never the ROM or extracted art.

Focused ROM-backed checks cover both characters, twin-shot creation, normal
and charged energy use, arm gating, charge-audio cleanup, exact save/replay
mid-flight and mid-turn, and a real highway enemy losing one HP through the
native collision routine. Original normal/charged art is captured for review;
extended and rotating tether renders were inspected. The full existing Zero
ROM regression, all sixteen menu-choice checks and five CTests pass after the
combat integration. The desktop game and capture tools build successfully.

This is a combat foundation checkpoint, not completion of the weapon set.
Spinning Blade's integration still needs final attention to transitions,
special terrain/reflection cases and fidelity during movement. Source X1
charge/firing sounds are reused; X2/X3 sound-bank import is not implemented.

## Energy integration

Extended selections use X1's vertical energy segments and the selected
weapon's **original X2/X3 gameplay footer**, extracted independently of the
pause icon. Native X2 tile $28 and X3 tile $AC are 16x16 sprites loaded by each
weapon's bulk DMA list. Their source pixels, frame, transparency and weapon
palette are used unchanged, with no crop, scaling or borrowed X1 frame.
The earlier cropped-menu approximation was rejected and removed.
All sixteen rendered footers were compared pixel for pixel against private
original-game screenshots; the corresponding DMA bytes and palettes also match
live source VRAM/CGRAM. Pause icons remain unchanged. Native/Python extraction
parity and focused menu/energy runtime checks pass. MMXWEAP4 adds these 256
indexed pixels to each entry; automatic ROM extraction regenerates old caches.
Gameplay save/capture layouts are unchanged. HUD inventory reads are virtualized;
they never change the X1 inventory.

The native weapon-energy pickup actor (item kind 1) retains its collection
collision, gameplay freeze, incremental refill and sounds. Small pickups add
two energy and large pickups add eight to the selected extended weapon. After
the selected weapon fills, the original auto-refill scan gets first choice of
owned X1 weapons; its remainder can fill X2/X3 weapons in inventory order.
Picking up energy while using the buster can also refill new inventory.

Checkpoint death preserves weapon energy, matching measured X1 behavior. The
native full-inventory refill entry $00:9EF9 also refills all sixteen additions.
This is separate from the two character HP pools, which refill on respawn.
Focused runtime checks cover native small/large pickups, overflow, buster
auto-refill, unchanged X1 unlocks/inventory, save/replay during animated refill,
and matching native/extended energy retention through actual death/respawn.

Fractional costs are preserved internally to 1/256 energy, matching the source
games' representation; native HUD/menu bars display whole units. Native pickup
reads/writes preserve the low byte. X1's selected-weapon refill increments one
whole unit at a time and clamps the final increment, discarding any fractional
excess from that increment; only remaining pickup ticks enter auto-refill.
The port preserves that behavior and other weapons' existing fractions.
Focused checks cover half-unit refill, reserve quarter-unit preservation,
native clamping/overflow, exact replay during refill, fractions across pause
selection, v10 save migration and an original v9 renderer capture.

## Second combat checkpoint: Acid Burst

Normal Acid Burst now lobs from either character's muzzle, accepts up/down
aiming, splashes on X1's live tile terrain, and emits the original four
droplets. Charged release creates the two original growing blobs, which bounce
and leave splashes for up to five terrain contacts. Original animation records
and art come from the user's X3 cache. Costs are one normal / two charged;
real X1 arms still gate charging. Enemy hits retain ordinary X1 damage.

Shared combat dispatch now supports both Blade and Acid. Collision boxes vary
by weapon/phase. The new terrain helper reads X1's live metatile properties,
including half/quarter slopes and one-way ladder tops. It sweeps movement to
avoid skipping thin tiles. No player-only hazard side effects are invoked.
The existing save/capture layouts are unchanged.

Focused checks pass for both characters: normal/charged creation and energy,
native highway floor contact, four droplets, charged bounces/splashes, slot
cleanup, charge-audio cleanup, exact save/replay through terrain interaction,
and ordinary damage/impact retirement against an actual highway enemy. Normal
and charged source-art renders were inspected; the existing Blade combat
regression passes and the desktop/capture tools build.

Remaining Acid fidelity work: underwater dissolution, moving-platform contact,
source sound import, and representative non-flat terrain playtests. Addresses
and measured behaviors are in the [source notebook](x-weapons-source-notes.md).

## Third combat checkpoint: Ray Splasher

Ray Splasher now emits seven spread rays over its normal burst, using the
original muzzle animation and ray/trail poses. The muzzle follows the active
character, and the firing body pose now stays active for the complete 60-frame
burst, matching the original X3 reference. Zero retains his original extended
arm pose instead of returning to idle while the muzzle keeps emitting.
The charged attack deploys the original floating turret, launches upward and
cycles the source sixteen-direction pattern for twenty-two shots over 180
active frames. Both characters pay one energy normally and exactly 2.5 for
charged release; child rays do not consume additional energy.

Ray projectiles retain ordinary native damage; their original trails drain
after impact. Focused checks cover both characters, shot counts/directions,
fractional cost, charge sound cleanup, native enemy damage/retirement, complete
slot cleanup and exact save/replay from deployment through radial firing.
Normal and turret renders were inspected. Blade's regression passes, including
a fix for an old adapter mistake: RAM `$0C25` is the native charged-beam count,
not a firing-pose timer. The adapter no longer pins it to two while the charged
blade is active, avoiding a leak into later attacks. Desktop/tools build.

Remaining Ray fidelity work: full character deployment/firing body poses and
their movement/air transitions, turret destruction effects/interactions,
source audio, and visual comparison of trails at contact/offscreen edges.
The current deployment uses a 36-frame wait measured from the reference;
the original waits on its body-animation event. This is a practical adapter,
not a claim that the source body-action state machine has been fully ported.

## Fourth combat checkpoint: Sonic Slicer

The normal attack forms at the muzzle, launches two accelerating arcs and
ricochets from live X1 tile terrain. The third vertical contact retires a blade.
It costs exactly half an energy unit. Charged release uses X2's separate `$87`
sprite group and creates five blades, spreading upward, stopping horizontal
travel at their apex, then falling through terrain. It costs two energy and
requires X1's arms. Native ordinary enemy damage and original impact art apply.

Focused checks pass for both characters: split timing, fractional costs and
insufficient energy, arm gating, native floor bounce/retirement, five charged
arcs and their apex, charge sound/slot cleanup, ordinary native enemy contact,
and exact save/replay. Original normal and charged captures were inspected;
desktop and capture tools build. Original normal trails, cosmetic wall sparks,
source sounds and broader movement/terrain comparison remain fidelity work.

## Fifth combat checkpoint: Spin Wheel

The normal wheel uses X2's original forming/spinning/shrinking animations,
falls onto live X1 terrain, waits 30 frames and rolls with source momentum
decay. Its speed responds to slope direction; airborne wall reflection loses
momentum, and a ground wall starts the original delay/hop behavior. Enemy
contact deals ordinary X1 damage, briefly stops the wheel and reduces momentum
before it resumes. Source ground-effect poses follow the wheel on the floor.
Normal fire costs one energy; charged release costs three and requires arms.

Charged formation emits the original eight radial projectiles with source
positions, velocities and directional art. X1 has eight projectile slots, so
the center's slot becomes one projectile while saved state renders its brief
original center flash. All eight directions fit when the pool is free. These
charged projectiles pass through terrain, as in X2. No cache or state size
change is required; all 27 poses were already extracted from the user's ROM.

Focused checks cover both characters, forming/falling/ground delay/rolling,
costs and insufficient energy, arms gate, eight charged directions, charge
audio and slot cleanup, deterministic replay, and native enemy damage/pause.
Normal and charged original-art captures were inspected. Broader slope/wall
and moving-platform comparisons, source sounds and X2-specific destructible
terrain remain fidelity work; X1 has no equivalent source-only breakable blocks.

## Sixth combat checkpoint: Frost Shield

Normal fire uses the original X3 ice formation, accelerating rocket, falling
core and planted spike. Air/water variants use source animation records,
acceleration, steering, gravity and hitboxes. Charged fire forms the original
arm shield, holds for 360 ticks, blocks destructible enemy shots, then releases
the moving ice chunk. Killing contact preserves the charged shield; contact
with a surviving enemy breaks it, as in X3. Normal/charged costs are 1/3.
Damage is 15/3 buster strength, with 9/3 for the released charged chunk.

Underwater charging creates the original rising platform. A bounded native
terrain hook lets either character stand on it and jump off; it floats for
240 ticks at the waterline. Ice shards use the original particle art and
velocity tables. All 63 poses were already in the source-ROM cache, so no
asset format or save size changes were needed. Landing is X1 action `$0A`,
not hurt (`$0E`); both Frost Shield and Ray Splasher now preserve their firing
poses across ordinary landings.

Focused checks cover both characters, normal terrain phases, arms gating,
energy, charged contact responses and projectile blocking, shield expiry,
counter/audio cleanup, and deterministic replay. A private controlled water
fixture exercises native landing, riding, jump-off and save/replay. Full water
stage/camera traversal still needs playtesting. Source sounds, source RNG
ordering for cosmetic shards and interactions with moving stage actors remain
fidelity work. Cosmetic shards share X1's finite projectile slots.

## Seventh combat checkpoint: Bubble Splash and full-charge detection

Normal fire reproduces X2's two-tick held-fire cadence, seven-bubble limit,
one-eighth energy cost, varied growth/pop sequences, initial speed range and
upward acceleration. Water increases acceleration. Original bubbles pass
through terrain; enemy contact plays the original popping animation. Normal
damage is 2/3 of a buster hit, with fractional carry across contacts.

Charged release creates the renewing seven-bubble cloud. Each bubble follows
X2's 29-record movement path, with source delays, growth art and parent motion.
Activation and each replacement cost one-eighth energy; charged contact is
5/3 buster damage. The source three-pixel underwater lift is included with
an X1 ceiling check. Shoulder cycling cancels the persistent cloud. Original
32-pose art is already extracted; no new asset or save format is needed.

This exposed a shared charge bug: buster class 3 represents the intermediate
arm-upgraded beam, not the full special-weapon release. Imported attacks now
use X1's own full-release marker `$C01 == 4`, gated by arms. Holding beyond
full charge therefore retains the charged attack. Earlier 150-tick test
releases exercised the wrong tier; charged checks now hold for 205+ ticks,
and Bubble Splash also checks that an intermediate release stays normal.

Focused checks exercise both characters, continuous fire/energy, growth/pop,
full/intermediate charge and arm gate, renewing cloud/drain, native enemy
contact, private water lift, shoulder cancellation, empty-energy cleanup and
exact save/replay. Original-art captures were inspected. RNG is deterministic
from port state using the source arithmetic, rather than the source game's
whole RNG call stream. Source audio and full water-stage traversal remain
fidelity work.

## Eighth combat checkpoint: Magnet Mine

Normal mines travel at two pixels per tick, steer with source acceleration
and retain vertical momentum after releasing up/down. Terrain or another mine
starts the original arming animation and 60-tick planted wait. Native enemy
contact or timeout starts X2's original shared explosion; blast completion
detonates nearby mines. One flying/arming mine is allowed; planting frees
the firing limit while retaining its physical slot. Normal cost is one energy.

Charged mines travel at half a pixel per tick and steer the same way. They
pull destructible enemy projectiles five pixels horizontally and three
vertically each tick, absorb contacting shots on alternating ticks, and grow
at 16/32 absorptions through the three original animations and 8/16/24-radius
hitboxes. Native immunity is respected. Charged cost is three energy; both
forms deal 5/3 buster damage with source-style repeated explosion/charged
contacts. X2's separate armor energy-conversion bonus is outside this pass.

Both-character checks cover speed/steering, source firing limit, native terrain
planting, timed explosion, chain reaction, arms/costs, projectile attraction,
immunity/absorption, both growth thresholds, native enemy damage, save/replay
and cleanup. Normal blast and charged art were inspected. Original blast CHR
and its separate palette are extracted from the user's X2 ROM (MMXWEAP5),
not recolored weapon sprites. Moving-platform attachment and source audio
remain fidelity work.

## Remaining implementation

Implement the other eight weapons' normal and charged attacks, native
sound/effect cleanup, X/Zero firing origins, terrain/enemy interaction and
meaningful special behaviors. Gate charging on X1's arm upgrade. Keep ordinary
damage and existing X1 progression. Co-op remains a later, separate mod. The
owner's running playtest is the weapons follow-up build; private fixtures must
never be loaded into it.


## Branch boundary after Zero 0.0.1

Zero 0.0.1 is released independently (PR #52). This branch contains the entire
X2/X3 weapon follow-up; do not merge unfinished weapons with the Zero release.
The shared source-ROM provider uses the same framework pin as Zero. The Zero
release notes describe only its v8 save / v7 capture layout. With weapons here,
game chunks use v12 (40-byte inventory plus 388-byte combat state), captures
use v11. Older v9/v10 game and v8/v9 capture inventories migrate their 24-byte
prefix; game v10/v11 and capture v9/v10 combat prefixes initialize empty damage carry.
The full 40-byte Zero state is unchanged. Do not load weapon-branch saves in
the Zero-only release. No co-op implementation belongs in this branch.

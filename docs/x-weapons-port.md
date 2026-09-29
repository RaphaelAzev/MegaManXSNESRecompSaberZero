# X2/X3 boss-weapon port

Scope and sequencing: [character/weapons/co-op roadmap](zero-weapons-coop-roadmap.md).
Tracking: central Beads `beads-8wg.1.32`, branch `feat/x3-zero-port`.
Addressed source findings and remaining uncertainties:
[X1/X2/X3 source notebook](x-weapons-source-notes.md).
Public release contract: [user-supplied source ROMs](mod-source-roms.md).

## Asset extraction foundation

`tools/extract_x_weapons.py` builds a local `MMXWEAP3` cache from both original
USA ROMs. It validates normalized ROM hashes, accepts copier headers, reads
the original sprite layouts and DMA lists, and preserves original palettes.
The source-controlled descriptor contains addresses only; ROMs and extracted
graphics remain local.

```powershell
python tools/extract_x_weapons.py ../MegamanX2Recomp/mmx2.sfc ../MegamanX3SNESRecomp/mmx3.sfc build-zero/port-work/x-weapons.bin
```

The current cache contains 591 projectile poses, sixteen original pause-menu
icons and the original animation sequences (429,227 bytes). This is an asset foundation, not a
claim that the weapons are already playable in X1.

Sources were checked in the local recomp projects using private, paused
reference runs. The owner's playtest was never loaded from these fixtures.
Normal and charged releases were recorded for each native weapon ID. Artwork
samples were rendered directly from the extracted indexed pixels and inspected.

| Native ID | X2 weapon / sprite group | X3 weapon / sprite group |
| --- | --- | --- |
| 1 | Crystal Hunter / `$10` | Acid Burst / `$05` |
| 2 | Bubble Splash / `$44` | Frost Shield / `$06` |
| 3 | Silk Shot / `$48` | Triad Thunder / `$0B` |
| 4 | Spin Wheel / `$46` | Spinning Blade / `$0C` |
| 5 | Sonic Slicer / `$41`, charged `$87` | Ray Splasher / `$0D` |
| 6 | Strike Chain / `$47` | Gravity Well / `$0F` |
| 7 | Magnet Mine / `$0F`, charged `$13` | Parasitic Bomb / `$10` |
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
not use projectile pose zero as a substitute for menu art. X3's menu order is
mapped back to its actual weapon IDs (Frost Shield is ID 2, Parasitic Bomb 7).

## Pause selection and persistence

`mmx_weapons.c` validates and owns the optional local cache, separate selection,
and sixteen energy pools. While the cache is loaded beside the executable,
L/R cycles X1/X2/X3 pages within native pause navigation. The compositor uses
X1's font and energy-bar tiles with the source games' actual menu icons.
No instructional UI text is added. Both X and Zero can select the new entries.

Bounded generated/interpreter hooks virtualize the pause inventory reads and
selection. X1 progression/energy stays untouched. An extended selection uses
native buster resources as a safe underlying actor. The first combat checkpoint
below is implemented, but this is not yet a weapon playtest build. The owner's current
playtest has not been replaced with this intermediate implementation.

The game save chunk is version 11 when extended weapons are enabled; legacy
saves initialize full energy without changing X1 inventory. Zero-only saves
remain version 8 and stock saves version 3. Renderer capture version 10 also
stores the displayed weapon page and the active projectile simulation. Existing
older captures still load. Version 9 game states initialize an empty projectile
simulation; their weapon selection and energy remain intact.
Game versions 9/10 and capture versions 8/9 retain their original 24-byte
inventory prefix; new loads initialize the sixteen appended fraction bytes
to zero. New inventory state is 40 bytes and preserves source 8.8 precision.

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
MMXWEAP3 is 429,227 bytes for the current descriptor. Only addresses and the
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

Extended selections use X1's original vertical weapon-energy bar. A cropped,
unscaled glyph from the original source pause icon identifies the new weapon.
HUD inventory reads are virtualized; they never change the X1 inventory.

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
character, preserving its firing offset after X1's brief firing overlay ends.
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

## Remaining implementation

Implement the other twelve weapons' normal and charged attacks, native
sound/effect cleanup, X/Zero firing origins, terrain/enemy interaction and
meaningful special behaviors. Gate charging on X1's arm upgrade. Keep ordinary
damage and existing X1 progression. Co-op remains a later, separate mod. The
owner's running playtest remains the stable exchange/HP build.

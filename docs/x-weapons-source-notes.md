# X1/X2/X3 weapon source reference

This is the durable investigation notebook for the weapon backport. Update it
when a source address, runtime observation or mapping is established. Scope:
[roadmap](zero-weapons-coop-roadmap.md); implementation/checkpoints:
[port notes](x-weapons-port.md). Work is tracked in central `beads-8wg.1.32`.

Addresses below are SNES CPU LoROM addresses unless explicitly marked RAM or
normalized ROM offset. Sources are the local original USA ROMs in the X1, X2
and X3 recomp projects. ROM hashes are checked by
[`x_weapon_assets.json`](../tools/data/x_weapon_assets.json). Do not commit ROMs,
extracted art or Ghidra databases. Reference sessions use private fixtures,
never the owner's running game.

## X1 integration map

| Address / RAM | Established purpose and port use |
| --- | --- |
| RAM `$0BA8` | Player object; X position `+$05`, Y `+$08`, facing `+$11` bit `$40` means right |
| RAM `$0BDB` | Native selected weapon, even ID; extended weapons retain native buster resources |
| RAM `$0BDD` | Active player projectile count; every owned allocation/retirement must balance it |
| RAM `$1228..$1427`, stride `$40` | Eight native projectile slots; port ownership tag `$5758` at `+3E` |
| RAM `$0E68..$1227`, stride `$40` | Ordinary enemy slots; enemy current HP `+27`, collision box pointer `+20` |
| RAM `$1F99` bit `$02` | Actual arms upgrade; required for charged X2/X3 weapons |
| RAM `$1F99` bit `$08` | Dash upgrade, NOT arms; Zero's innate dash must not enable charged specials |
| RAM `$1F88 + weapon_index*2` | Native X1 unlock/energy byte; extended inventory must not modify it |
| RAM `$1F12` | Native weapon-energy HUD state; zero rebuilds after extended selection changes |
| `$81:815C`, `$81:8165` | Player tick entry/end; Zero and extended combat input handling |
| `$81:9D47` | Newly allocated buster object available in X; mark extended shots before native effect dispatch |
| `$00:D3E5` / interpreted `$00:D3E7` | Projectile-active read; owned host simulation replaces native class update |
| `$84:9C16` / interpreted `$84:9C19` | Enemy/projectile hitbox read; suppress duplicate contact as appropriate |
| `$84:9E6E..9E76` | Native positive-damage subtraction; use original buster damage and existing immunity rules |
| `$81:A578..A5B5` | Native buster muzzle coordinates, facing and player-position addition |
| `$82:8174..8235` | Original fixed-point acceleration/movement; source vertical velocity is positive UP |
| `$00:C67F` | Pause menu input: L/R page cycling |
| RAM `$1EC8` | Native pause direct-page base; cursor `+0A` = `$1ED2`, candidate `+0B` |
| `$00:D907..D9F9` | Native weapon HUD setup/draw, using virtual selection/energy reads |
| `$81:E042..E103` | Collected weapon-energy item refill: freeze, four-frame increments, native sounds/cleanup |
| `$81:E11A..E169` | Auto-refill scan of owned X1 weapons; X reaches `$12` after exhausting the scan; RAM `$0000` holds remaining 8.8 energy |
| `$00:9EF9..9F0E` | Native full-inventory refill; extend to all 16 added weapons |
| Normalized ROM `$37F80..37F9F` | Verified unused bytes used for eight owned projectile collision boxes; separate from Zero's `$37FB0` saber boxes |

Item pool fixture: RAM `$1628`, stride `$30`. Kind `+0A=1` is weapon energy;
kind 2 is health. `+0B & 127` distinguishes small (1) from large (0), and bit
7 prevents fixture expiration. Kind 3 is NOT weapon energy. A wrong item kind
is an invalid test fixture, not evidence about the energy hooks.

Checkpoint death preserves native weapon energy. This was checked against an
owned X1 weapon at five energy while an extended weapon retained twelve.
Health refill is deliberately separate: both character HP pools refill on
respawn; ordinary HP pickups heal only the active character.

## X2/X3 source assets and runtime mapping

| Source fact | X2 | X3 |
| --- | --- | --- |
| Player object in measured source fixtures | RAM `$09D8` | RAM `$09D8` |
| Source projectile pool | RAM `$10D8..$1317`, stride `$40` | Same |
| First weapon energy pair | RAM `$1FBA` | RAM `$1FBB` |
| Arms upgrade byte | RAM `$1FD0`, bit 2 | RAM `$1FD1`, bit 2 |
| Sprite-layout root | `$8D:8000` | `$8D:8000` |
| Animation-group directory root | `$2F:A000` | `$3F:8000` |
| Static weapon-selection DMA root | `$86:9664` | `$86:97AD` |
| Menu compressed-resource table | `$86:FA01` | `$86:F732` |
| Resource `$4C` source | `$22:8FB9` | `$21:E7E4` |
| Menu icon palette, normalized ROM offset | `$2CEE0` | `$62DA0` |

Weapon-selection DMA is indexed by `$3E + weapon_id*2`. The descriptor records
each per-pose DMA table, group and palette. Animation directories contain
16-bit offsets into records `(duration, flags, pose)`; flag `$80` adds a signed
16-bit relative loop displacement from the following offset. The extractor
walks all sequences, validates bounds, and stores these source records.

Native IDs X2: 1 Crystal Hunter, 2 Bubble Splash, 3 Silk Shot, 4 Spin Wheel,
5 Sonic Slicer, 6 Strike Chain, 7 Magnet Mine, 8 Speed Burner.
Native IDs X3: 1 Acid Burst, 2 Parasitic Bomb, 3 Triad Thunder, 4 Spinning Blade,
5 Ray Splasher, 6 Gravity Well, 7 Frost Shield, 8 Tornado Fang. Pause menu
order follows these IDs. Earlier notes incorrectly swapped names 2/7 and
compensated by swapping their menu icons, masking the mismatch with the source
projectile data. Original menu capture and handlers `$81:970C` / `$81:AC82`
confirm ID 2/group `$06` is the parasite bomb and its charged seeking bits;
ID 7/group `$10` is the ice missile and charged shield. Names and icon selection
are corrected in both native and Python extractors. Data groups/palettes did
not need remapping. The five implemented combat weapons use other IDs.

## X3 Spinning Blade observations

Normal actor class `$0A`, group `$0C`: initial horizontal speed `$0400`,
deceleration `$20` per frame toward the return direction. Twin vertical speeds
separate by `$10` per frame and clamp at magnitude `$C0`. Normal damage bounds
are `(0,0,13,10)`. A normal pair costs one energy; repeated presses while the
pair is active do not create more pairs in the source runtime.

Charged actor class `$13`, same group: extends 80 pixels from the muzzle in
four-pixel steps, can make a full 64-step orbit, then retracts. Normal stationary
source muzzle measured at player `(14,-4)`, blade fully extended at `(94,-4)`.
Idle extension times out around frame 141 and retracts; exact transition/control
timing still needs a final fidelity pass. A charged release costs three energy.
The initial press used to begin charging can separately emit a one-energy pair.

Animation sequences: 0 normal spin; 1 impact; 4 charged spin; 5 tether extension;
6 tether retraction; 7 muzzle; 8/9 tether rotations. Tether pose 66 points left
in unflipped source art, 42 up, 50 right, 58 down. Actor facing flips matter:
using a world-angle pose and then flipping it again reverses the tether.

## Limits of the current reference traces

The original 100-frame normal/charged recordings are discovery material, not
proof of every charged form. The X2 fixture is near a wall. Some charged forms
have eligibility constraints or activate while held. In particular, Sonic
Slicer's charged discovery trace was superseded by the verified trace below;
Frost Shield, Gravity Well and Tornado Fang still require follow-up source
checks before implementing from those initial traces. Keep verified facts
separate from inferred behavior; add new findings below as weapons are ported.

## X1 terrain mapping for ported projectiles

`$84:90B3..9110` samples the object's terrain offsets and resolves the collision
class. `$84:916A..91AC` reads the live screen map at RAM `$E800`, indexed by
`(y >> 8)*32 + (x >> 8)`. The screen selects a 512-byte metatile page; the
within-screen byte offset is `((y & $F0) << 1) + ((x & $F0) >> 3)`. That index
wraps to 16 bits before the long read at RAM `$7E2000 + index`.

The metatile word indexes the collision property table pointed to by RAM
`$0B92..0B94`. Its low six bits select terrain behavior. This is **not** the
graphics definition table at `$0B95`. Read current RAM, not an initial ROM
layout, so broken/changed tiles immediately affect projectiles.

Floor dispatch `$84:961C`: classes 1-4 are half slopes and 5-12 quarter slopes
(`$96D3..9815`); `$13` is solid (`$96B5`). Classes `$33..3F` contain solid,
conveyor, ladder-top and spike behaviors. `$39/$3A` tops are one-way. Water and
ordinary ladder classes do not block these attacks. The port reads these
properties without invoking player-only hazard or conveyor side effects.
The swept projectile helper currently covers static tile terrain and slopes;
dynamic moving-platform actor contact still needs a separate integration.

## X3 Acid Burst

Verified against original ROM and private runtime traces:

- Normal actor class `$07`, group `$05`; initial source velocity `($0120,$0280)`
  with gravity `$20`. Source Y is positive up. Up/down modify the lob to
  `(0,+$0500)` / `(0,-$0500)`; table `$06:B893..B8A2`, selection `$81:919B..91DE`.
  Normal flight uses animation sequence 2, poses 10-14, three frames each.
- Native terrain contact `$81:91FC..9267` selects sequence 7 for floor,
  10 for ceiling, 11 for wall; wall contact flips facing. A floor splash lasts
  40 frames. The animation flag on pose 37 triggers four droplets; tables at
  `$06:B8AD..B90C` specify their offsets/velocities. Droplet actor `$18` uses
  gravity `$30`, ten initial terrain-grace frames, sequence 4, then sequence 5
  on terrain. Source routines `$81:92D3..933F` and `$81:93BD..945F`.
- Charged actor `$10`, same group: `$81:947E..9545` creates two blobs, initially
  `($00C0,$0500)` and `($0200,$0300)`, source gravity `$38`. Sequence 12 grows
  from pose 20 through 15,10,5 to 0; then sequence 0 animates full size.
- Charged collision `$81:9555..95F9` permits five terrain contacts, emits a
  splash each time, reverses X on walls and uses source vertical speed
  `+$0500` on floor / `-$0500` on ceiling. Floor/ceiling contact sets X speed
  magnitude `$0100`. Splash clones are created at `$81:962B..965A`.
- Original collision radii: normal 6x6, charged main blob and droplets 4x4,
  charged splash 8x8. Visual growth does not enlarge the main blob's source
  damage box. Enemy impact uses sequence 6; normal terrain splashes are visual
  parents for the damaging droplets.
- Measured energy: normal **1**, charged **2**. Private original runtime went
  from 28 to 27 on the initial charging press, then to 25 on charged release.
  Do not reuse Spinning Blade's three-energy charged cost.

Port checkpoint covers those airborne, tile-contact and enemy-contact forms
for both characters. Source underwater dissolution (`$02:DF16` called from
`$81:9183/$94C5`) and source audio are still pending; current sounds come from
X1. Revisit moving-platform contact and broader slopes/ceiling/wall playtests
alongside the other terrain-sensitive weapons. Those are open fidelity items,
not facts already established by the highway floor test.

## X3 Ray Splasher

Normal class `$0B`, sprite group `$0D`: `$81:A19C..A20F` attaches its muzzle
effect to the player for 60 frames, emitting a ray every eight frames (seven
rays total). The muzzle uses animation sequence 10. Child class `$1C` setup
`$81:A254..A2CB` uses sequence 3 (pose 13), radius 8x8 and a speed lookup:
normal directions `$06:B9C8..B9CF`, charged directions `$06:B9D0..B9DF`,
vectors `$06:E18E` multiplied by eight. The normal first directions produce
`(3968,+992)`, `(3968,-992)`, `(4096,0)` in source coordinates when facing right.
The ray trail is real source art: effect class `$0F`, `$81:8578..85FB`, uses
sequences 4/5 (poses 14/15), delayed along the parent's position history.

Charged turret class `$14`: `$81:B7C7..B83B` waits for the character's deployment
animation, then `$81:B83C..B856` launches upward at `$0300` with deceleration
`$20`. Once stationary, `$81:B858..B878` fires every eight frames for 180 frames.
`$81:B8E0..B914` cycles sixteen radial directions; this is not an enemy-seeking
turret. Animation sequences 0/1 supply launch/active poses. Original deployment
trace starts at player `(-2,-44)` and launches about 36 frames after release;
that delay comes from the source body-animation flag, not a proven universal
timer. Adapt the deployment pose/timing carefully for X1 X and X3 Zero.

**Inventory prerequisite:** measured normal cost is 1; charged cost is **2.5**.
The private source RAM pair `$1FC3` changed from `$5B00` (27) to `$5880` (24.5)
on charged release after the one-energy initial shot. Fractional inventory is
now implemented with sixteen appended low bytes, retaining full source 8.8
precision. Game state v11/capture v10 migrate older inventory prefixes without
altering their whole units. The unused `charge` field remains separate because
menu selection clears it; it must not hold inventory fractions.

X1 pickup `$81:E0A9..E0CE` reads the full 8.8 pair and adds `$0100`. At `$1C00`
it clamps, discarding fractional excess from the last increment; `$E0B7..C1`
passes only the remaining whole pickup ticks to auto-refill. Verified port
example: selected 27.5 + small pickup becomes 28; reserve 10.25 becomes 11.25.
The selected weapon's discarded half is original native behavior, not a
fractional-state bug. HUD/menu energy still uses the pair's high byte.

Normal/charged Ray combat is now implemented. Trail history offsets are
`$06:B7DA` values `-8/-12` bytes; four bytes represent a position and the head
index advances after writing, giving the first/second prior recorded positions.
The port reconstructs these constant-velocity positions from saved ray state,
using original poses 14/15. It does not allocate cosmetic native projectiles.
Normal source angles are 7,9,8,7,9,6,10; left-facing adds 16 modulo 32. Charged
direction indexing increments before lookup. Turret launch uses Y `-44` for
X; Zero adapts that to `-52` to preserve the shared eight-pixel foot-origin
translation. Exact source body-event deployment, destruction effects, source
audio and contact-edge trail comparison remain open fidelity work.

## X1 charged-beam counter correction

RAM `$0C25` increments at `$81:A2E9` and decrements at `$81:A407` when a native
class-3 full-charge beam finishes its disappearance. It is **not** a general
firing-pose timer. The earlier Blade adapter pinned it to two as an attempted
pose hold, leaving a stale count after the host-owned blade retired. That
write is removed. Ray/Blade runtime checks now require zero native charged-beam
count after the extended attacks finish. Own projectile count remains `$0BDD`;
do not confuse these counters.

## X2 Sonic Slicer

Normal class `$0B`, group `$41`, DMA `$85:9F83`, animation `$2F:EC62`:
`$81:9622..9666` waits for sequence 0's final flag, then starts sequence 1
and creates a second blade. Table `$86:B784` gives horizontal speeds 768/896
and source upward accelerations 4/8 in 8.8 units; initial source Y speed is
-128. Bounds `$86:B77A` are `(0,0,12,8)`. `$81:9667..96A9` reflects off walls;
`$81:96AB..96D5` reflects and halves vertical speed, retiring on the third
vertical contact. Normal impact sequence 4 uses poses 16-21. Normal cost is
**0.5**, verified from the native energy pair and preserved by host fractions.

Charged class `$14` uses **group `$87`, not `$41`**: DMA `$85:9FBE`, animation
`$2F:D54D`. The initial 205-frame source recording still showed the normal
pair and was not evidence for the charged form. A private 400-frame charge
allowed that pair to retire and produced the real five-blade attack. The
charged group adds 19 locally extracted poses. Pose 18 is the inherited blank
8x8 tile `$45` (all pixels zero); there are only 18 entries in its DMA table.
The descriptor marks that pose as inherited instead of reading past the table.

`$81:A570..A5BF` sets up the charged form. `$81:A5D2..A613` waits on forming
sequence 0's last flag, starts sequence 1 and spawns four siblings. Source
velocity rows `$86:B9F9` are `(0,2304)`, `(400,2269)`, `(-400,2269)`,
`(787,2165)`, `(-787,2165)`; Y is positive up. Gravity bytes at `$86:BA0D`
are 80,78,78,75,75. `$81:A614..A632` stops horizontal travel at the apex,
switches to sequence 2 (pose 11) and downward acceleration 96. `$A647..A653`
clamps the source Y high byte to `$F8` after movement, preserving its low byte.
Charged blades pass through tiles. Bounds `$86:B9F1` are `(0,-2,10,11)`.
Impact uses sequence 3; charged cost is **2**, separate from the initial
half-unit normal shot that can begin a charge.

Port checks cover X and Zero, fractional debit and insufficient energy,
charging locked without arms, native floor reflection/third-contact retirement,
five charged arcs/apex transitions, native enemy damage/impact retirement,
slot and charge-audio cleanup, and exact save/replay before splitting and
across the apex. Normal and charged original-art captures were inspected.
The existing combat/save ABI is unchanged. Open fidelity items: normal trail
history (`$81:97B3..97FF`), wall-contact cosmetic effects, source sound import,
and broader terrain/movement comparisons. The shared swept tile solver resolves
contacts in the current tick; the source consumes its preceding contact flags.


## X1 gameplay weapon HUD footer (2026-09-29)

The extended weapon proxy submits slot 7 at (24,80), attribute $3620. Its
buster-loaded tile $20 contains unrelated graphics, so simply drawing the
pause icon lost the gameplay meter enclosure. Reuse the live X1 health footer
tile $86 with palette 2: side columns 1/14, white lower rim and rounded black
outer edge. The X glyph reaches columns 2/13, so the first 10x10 clear left
colored corner remnants. Clear columns 2..13 and rows 2..12 to black index 1;
preserve row 13's lower bevel and the original segment connection. Menu art
has its own border in the outer two pixels. Find the nonblack symbol bounds
only within source x/y 2..13, center them in the 12x11 inset, and reduce only
oversize symbols with centered nearest samples. Derived HUD pixels are built
after asset loading; cache format and original 16x16 pause art are unchanged.
The corrected native-width replay capture was inspected visually.

### X1 shoulder cycle reference

`$81:99D3..9A6F` checks `$1F23`, projectile count `$BDD`, `$1F31` and actions
`$18/$42` before changing weapons. Pressed `$BE2` bits `$10/$20` advance or
reverse; both held `$BDE & $30` return to buster. Ownership bit `$40` in
`$1F86 + native_weapon` filters X1 choices, independent of remaining energy.
HUD `$1F12=0` rebuilds the special meter, `4` removes it for the buster.
Extended selection handles these buttons before the normal player action and
consumes only pressed bits. Clearing held bits would break next-frame edge
detection and repeatedly cycle a held shoulder. Page 0 still uses original code;
pages 1/2 wrap weapon IDs 0..8 without changing page, including fallback entries.


## Spin Wheel (X2)

The source handlers are in **bank $87**, not $81. Normal class $0A uses group
$46, DMA $85:9F2E, animations $2F:EDD2. Entry $87:848D / init $84A8 sets
momentum +$37 to $0400 and starts formation sequence 0. After its end flag,
$8565 starts sequence 1 with VX/VY zero and downward acceleration 64; $8937
caps falling VY to source -$0400 after movement. Ground contact starts a
30-frame wait ($85BD..8624), then rolling ($863C..86A9). Ground traction
$88A1 adds a signed value from $86:B75E: 16,-20,0,-16,-4,-4 depending on the
source slope orientation; the flat-ground index is verified below.
Airborne momentum loses 4/frame, wall reversals lose 16. A wall stall waits
30 frames while losing 8/frame, then hops with current momentum as upward VY
($872D..87D2). Exhausted momentum enters shrink sequence 10. Enemy contact
has a 10-frame pause with momentum loss ($87D3..8822), not immediate retirement.
Normal collision bounds $86:B754 are (0,0,9,9). Spin sequences 1..4 use poses
6/8/7 at durations 1..4; 5..8 are ground-effect poses 9..13.

Charged class $13 starts at $87:894E. Sequence 11's flag-1 pose 15 triggers
eight children ($8A13..8A75). Table $86:B99D has signed dx,dy,VX,source VY:
(0,8,0,-1024), (6,6,724,-724), (8,0,1024,0), (6,-6,724,724),
(0,-8,0,1024), (-6,-6,-724,724), (-8,0,-1024,0), (-6,6,-724,-724).
Children use static directional poses 19..26 (sequences 12..19), constant
velocity and terrain-passing movement ($8A95..8A9F). Parent bounds $86:B9DD
are 7x7; children $86:B9E7 are 6x6. Formation emits around release frame 13/14.
The source uses nine actor slots including the visual parent; X1 has eight
projectile slots, so retain all eight directions and render the center burst
cosmetically in the port. Energy costs are verified below. All 27 poses are
already in the extracted cache. Private trace: x2-weapons-reference.json.

Follow-up verification: private paused X2 PID 56056 / debug port 4394, fixture
0 (the owner's X1 playtest is separate), recorded in wheel-probe.json. Energy
at `$1FC0` changes `$5C00 -> $5B00` on the normal press, then `$5800` on the
charged release after that press: **normal 1, charged 3**. Original charge
formation splits at about release tick 13/14, with all eight original vectors.
Normal formation ends after 29 source animation ticks; gravity 64 reaches a
1024 downward cap. Ground center is eight pixels above the surface (damage
radius is nine), followed by the 30-frame wait before rolling.

`$88:D6FA..D751` resolves the traction index. Collision classes 1/2, 3/4,
5..8 and 9..12 select slope groups 0..3; flat/ordinary solid selects 4 (right)
or 5 (left), both **-4 momentum per frame**. For positive horizontal velocity,
table `$86:B08F` swaps 0/1 and 2/3. Thus rising half-slopes are -20 uphill,
+16 downhill; rising quarter-slopes are -16 uphill, 0 downhill. `$87:88B1`
caps positive momentum at 1024. Ground wall contact enters a 30-frame stall,
losing 8/frame, then hops with the remaining momentum as upward velocity.
Airborne wall reflection loses 16; ordinary airborne travel loses 4/frame.
`$87:87FB..8822` pauses movement for ten frames after enemy contact, losing
8 momentum each frame, then resumes the preceding movement phase. Exhaustion
plays source sequence 10 before retiring. Spin sequences 1..4 and ground
effect sequences 5..8 are selected by the momentum high byte.

Port state mapping: normal `muzzle_pose` is form/fall/ground delay/roll/
airborne roll/wall delay/shrink (0..6), `origin_x` is momentum, `radius` is
the remaining delay, `tether_pose` is the enemy-contact pause, `origin_y` is
ground-effect time. Charged `muzzle_pose` is formation/flight (0/1), `variant`
is direction 0..7, `origin_x/y` retain the center, and direction 0's `radius`
drives the six-frame original flash. Charged `tether_pose` retains the original
center facing. These fields are saved already, preserving the 40/328-byte ABI.

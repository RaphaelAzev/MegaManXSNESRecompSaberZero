# Additional X1 boss weaknesses

Owner-approved mappings, tracked in `beads-8wg.1.50`. These extend the X2/X3
weapon mods for both characters, including co-op. At least one weapon pack
must be enabled; disabling both packs restores stock X1 behavior.

| X1 boss | Original weakness (unchanged) | Additional weaknesses | Added normal / charged HP per accepted hit |
| --- | --- | --- | --- |
| Chill Penguin | Fire Wave | Speed Burner | 3 / 4 |
| Spark Mandrill | Shotgun Ice | Strike Chain, Frost Shield | 3 / 4 |
| Armored Armadillo | Electric Spark | Triad Thunder | 3 / 6 |
| Launch Octopus | Rolling Shield | Triad Thunder, Electric Spark | 3 / 3 |
| Boomer Kuwanger | Homing Torpedo | Parasitic Bomb | 3 / 4 |
| Sting Chameleon | Boomerang Cutter | Sonic Slicer, Spinning Blade | 3 / 4 |
| Storm Eagle | Chameleon Sting | Spin Wheel, Sonic Slicer, Spinning Blade | 3 / 4 |
| Flame Mammoth | Storm Tornado | Bubble Splash | 3 / 4 |

These values come from X1's live damage rows, not a new global multiplier.
Original projectile lifetimes, multi-hit cadence, boss hurt timers, immunity,
and reflection remain in force. A charged attack can contain multiple damaging
components; the table describes each contact the native game accepts, not the
total damage from holding/releasing the attack.

Octopus is an explicit exception: charged Rolling Shield's native boss damage
is zero because it is a defensive barrier. Both forms of the added electrical
attacks use the normal Rolling Shield weakness value (3 HP). Original charged
Rolling Shield remains unchanged at zero boss damage.

## Damage and reactions are separate

`src/mmx_weapon_weakness.inc` contains an extensible list keyed by target actor
kind, source-game page, and imported weapon ID. Each entry independently
specifies its normal/charged damage reference and reaction class. Add future
interactions here; do not infer equivalence from the weapon's source Maverick.

- Speed Burner preserves Fire Wave's native burning reaction. Its underwater
  bubbles and unlit underwater dash remain excluded from the fire mapping.
- Frost Shield invokes Mandrill's original Shotgun Ice freeze reaction.
- Strike Chain uses Mandrill's weakness damage but does not become an ice attack.
- Triad Thunder preserves Electric Spark's original Armadillo armor break.
- Slicer/Blade use Chameleon's original Cutter contact classification.
- The remaining additions change damage while retaining their attack class.
  Electricity against Octopus does not cut his arms; Bubble Splash does not
  cut Mammoth's trunk. Original Boomerang Cutter appendage reactions remain.

## X1 source notes

The native collision routine `$84:9E15` reads the projectile's class, looks up
the target's current damage row at `$86:EF37`, and dispatches reflection or
immunity before subtracting HP. The table directory is a word-offset array;
normal special classes `$07..0E` have charged counterparts nine entries later.

The damage-class hook at `$84:9E1D` changes only this contact's lookup. The
reaction hook at `$84:9E3A` separately publishes `$1F1D`, preserving the actual
projectile ID and imported animation. Both interpreter and generated high/low
bank mirrors use the same helper. The final imported damage adapter avoids
scaling this native weakness value again. No ROM table or save format changes.

| Boss | Actor kind | Native damage row | Original weakness class |
| --- | --- | --- | --- |
| Penguin | `$02` | 9 | `$0A/$13` |
| Mandrill | `$31` | 13 | `$07/$10` |
| Armadillo | `$14` | 11 | `$0C/$15` |
| Octopus | `$07` | 7 | `$08/$11` |
| Kuwanger | `$05` | 10 | `$0E/$17` |
| Chameleon | `$0A` | 6 | `$0D/$16` |
| Eagle | `$52` | 8 | `$0B/$14` |
| Mammoth | `$0C` | 12 | `$09/$12` |

Native reaction sites: Penguin `$81:B683..B69C` (fire); Mandrill
`$87:8C7F..8C92` (ice); Armadillo `$83:B387..B398` (electric armor break);
Octopus `$87:935E..936D` and Mammoth `$81:C58A..C595` (Cutter appendages).
Stage-object elemental compatibility is documented separately in
[X1 stage reactions](x1-stage-reactions.md).

## Focused validation

`MMX_WEAKNESS_TEST=1` runs the original ROM collision routine in
`mmx_state_tests`, with `MMX_WEAKNESS_X2_ASSETS` and
`MMX_WEAKNESS_X3_ASSETS` pointing to private `MMXWEAP5` caches. Run in an
empty private working directory and pass the owner's X1 ROM as the argument.

The regression covers all 13 added pairings in normal/charged form through
the interpreter and generated collision entry; real HP subtraction,
published reaction class, unchanged projectile class, and native zero-damage
and reflection rows. It also checks all eight original weaknesses, Cutter
appendage classification, an unlisted pairing, underwater Speed Burner, and
stock Octopus/Spark damage with the weapon mods disabled. This is focused
collision validation, not a claim of complete playthroughs of every boss.

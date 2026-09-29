# Triad Thunder source and X1 adaptation

Central Beads `beads-8wg.1.39`. X3 weapon ID 3, projectile group `$0B`.
All graphics and animation records are extracted from the owner's original
USA X3 ROM. No ROM data, sprite sheets, or generated asset caches belong in
the release repository. The shared X3 ROM setting supplies this weapon and
Zero; extraction happens behind the scenes.
The host retains its existing X1 firing/charge audio integration; this change
does not import the X3 sound driver or sample bank.

## Source map

| Function/data | USA X3 address | Meaning |
| --- | --- | --- |
| Normal orb dispatch/init | `$81:9A0B / 9A1C` | Native class `$09` |
| Orb companion allocation | `$81:9AA1` | Three positions, one energy cost |
| Orb hold/link/release | `$81:9AFD / 9B2C / 9B80` | 30 ticks, 38 ticks, then bolts |
| Repeat input/renewal | `$81:9BE4 / 9C17 / 9C53` | Four fire edges, at most three renewals |
| Player displacement | `$81:9CA0` | Triangle follows player motion |
| Link/bolt initialization | `$81:9CE4` | Native class `$1B` |
| Dynamic bolt boxes | `$81:9E8A` | Animation flags select collision segments |
| Orb/link/bolt boxes | `$86:B93C..B97F` | Original centers and radii |
| Angle indices, links, velocities | `$86:B980..B9BB` | Source Y is upward-positive |
| Charged controller | `$81:B167 / B178` | Native class `$12`, creates two waves |
| Ground wave | `$81:B230 / B241` | Native class `$0F`, contour following |
| Charged boxes/speed | `$86:BA72 / BA76 / BA80` | Screen quake, wave, 4 px/tick |
| Charged player action | `$84:A2B3` | Source action `$72`, landing and punch |
| Base-X body animation | `$3F:D09E`, sequence `$22` | Original punch timing and poses |
| Base-X body DMA | `$85:D334` via `$86:B26C` | No imported armor overlay |
| Ordinary damage row | `$86:E55D` | Buster base 3 |

The private X3 reference fixture was stepped with normal and 205-tick charged
releases. RAM traces established the projectile classes, source coordinates,
animation timing, body group, energy costs, and ground-wave release point.
The reference process was separate from the owner's running game.

## Normal attack

One energy creates three orbs. They travel outward for eight ticks at four
pixels per tick, arriving at player offsets `(0,-40)`, `(-32,24)`, `(32,24)`.
They charge for 30 ticks, then retain a triangle of connecting electrical
arcs for 38 ticks. The connecting actors grow, loop, and shrink through the
original sequences 3/4/5/6. Orb sequences 0/1/2 preserve their source frame
durations and loops. Each component remains attached to player displacement.

At the final boundary the three orbs become falling debris and emit the
three original bolts. Bolt sequences 7/8 grow for 18 ticks while attached,
then move at `(0,-2048)`, `(-1636,1228)`, `(1636,1228)` in downward-positive
8.8 host coordinates. The full sprite footprint controls offscreen retirement,
so the long original artwork does not disappear when only its origin exits.

Four new fire presses during the active formation request another cycle.
At its boundary, one more energy rotates the triangle vertically through a
16-tick route. Up to three additional cycles are allowed. Original horizontal
and vertical sprite flips apply to the orbs, links, and released bolts.
Holding fire still builds charge through X1's native charge system.

## Charged attack

X1's arm upgrade is required, for either playable character. A full release
costs three energy; the initial uncharged shot while beginning the hold costs
its usual one. The body waits for solid ground, then plays the original X3
punch sequence: 24 ticks of windup, 60 ticks of earthquake, 120 ticks with the
ground waves, and nine ticks of recovery. Inputs cannot cancel the committed
punch. Damage/hurt and stage transitions still interrupt it.
An airborne release falls through native X1 terrain handling, capped at the
source 5.5-pixel fall speed, and begins the punch only after landing.

X uses eight original base-body poses from group `$33`. The descriptor's
`animation_sequence: 34` restricts source animation traversal to that action,
because the larger player directory includes unrelated animation commands.
Offsets, timing records, and loop bytes remain original. This extension is
address-only metadata and does not change the `MMXWEAP5` cache layout.
X1 armor overlays are suppressed during this action: there are no equivalent
X1 armor pieces for the X3 punch, so the base X body is shown temporarily.
Zero has no source punch sequence; existing X3 Zero windup/crouched strike
poses follow the same timing. No replacement sprite art was fabricated.

The controller starts screen-wide damage on the source strike flag. Every
eight ticks it invokes the state of X1's native camera quake (`$84:A341`),
equivalent to X3 `$84:D5A2`: duration 6, magnitude 3, X/Y increments 1.
The two ground waves start 60 ticks after the strike and use the original
group `$0B` sequence 10. Their terrain adapter uses live X1 metatiles and
slopes, follows floor contours, climbs walls, and descends exposed edges.
Four one-pixel probes implement the source four-pixel speed without skipping
X1 corners. This is a geometry adapter, rather than transplanting the X3
terrain engine or its stage-specific breakable scenery.

## Damage and state

Ordinary-enemy source damage is 15 for orbs and the earthquake, 5 for links
and bolts, 9 for ground waves, against source buster damage 3. Existing shared
fractional carry preserves these ratios on X1's HP scale: respectively 5,
5/3, and 3 basic shots per contact. Native X1 invulnerability/collision remains
in charge; boss responses keep neutral buster damage and existing immunity.
No X3 boss weaknesses or X3-only stage destruction rules are imported.
The later owner-approved [X1 electric compatibility](x1-stage-reactions.md)
does reuse Electric Spark's damage and Armadillo armor-break reaction.

The source diagonal bolt collision table has unusual flag-dependent entries
past the initial six boxes. Private runtime RAM confirmed those addresses;
the adapter preserves them rather than replacing them with sprite rectangles.
All formation, renewal, animation, body, quake, and wave state fits existing
saved projectile fields. The combat state remains 1,028 bytes.

## Validation

`MMX_WEAPON_TRIAD_TEST=1` selects focused native-runtime checks for both X
and Zero: triangle/link/bolt phases, renewal input and energy, arm gating,
original punch timing, frozen movement, native camera shake, delayed waves,
snapshot replay, return of controls, airborne landing, hurt cancellation, and
real native enemy damage. The final focused check exits successfully for
both characters. An ordinary enemy takes five X1 HP on one orb contact.
Native/Python extraction parity covers the added body poses and restricted
animation extraction. Captures compare original source artwork with the
port's normal triangle, emitted rays, inverted formation, punch, and waves.

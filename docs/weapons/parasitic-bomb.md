# Parasitic Bomb

Implemented for X and adapted Zero on `feat/x2-x3-weapons`.
Tracking: central Beads `beads-8wg.1.42`, parent `beads-8wg.1.32`.
Reference: X using native X3 weapon ID 2 in the original USA ROM.
Zero using boss weapons here is an adaptation, not native X3 functionality.

## Source record

| Source address | Finding |
| --- | --- |
| `$81:96F0..97A7` | Normal class `$08`, 3 px/update; reflection reverses X and gives upward Y velocity -3 |
| `$81:97AB..99FA` | Captured-enemy class `$1E`, original host art, formation and homing/stationary/rolling responses |
| `$81:982A / 98C2 / 98E7` | Homing life 180 ticks; stationary/rolling life 240; rolling gravity 32/256 |
| `$84:D00E` | Special enemy response captures instead of subtracting ordinary HP |
| `$81:AC82..AF37` | Charged class `$11`: four cursors, lock, miniunit launch, homing, impact and deflection |
| `$86:BA62` | Cursor offsets (-16,-20), (-16,20), (16,-20), (16,20) |
| `$81:AD5B / ADB5 / ADC6` | Rotating cursor search, 4 px travel, snap when either axis is within 5 px |
| `$81:ADFE / AE1C / AE33` | 16-tick lock; one unit of energy spent at miniunit launch |
| `$81:AE42..AE86` | Wait for unit completion, then 30-tick cooldown; release ends the cursor cycle |
| `$81:AD07 / AE9A..AEFC` | Unit starts at 3 px, decelerates 16/256 for 10 ticks, then homes at 4 px every fourth tick |
| `$81:AEB5..AEE5` | Unit attack box and 120-tick lifetime activate near its locked cursor |
| `$81:AF0D` | Deflected unit uses pose 39, vertical flip, zero X velocity and gravity 32/256 |
| `$84:D757` | Unit contact effect: original group `$17`, sequence 1, poses 0..2 |
| `$84:D6E0 / D6AF` | Ordinary impact/timeout effect: group `$08`, sequence 5 |
| `$86:B925 / B92F / BA5E` | Shot (0,0,13,12), payload (0,0,16,16), unit (1,-2,8,9) attack boxes |
| `$86:E4A5` | Enemy-response matrix; ordinary buster 3, shot 2, unit 5, payload 15 |

Normal formation uses group `$06`, sequence 1 (30 ticks), then sequence 2.
Cursors use sequences 8/10, miniunits sequence 12, normal projectiles sequence 0.
All art and animation records come from the user's X3 ROM. Shared effect
graphics come from compressed resource `$0A` loaded at OBJ `$6800`. Runtime
palette comparison identifies the small unit impact at ROM `$630E0`; the
larger puff uses the reference runtime's gray palette at `$66948`.

Private research includes `_research/parasitic-source-check.json`,
`parasitic-normal-impact.json`, source screenshots and the earlier bounded
assembly/traces in `_wt_mmx_weapon_fang/_research`. Source runtime port 4385
used private fixture 6 with `$0A8E=0` (X), never an owner playtest save.
Do not distribute these captures or extracted assets with the public mod.

## X1 behavior and deliberate adaptations

Normal shots cost one energy. Eligible ordinary-enemy hits retain the host's
native sprite/pose and suspend its AI and body damage. After formation it
becomes a moving or stationary payload. Timeout, collision or cancellation
enters X1's native death handling and clears contact damage. Saved host slot
and type are checked before moving or retiring an enemy.

X1 has no X3 per-enemy response table. Ordinary categories 0..5 with nonzero
boxes up to 24x32 half-extents can be captured. Larger actors (over 16x24) stay
stationary; small grounded actors roll; airborne actors home. Bosses/protected
categories remain immune to capture/targeting. Source angular motion tables
are retained, with integer nearest-direction and nearest eligible X1 target
selection. These are explicit cross-game adapters, not native X3 response types.

Full charge requires X1's arm upgrade. Four cursors appear while fire is held;
searching is free and each launched unit costs one energy. Cursors reserve
separate targets. Releasing fire retires idle cursors; active locks/units finish
their cycle. Searching follows the player, eight pixels higher for Zero, and
units launch at the active character's muzzle. The port shares X1's eight
attack slots, permits up to three ordinary shots/payloads, and waits for a
free slot before spending energy on another unit.

Ordinary damage preserves source-to-buster ratios: noncapturing shot 2/3,
unit 5/3, payload 15/3. Fractional carry uses the shared saved per-enemy
accumulator. Capture itself removes no HP. Protected encounter rows keep
neutral X1 buster damage and native immunity, without imported boss weaknesses.

## State, validation and remaining fidelity work

No save/capture ABI growth: existing 40-byte attacks store host/target,
quadrant, phase, timers and animation. Unit contact bit 15 gates collision and
lifetime; bits 0..14 retain native enemy contact history. The shared moved-enemy
renderer reconstructs host art and suppresses its duplicate native draw.

Focused ROM-runtime checks cover X/Zero motion/cost, arm gating, automatic
held-charge activation, four free cursors, movement/release and exact snapshot
replay. A real X1 enemy is captured without HP loss, keeps its AI suspended,
completes formation and enters native death on cleanup. A charged encounter
checks protected-target exclusion, one-energy launch, actual homing damage,
original shared impact, cursor recovery and replay. Captured host, payload,
search and impact renders were visually reviewed. Native/Python extraction
agrees byte-for-byte, including copier headers, wrong-ROM rejection and good
cache preservation.

Original SPC sound effects remain a shared follow-up. X3's enemy-specific
responses and stage-dependent palettes cannot map identically onto X1; the
rules and reference palette above are intentional adaptations.

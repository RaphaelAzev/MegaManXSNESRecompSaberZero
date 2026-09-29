# Crystal Hunter port and source notes

Implemented under central Beads `beads-8wg.1.45` / weapon epic `.32`.
Original USA X2 ROM is required, with no game assets committed. X is the
original reference; Zero uses adapted X1 firing poses and muzzle placement.

## Normal projectile and crystallization

Class `$07` dispatch `$81:8D1C`, initializer `$8D2D`; group `$10`, sequence 1,
DMA `$85:9D28`, box `$86:B694` = (0,0,8,8). Speeds `$86:B6A6` give horizontal
`$0340` (3.25 pixels/frame), zero initial vertical velocity, gravity `$10`.
Water uses the second record: horizontal `$0300`, vertical `$0140` upward.
`$8DBC..8DC6` clamps downward velocity by its high byte to `$FB`.

Native collision `$88:DAA9` writes the target enemy address to projectile
`+$38`. A surviving target enters projectile state 8 (`$81:8DD4`), which
copies the target position, places its enemy actor in source freeze state 6,
and forms the crystal using sequence 2. Sequence 3 is the completed block;
gravity becomes `$40`, with a half-speed upward bounce on first floor contact.
`$81:8FB1` shatters it when the player's horizontal speed reaches 3 pixels
and their boxes overlap. `$81:8F14` creates eight original crystal fragments
and invokes the source enemy/item cleanup helpers. Offscreen cleanup also
disposes of the frozen target. Boss/armored rows generally reflect Crystal
Hunter; ordinary susceptible row `$86:F40A` gives damage 1 versus buster 3.
The generic neutral row `$86:F4C8` is immune to this special-response weapon
and must not be mistaken for an ordinary numeric damage amount.

## Charged time distortion

Charged class `$10` dispatches at `$81:9CCF`. It is an invisible controller:
the stale group/pose in a RAM trace does **not** represent a frozen enemy or
a projectile to draw. This corrects the ambiguity in the early reference log.

`$9CFD..9D4D` locks the player and object groups, initializes vertical-scroll
HDMA buffers `$7F:C640/$7F:C840`, and sets a 180-frame distortion timer.
`$9D4F..9E9D` alternates those buffers, propagating paired scanline offsets
outward from the middle and changing the amplitude every sixteen frames.
The source displacement directory is `$86:B7ED`.

At distortion end `$9E9F..9ECE`, normal player/object processing resumes and
`$1F32=$80`, `$1F33=1` enable half-speed game simulation. `$80:98D6..98EB`
(and `$80:8F75..8F8F`) skip every other stage update, including the player.
`$81:9ED1` runs this for 240 simulation updates: approximately eight real
seconds. `$9EEB` clears the effect and releases its firing lock.

Costs from `$86:AF28/$AF3E` are one unit normal plus one extra for charged
release, i.e. two units for the charged effect. A private paused X2 runtime
confirmed the 180-frame onset and the subsequent timer decrementing once
per two displayed frames. It uses an isolated copied reference fixture;
the owner's game/save state is never loaded by these checks.

## X1 implementation and validation

`src/mmx_weapon_crystal.inc` implements the source projectile, one flying
shot/three frozen targets, formation, fall/bounce, solid top, dash shatter,
and charged time control. Eligible ordinary X1 enemies use damage categories
0..5; bosses and protected actors keep their native response. Their AI is
suspended through the enemy-loop hook while the crystal owns their position.
The player can jump onto a crystal or dash through it at three pixels/tick.
Shattering re-enters X1's native lethal-hit path for death effects and drops.
The dying host's contact damage is cleared, including after the crystal
controller retires; otherwise X1 can hurt the player from its death actor.
Offscreen/cancellation cleanup consumes the frozen host instead of restoring AI.

Original fragment class `$08`, subtypes `$A5/$A6`, dispatches at `$81:F068`.
Descriptors `$86:EBCC/$EBD0` select group `$10`, sequences 4/5, poses 14/15.
Eight fragments use velocity tables `$86:EE38/$EE48`, gravity `$30`, alternating
slot/frame visibility, and offscreen retirement. They occupy the existing
saved cosmetic pool, leaving native attack slots available. Source RNG
arithmetic is seeded from saved port state because X1 consumes RNG differently.

Charged onset freezes the game for 180 displayed frames, then runs the whole
simulation, including the player, every other frame for 480 displayed frames.
Pause does not consume the effect. Charging requires X1's arm upgrade. A
179-frame original-runtime HDMA capture matched the translated ripple on rows
0..222 throughout; the port uses the continuous displacement on the last row
instead of X2's byte-copy wraparound artifact there. It affects the original
BG1 vertical scroll, leaving sprite/HUD positions alone.

Focused native-runtime checks passed for both characters: source speed/arc,
energy and arm gating, frozen onset, half-speed motion, expiration, and
save/replay across onset/end. A real enemy encounter checked suspended AI,
formation, standing, native shatter death without player damage, eight original
shards, and debris/death replay. Crystal, debris and distortion captures were
visually inspected. Original SPC sound effects remain part of the shared
audio follow-up; no substitute sound is presented as X2 audio.

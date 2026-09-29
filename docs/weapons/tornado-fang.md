# Tornado Fang source and X1 adaptation

Tracked by central Beads `beads-8wg.1.37`. X3 weapon ID 8, original sprite
group `$13`, normal class `$0E`, charged class `$17`. Source is the user-supplied
USA X3 ROM; no extracted graphics are committed. This work is independent of
the released Zero mod and belongs on the X2/X3 weapon follow-up branch.

## Normal drills

`$81:A9F6` dispatches through `$A9FB`: AA07/AAB6/AC4A/AC1D/AC1D/AC79.
Initializer `$AA07` sets seven-contact durability, 60-tick staging timers,
group `$13`, animation 13, box `$86:BA32` = (-5,0,9,7), and DMA `$85:E5A5`.
Water classes `$0D/$0E` choose the stored acceleration cap: 4 px/tick in
water, 3 otherwise. This cap is chosen at creation, not every frame.

Routes `$86:BA36`, selected by `$81:AB9B`, contain duration and signed 8.8
velocities. A single drill moves forward at 2 px/tick for two four-tick
records, then pauses. A second press while the first drill's 60-tick flag
remains active allocates two drills (`$AB54/$AA7F`): four ticks forward,
six ticks vertically at +/-3 px/tick, eight ticks forward, then pause.
Their final separation is 36 pixels. The source limit is three drills.
Each press costs one energy; the extra drill does not charge another unit.

At the 60-tick boundary, `$AAF5` clears velocity and starts animation 1.
`$AB19` accelerates forward by $20/256 per tick to the stored cap. Original
animations 13, 0, 1 retain their frame durations and loops.
The drill passes through ordinary static terrain, as the source routine does;
native X1 actor collision still handles enemies and breakable actor objects.

Enemy contact `$AC1D` decrements the seven-contact durability, starts animation
2 and a 16-tick contact pause, moving forward by $10/256 for its first two
ticks. Afterwards it accelerates again. Native deflection state 4 runs
`$AC4A`: half horizontal velocity, upward 3 px/tick, gravity $10/256 and
animation 3. Normal per-target hit suppression resets after the impact pause.

## Held charged drill

`$81:BE58`, initializer `$BE69`, active `$BEB4`: the fully charged drill is
deployed while fire remains held. `$81:AF4A/$AF5D` retires it when the held
fire bit clears, energy is empty or the player's action is incompatible.
Every 64 active ticks `$BEC2` calls `$84:A556` with mode 4, subtracting the
normal one-unit cost plus two-unit charged supplement. Initial deployment
also costs three. X1's arms bit `$1F99 & 2` gates this form for either character.

The Fang-only input adapter invokes X1's existing native full-release path
when its full tier is reached while held; the unmodified physical held value
continues to own the drill. Native charge audio/visual state is stopped after
deployment. The player's firing overlay remains active until release. No
shared charge algorithm or save ABI changes are made.

`$BF62` maps source body poses through `$39:90A0/$39:9104`, placing the drill
at the arm and selecting layout poses 12..17. The X1 adaptation follows the
existing X/Zero muzzle position and adds seven pixels beyond the barrel.
Normal running/jumping/dashing retain the character's native firing animation.
Wall slide uses source vertical layouts 14/15. `$BFC4` changes the drill to
pose 15 on wall contact and increments source player +$77; `$84:8A19`
consumes this to suppress wall-slide velocity. The port holds X1's wall-slide
height while retaining native jump/action transitions. Full-stage wall-grip
and ladder pose coverage remains a playtest item; it is not claimed proven
by the standing/moving fixture.

Charged hitboxes `$86:BAEE`: horizontal (0,-6,4,6), vertical
(-4,-8,4,16), and zero box when embedded in a wall. Charged drill contacts
remain available every simulation tick, subject to the enemy's native
invulnerability logic.

## Original rotating graphics

Charged drill rotation is **CHR animation**, not pose animation. `$81:BF00`
requests directory indices `$29..$2C` every two ticks through `$80:872F`.
The `$86:97AD` directory pointers at `$86:97FF` lead to bulk DMA lists
`$86:9A61/$9A70/$9A7F/$9A8E`. These replace the drill tiles without changing
the arm-orientation layout.

Both extractors now support an address-only animated-layout descriptor.
Normal group 19 poses 0..35 remain unchanged. Four CHR phases times six
layouts are appended as cached poses 42..65; 36..41 remain empty because
those layouts belong to a separate source sequence. MMXWEAP5's binary format
is unchanged. Native extraction automatically regenerates the cache from the
user's ROM. The combined reference cache contains 624 populated poses and
451,225 bytes. Source runtime VRAM comparison matched all six orientations
for the captured phase. Native/Python extraction parity also passes.

## Damage, persistence and validation

Neutral ordinary-enemy row `$86:E55D` gives raw damage **2 normal / 6 charged**,
versus buster **3**. Existing damage normalization therefore uses 2/3 and 2
X1 buster damage respectively, retaining fractional thirds between hits.
The normal drill's individual contacts are intentionally small; seven hits
add up to 14/3 buster damage. Boss weaknesses are unchanged and native immunity
is preserved. This is neither an arbitrary one-HP projectile nor raw X3 damage
applied directly to X1's lower HP scale.

All route timers, durability, selected art phase, positions and contact state
fit the existing 40-byte shot. No game-save/capture size or version changes.
Fields are documented in `src/mmx_weapon_fang.inc`.

Focused checks cover both X and Zero: normal 2 px/tick launch, pause,
second-press paired drills, 60-tick transition, energy, arms gating, automatic
held charged activation, moving arm attachment, 64-tick upkeep, release/audio
cleanup, and deterministic snapshot replay. A real native enemy contact checks
damage and impact transition; controlled continuation checks seven-contact
retirement. Source extraction checks cover copier headers, wrong ROM rejection
and cache preservation on failure. Original charged art was visually inspected.

Remaining fidelity work: source drill motor/impact audio, the small shared
impact debris effect, broader wall-grip/ladder pose playtests and compatibility
with stage-specific moving/destructible objects. No source armor/chip bonus is
imported in this boss-weapon scope.

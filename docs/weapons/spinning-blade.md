# Spinning Blade: contact and charged timing

X3 USA ROM, native X reference. Zero uses adapted X1 firing origins; vanilla
X3 does not give him these boss weapons. See `../mod-source-roms.md` for the
user-ROM extraction requirement. No ROM, extracted art or private save fixture
is distributed with this source.

## Source evidence

- Charged dispatch `$81:B423`, active dispatch `$81:B521`: the main actor
  owns two children for the muzzle and tether. Children retire if the parent
  leaves active state 2 or sets contact flag `+$30`.
- `$81:B55C`: local tick `+$3B` is incremented once per update. On odd ticks,
  radius `+$38` increases by one eight-pixel unit, to ten. The next odd update
  starts the 120-tick hold (`+$3C=$78`).
- `$81:B5B8`: decrement the hold timer, retract when zero; otherwise only held
  Up/Down (`$0A0F & $0C`) initiates an orbit. Fire is not a rotation command.
- `$81:B5FE`: 32 angle changes, one every other local tick. The original signed
  coordinate pairs are at `$86:BA86`, not bank `$81`. Their horizontal maximum
  is 77 pixels and vertical maximum 80; interpolating a circular radius-80 path
  subtly changes the original motion. `$81:B671` retracts eight pixels on odd
  ticks. In the local timing sequence, radius is still 80 at tick 140 and is
  48 at tick 150.
- Charged collision bounds `$86:BA82` are `(0,0,13,10)`.
- `$81:B753` handles a killed target by clearing `+$30` and resuming active
  state 2. `$81:B74C` retires a reflected actor.
- `$81:B75C` handles a surviving hit: enter detached phase 8, set vertical
  velocity `$0800`, and use `$81:8CA4` to sign the horizontal `$0800` against
  facing. This handler runs each tick while state 8 persists. `$82:D6F8`
  subtracts the `$E0` horizontal acceleration initialized at `$81:B48C`, so the
  observed velocities are -2272 facing right and +1824 facing left, with
  screen-space vertical velocity -2048 (all 8.8 fixed point).

A private controlled original-X3 probe placed a surviving ordinary enemy at
the fully extended blade: 120 HP became 90 once, then remained 90 over the
remaining 45-tick trace. The blade entered state 8/phase 8 and flew off while
the tether disappeared. This disproves the early adapter's assumption that
the attached charged blade could repeatedly damage the same target every
16 ticks.

## Port behavior and validation

`src/mmx_weapon_blade.inc` owns the normal pair and charged controller. Charged
variants 0/5 extend/hold, 1/2 orbit, 3 retract, and 4 recoil. The recoil has no
damage box or tether/muzzle rendering and retires outside the viewport. Killing
a target permits continued flight; surviving contact ends the attached attack.
The player retains the native firing overlay while the blade is attached,
matching the source controller's firing-timer updates. The generic 16-tick
hit-ledger reset has been removed. Saved combat structure
size is unchanged; the held countdown and orbit index use existing velocity
fields. Previous four-pixel extension states can finish extending without
exceeding the radius limit.

Damage retains the original ratios of 9/3 for normal and 30/3 for charged on
X1's ordinary-enemy HP scale; native boss immunity and neutral damage remain.

The focused ROM-runtime suite exercises both X and Zero: arm-upgrade gating,
one-/three-energy costs, normal twin launch, native normal contact, timed hold
and retraction, fire suppression, Up rotation, surviving charged contact,
recoil velocity, damage cessation, and snapshot replay while rotating or
recoiling. Render captures cover the held blade, orbit and detached blade.
Original SPC sound and the broader weapon contact audit remain separate work.

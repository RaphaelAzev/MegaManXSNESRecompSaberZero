# Saber Zero reference

Saber Zero was written by [RaphaelAzev](https://github.com/RaphaelAzev) in the
[MegaManXSNESRecompSaberZero](https://github.com/RaphaelAzev/MegaManXSNESRecompSaberZero)
fork and merged here. It is a **Zero behavior**, alongside X3 and Modern: it
keeps the X3 Zero character and adds a Saber attack owner, the X3
finisher/wave sequence, priority-aware follow-ups, donor sprites and sound
effects, Ride Armor pilot art, X1 armor upgrades, Black Zero, and an optional
hitbox overlay.

Choose **Zero behavior → Saber Zero** in **Characters → Add Zero** (single
player; **Starting character** and the native Select exchange are unchanged) or
in **Characters → Co-op** (Saber belongs to whichever seat is Zero). Both
features already ask for the player's **Mega Man X3 USA** ROM; Saber Zero uses
that same selection. ROMs are never included.

The package's sprite and sound-effect donors are credited in
[assets/saber-zero/CREDITS.md](../assets/saber-zero/CREDITS.md). They are by
the Zashiko Mod, are included for non-commercial use, and must not be used
commercially. This project is not monetized.

## Controls

The physical SNES pad rules are deliberately narrow: Saber owns Y and the
buster/special translation owns X. The other native controls remain available.

| Button | Behavior |
|--------|----------|
| Y | Start/continue a Saber attack. On the ground it starts the 3-hit combo; in the air it is an air slash; on a wall it is a wall slash; during a grounded dash it is a dash slash. After they unlock, Up + Y starts the uppercut on the ground or during a dash, and the rolling slash in the air. After any four of the eight Mavericks, Down + Y starts the downward thrust in the air. A held Y produces one edge. |
| X | Charge and fire the buster when the buster is selected; otherwise fire the selected special weapon. X is blocked from creating a new buster/special shot while a Saber slash, finisher, or wave-shooting animation owns the action. |
| B | Native jump. |
| A | Native dash. |
| Direction | Native movement. Ground swings and dash swings mask the movement edges required by their attack state, so holding the opposite direction cannot turn a swing. |
| Select | Native X/Zero exchange in the normal standing exchange context. |

Hurt, death, reset, menus, and an exchange to X cancel the Saber owner for that
frame. A buster or selected special-weapon charge held before a slash can
continue through the slash; a release is latched and fires on the first
eligible frame after the slash. A hurt frame does not invent a buster press or
throw away an existing release latch. While frozen by Chill Penguin, Saber
Zero cannot slash or fire; mashing breaks the ice as it does for X, and attacks
resume after he is free.

## Special moves

After any two of the eight Maverick bosses are defeated, Up + Y on the ground or
during a dash starts the uppercut. The eight bosses are Chill Penguin, Spark
Mandrill, Armored Armadillo, Launch Octopus, Boomer Kuwanger, Sting Chameleon,
Storm Eagle, and Flame Mammoth. Sigma-stage bosses and Vile do not count. The
progression follows the native boss-clear flags, so it persists through saves
and passwords.

The `special_moves` option defaults to true; when off, Up or Down + Y keeps the behavior it had before these moves existed.

Before the required boss count, Up + Y keeps its previous behavior: ground
slash, air slash, dash slash, or wall slash. Wall slash always wins while Zero
is on a wall. Both special moves are Saber Zero-only and cost no energy or
cooldown.

The uppercut makes Zero rise through the regular jump path while Y is held, by
default 30% higher than a normal jump (`uppercut_height`, 100-200%). It can also
cancel the third ground slash or the dash slash: press Up + Y during either.
Releasing Y early cuts the height and ends the attack. From a dash, it keeps
dash-jump speed. It ends at the top of the rise, on landing, when Zero is hurt,
or when the jump is not possible.

After any three of the eight Mavericks are defeated, Up + Y in the air starts
the rolling slash. It performs one 18-tick spin with normal air control and a
hit that remains live through the spin. Landing ends it. The crescent effect is
drawn in front of Zero when the `crescent_effect` option is enabled. The rolling
slash plays its own crescent-slash sound; the uppercut plays the first slash sound
pitched up.

After any four of the eight Mavericks are defeated, Down + Y in the air starts
the downward thrust. It uses the normal fall and air control. It lasts until
landing or until Y is released. A hit bounces Zero up by `thrust_bounce_height`,
as a percentage of a normal jump; 0 turns the bounce off. Each bounce also makes
the double jump and air dash available again, as landing does. The thrust continues,
with one hit per bounce. Its default damage is 4 against normal enemies and 2
against bosses. Its default hit priority is 8.

The special-move art is credited in
[assets/saber-zero/CREDITS.md](../assets/saber-zero/CREDITS.md).

## Saber attacks

The ground combo is driven by distinct Y press edges:

1. Ground slash 1 has 4 startup frames, 8 active frames, and 18 recovery
   frames. Its chain window is ticks 12 through 29, with an early buffer from
   ticks 4 through 11.
2. Ground slash 2 has 0 startup, 12 active frames, and 18 recovery frames. Its
   chain window is ticks 12 through 29, with an early buffer from ticks 0
   through 11.
3. Ground slash 3 has 0 startup, 14 active frames, and 25 recovery frames. It
   has no fourth combo entry.

An air slash is 4 startup, 8 active, and 6 recovery frames. A wall slash is 0
startup, 12 active, and 8 recovery frames. A dash slash is 2 startup, 10
active, and 18 recovery frames. Each of those is a single attack. The attack
owner uses the native action context for jump, dash, wall cling, landing, hurt,
and death transitions, so an attack does not create a second movement system.

The default normal damage is 3 / 3 / 8 for the ground combo, 3 for air, wall,
and dash, 8 for the uppercut and rolling slash, 16 for the X3 finisher, and 6
for each wave pulse. The boss values are intentionally lower; the full defaults
are in the option table below.
Damage is applied once per enemy per swing where the current attack owner
requires that protection.

## Buster charge and the X3 finisher

While X is held, the Zero charge counter reaches these X3 thresholds:

| Held frames | Tier | Result |
|-------------|------|--------|
| 21 | 4 | Small charged class |
| 81 | 6 | Class-3 charged shot |
| 141 | 8 | Max-charge route |
| 200 | 8 (Saber cap) | The counter stops here; the upstream tier-10 route is not entered. |

Release of the capped charge starts the two-shot class-3 sequence. Press X
again for the second burst after the native first-burst delay. Saber Zero
changes the two emission origins so the two max shots share one vertical height,
both on the ground and in the supported airborne route.

A held buster charge survives a double jump or air dash and can be released
after the aerial move to fire the charged shot.

With Fire Wave selected, holding the buster button streams flames continuously
as it does for X, with or without the Arms part, including while a charge
builds.

After the second burst becomes newly live, the finisher window opens at the
post-player update. It is 27 frames by default and can be tuned from 12 to 60.
Press Y during that window to request the X3 finisher. The request reserves a
projectile slot for the wave, starts the upstream-style finisher, and publishes
the wave at finisher age 7. If no slot can be reserved, the Y edge is consumed
but the finisher/wave reservation cannot be completed. While the finisher or
wave-shooting animation is active, X cannot start a buster or special shot.

## Hit priority

Saber Zero records the priority and frame of an eligible hit on an enemy. A
later eligible follow-up can use the native positive collision path to break
ordinary enemy invincibility only when its priority is strictly higher and the
follow-up arrives inside the priority window. The default window is 70 frames.
Equal-priority and lower-priority hits do not qualify. Native forced states,
bit-7/reflection responses, and the protected Armadillo cases are not broadly
overridden; the priority response only changes the specific admitted follow-up
path.

The default ladder is:

| Priority | Classes |
|----------|---------|
| 1 | Air slash, wall slash, small charge, full charge |
| 2 | Ground slash 1, max shot 1 |
| 3 | Ground slash 2, max shot 2 |
| 4 | Ground slash 3, X3 finisher |
| 5 | Dash slash, Saber wave |
| 6 | Uppercut |
| 7 | Rolling slash |
| 8 | Downward thrust |

The ladder is configurable through the priority options. This is a follow-up
system, not a general “ignore all invincibility” switch.

## Rendering and Ride Armor

The Saber player overlay supplies the body, blade, palette, layer, and facing to
the custom renderer. When Zero is piloting the Ride Armor, the pilot overlay is
selected from the Ride Armor animation and aligned to the native cockpit's
visible bounds and horizontal flip. The alignment is measured against the
native OAM result rather than a fixed screen offset.

`show_hitboxes` is false by default. When enabled, the custom renderer receives
debug rectangles for the enemy, Zero, and Saber attack boxes. It is a renderer
debug option; it does not change collision or damage.

## Armor upgrades and Black Zero

Zero can collect X1's four armor parts alongside X. The parts use the shared
native `$1F99` bits, so capsules work for both characters and collected parts
persist through the password like X's parts. The package option
`start_all_upgrades` grants all four parts at stage or checkpoint start while
Zero is active; it is intended for testing, and the granted parts persist in
the password like collected parts.

| Part | Effect |
|------|--------|
| Legs (`$08`) | Double jump and air dash. Standing on a moving platform refreshes both aerial moves because platform contact counts as ground. Zero's ground dash remains innate. |
| Arms (`$02`) | Purple saber blade and Saber wave. Adds 2 damage to every Saber attack, including special moves, or 1 against bosses, after tuned damage and before the 32-point cap. A full buster charge fires charged X1 special weapons. |
| Body (`$04`) | Reduces all enemy damage Saber Zero takes by 25%, including contact and hits (damage minus a quarter, rounded down). X keeps X1's native halving. |
| Head (`$01`) | Buster charges at 1.5 times the normal rate. X1 special weapons cost half energy using fixed-point accounting, so whole-unit costs alternate. Breaks the helmet blocks X breaks in Flame Mammoth. |
| All four parts | Black Zero: black and grey armor with pale hair. The appearance also applies on death and boss defeat. Death orbs use grey. |

During the armor-capsule celebration, Zero remains visible in his idle
stance instead of vanishing.

The Black Zero palette follows the MMX3 Zero Project romhack. This repository
contains colour constants only; it does not distribute ROM data. See the
[credits](../assets/saber-zero/CREDITS.md).

## Generic Zero extension hooks

`MmxZeroExtension` exposes these optional generic hooks for Zero behavior. An
absent hook preserves the original behavior:

| Hook | Purpose |
|------|---------|
| `colors` | Supply the Zero palette. |
| `modern_movement` | Enable aerial modern movement actions. |
| `charge_rate` | Supply the charge increment in Q8.8 fixed-point units per frame. |
| `weapon_cost` | Adjust a weapon's energy cost. |
| `player_damage` | Adjust incoming enemy damage, including contact and hits. |
| `death_orb_color` | Supply a Zero-owned death-orb pixel color. |

## Saber Zero settings

Tuning lives in the separate **Characters → Saber Zero settings** feature
(`megaman-x.character.saber-zero`), which claims no plugin and does nothing
unless a Zero behavior is Saber Zero. While it is disabled every option keeps
the default below, even if a different value was stored earlier: a disabled
feature is left out of the netplay mod comparison, so its values must not reach
the game either. It exposes 42 numeric tuning options and five boolean options:
`show_hitboxes`, `start_all_upgrades`, `special_moves`, `unlock_special_moves`, and
`crescent_effect`. Normal damage values accept 1–32; boss damage accepts 0–32,
where 0 means “use the matching normal value”; priority values accept 1–9.
The manifest also exposes `start` and `start_all_upgrades`; the table includes
their defaults.
The window ranges are 30–120 frames for priority and 12–60 frames for the
finisher. Swing volume ranges from 0–200 in steps of 10.

| Group | Options and defaults |
|-------|----------------------|
| Character/debug | `start=zero`; `start_all_upgrades=false`; `show_hitboxes=false`; `special_moves=true`; `unlock_special_moves=false` (testing aid); `crescent_effect=true`; `uppercut_height=130`; `thrust_bounce_height=60` |
| Normal damage | `slash1_damage=3`, `slash2_damage=3`, `slash3_damage=8`, `x3_finisher_damage=16`, `air_damage=3`, `wall_damage=3`, `dash_damage=3`, `wave_damage=6`, `uppercut_damage=8`, `rolling_slash_damage=8`, `thrust_damage=4` |
| Boss damage | `boss_slash1_damage=1`, `boss_slash2_damage=1`, `boss_slash3_damage=2`, `boss_air_damage=1`, `boss_wall_damage=1`, `boss_dash_damage=1`, `boss_x3_finisher_damage=6`, `boss_wave_damage=4`, `boss_uppercut_damage=2`, `boss_rolling_slash_damage=2`, `boss_thrust_damage=2` |
| Priority | `slash1_priority=2`, `slash2_priority=3`, `slash3_priority=4`, `air_priority=1`, `wall_priority=1`, `dash_priority=5`, `charge_small_priority=1`, `charge_full_priority=1`, `max_shot1_priority=2`, `max_shot2_priority=3`, `x3_finisher_priority=4`, `wave_priority=5`, `uppercut_priority=6`, `rolling_slash_priority=7`, `thrust_priority=8` |
| Windows/audio | `priority_window_frames=70`, `finisher_window_frames=27`, `saber_swing_volume=50` |

## Co-op, savestates and netplay

Co-op runs the player routine once per seat. Every Saber rule follows the seat
that is Saber Zero, whether P1 or P2, including body armor: that seat takes 25%
less enemy damage from contact and hits; an X seat keeps native halving. Saber's
per-frame and per-hit callbacks run only on Zero's seat; X's pass and co-op
"ghost" replays leave Saber state untouched, and a co-op seat exchange keeps it
(it uses `MmxZeroSelectState`, which does not reset the Zero extension). In
co-op the Saber pad is the seat's own X and Y, not port 1. A partner-seat Zero
is drawn with the Saber overlay as well; the Ride Armor pilot sheet is applied
only to the live seat, so a partner Zero piloting a Ride Armor keeps the native
cockpit art.

Saber's host-side state (attack, combo, hit priority, wave bookkeeping, pad
edges, and special-charge release ownership) is part of the savestate as a
fixed Zero-extension blob, version 2. A save state taken mid-slash resumes the
slash with its hitbox. Rewind and netplay rollback restore the same consistent
Saber state. The special-charge release latch survives save/load. Older Saber
data loads with the in-progress Saber state fully reset.

## Known limitations

With **Show hitboxes** in co-op, the Zero box follows the live seat, which may
be X. The **Developer → Hitbox overlay** mod outlines both seats and takes over
from this option when both are on. Saber Zero, Add Zero and Co-op remain the only features that use the
`megaman-x.zero` plugin key; Saber Zero settings uses none.

The fuzz test has not been run. The only boss-death issue found in the current
survey was Chill Penguin's native post-hit protection, and it is fixed. Other
bosses were surveyed, but only Chill Penguin, Mammoth, and Armadillo were
played through during this pass. Treat other boss interactions as needing more
playtesting.

Additional Zero saber techniques and Zero-related dialogue and other cutscene
entries remain planned.

## Running the tests

These are RaphaelAzev's MinGW commands from the fork. The ROM-backed runner
also needs private save fixtures under `_private/saves`. The commands below do
not reconfigure CMake:

```bash
export PATH="/c/msys64/mingw64/bin:$PATH"
cd MegaManXSNESRecompSaberZero
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
bash tools/saber/run_saber_rom_tests.sh
```

The Saber runner's no-argument form runs its default pass and every named group
in isolated test runs, and returns zero only when all of them pass. It includes
the input, attack lifecycle, buster, finisher, priority, damage, renderer,
Ride Armor, package, upstream Zero parity, response-seam, and fixture checks.
The named groups include `saber-frozen`, `saber-fire-wave-armor` (the real Fire
Wave route), and `saber-capsule`; `saber-capsule` uses the operator fixture
`capsule.sav` from the fixture directory.
To run one named group, for example:

```bash
MMX_SABER_TEST_ONLY=saber-priority bash tools/saber/run_saber_rom_tests.sh
```

`zero-response-seam` is exercised once through the interpreted path and once
through the compiled path by the runner. The ROM-backed runner needs the local
Mega Man X USA and Mega Man X3 USA ROM selections and the documented test save
inputs; it does not print ROM or save bytes.

CTest covers the unit, renderer, asset, and generated-hook checks. If the
`mmx_reset_reboots` test is the only intermittent failure, rerun the same CTest
command once; a repeated failure is not considered a pass.


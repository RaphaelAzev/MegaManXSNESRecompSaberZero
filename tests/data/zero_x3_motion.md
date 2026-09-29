# Original X3 Zero movement reference

`zero_x3_motion.csv` contains measurements, not ROM or graphics data. Recorded
from the original USA X3 ROM (SHA-256 `65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7`)
through the X3 project's current interpreter build, with execution mode `off`.
Zero was selected through the original pause-menu communication/swap sequence.
The stationary fixture is on flat ground near the beginning of Neon Tiger.

Each case starts from that same complete save with no buttons held, waits ten
frames, then clears only the player's X/Y fractional bytes. Each row is the
state after one input frame. X/Y are signed displacement in 1/256 pixels;
VX/VY are signed native 8.8 values. The columns `action` and `sub` identify X3's
internal states, not required X1 state numbers. Group 74 is Zero's `$4A` body.

Inputs (default SNES bindings):

| Case | Held inputs and durations |
| --- | --- |
| run | Right 30, neutral 8 |
| jump | B 44, neutral 16 |
| short_jump | B 5, neutral 40 |
| dash | A+Right 40, neutral 8 |
| dash_jump | A+Right 5, A+Right+B 32, neutral 20 |

The X1 ROM-backed test replays these from its unupgraded Highway fixture.
It compares every position and velocity, plus the mirrored original animation
pose after the first frame. The first pose is excluded because the two saved
fixtures can start at different idle-blink phases before the action starts.
The native X1 animation fields remain responsible for gameplay events.

Targeted ROM checks also find the 180-byte base movement tables identical:
X1 `$86:B9B1`, X3 `$86:B272` (30 records of VX, VY, acceleration). This establishes
the shared base values; these five trajectories do not establish water, ladders,
wall contact, damage, stage-specific terrain, or full campaign coverage.

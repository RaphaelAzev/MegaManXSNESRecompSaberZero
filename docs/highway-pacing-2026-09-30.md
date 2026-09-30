# Headed Highway pacing checks, 2026-09-30

Tracking: `beads-8wg.1.56`. Owner requested several visible Highway runs with
X and Zero moving right to investigate offline co-op as a stutter contributor.

## Method

- Actual SDL3 Direct3D 11 window, 165 Hz primary monitor, scale 3, audio on,
  rewind at its existing 15-frame interval. No dummy/headless renderer.
- Private installation under `_research/highway-pacing/bin`; owner installations,
  ROM selections, saves, and graphics settings were not changed.
- Fresh boot each time. Turbo only skips the logos/menu, then the script waits
  for the native gameplay mode, returns to normal speed, and settles 180 frames.
  No save states, invulnerability, position writes, or enemy modifications.
- Both actual controller ports hold Right for 480 frames; a further 12 short
  jump/fire sequences keep both moving through enemies and the first gaps.
- The shared script driver gained `p1:`/`p2:` button prefixes and logical port
  presence so the production binary can drive both players without a debugger.
- Four measured runs: co-op/VSync on, solo X/VSync on, repeat co-op/VSync on,
  co-op/VSync off. Separate preliminary screenshots qualified the route. Timed
  runs capture screenshots only after the movement interval. CSV timing stays
  in memory until exit; start/end state dumps are outside the analyzed interval.
- Read-only process sampling checks positions and normal input/status fields.
  Both co-op actors advance from X=128/160 to 696/728 during the walking section,
  and to 1378/1500 by the end of the longer route. Both remain alive. The three
  co-op runs finish with identical WRAM SHA-256:
  `9f21d65d57086c18dd99fefbef68f7cdfff0b202af9f01d25cccf85f7a1d0dbd`.

The first attempted visual qualification exposed that the previous startup
profiling script pressed Start too early. Its frames 601–1200 were boot/title
screens, not Highway gameplay. Those earlier measurements must not be used
as evidence of smooth scrolling. The route below gates on actual gameplay.

## Results

The directly comparable initial walk uses frames 2341–2814, with 473 completed-
present intervals per run. All actors are alive in this section. The later solo
route ends in death, so its complete-route average is not a matched comparison.

| Initial walking segment | Mean | 99th percentile | Maximum | >25 ms |
| --- | ---: | ---: | ---: | ---: |
| Co-op, VSync on, pass 1 | 16.772 ms | 19.989 ms | 35.344 ms | 4 |
| Solo X, VSync on | 16.768 ms | 19.648 ms | 44.045 ms | 3 |
| Co-op, VSync on, pass 2 | 16.771 ms | 20.030 ms | 34.322 ms | 4 |
| Co-op, VSync off | 16.774 ms | 19.037 ms | 35.377 ms | 4 |

The larger events repeat around guest frames 2556–2561 in every run. Co-op's
frame 2556 reports 2.603991 runtime frame periods and frame 2560 reports
3.074380; solo reports 2.593399 and 3.142147. The subsequent host waits account
for much of the long intervals. Presentation itself costs only about 0.5–1.9 ms
on those exact events. Disabling VSync preserves the long-frame locations and
the co-op guest-period values.

Ordinary guest-update wall time in the walking segment averages about 0.60–0.67
ms for co-op versus 0.45 ms solo. Composition averages 4.61–4.90 ms for co-op
versus 4.82 ms solo. Co-op adds work, but these results do not identify it as
the primary cause of the largest reproducible hitches.

`RtlLastFramePeriods()` derives its value from the shared APU frame clock,
not measured monitor scanout. The next investigation should attribute the
extended simulation/audio-time budgets at frames 2556 and 2560 and determine
whether they represent required guest work or runtime over-accounting. These
tests do not establish original-SNES lag-frame parity and do not resolve the
remaining physical-display/VRR question. A 165 Hz cadence explanation alone
is insufficient for the measured host stalls.

## Reproduction and artifacts

The checked-in input route is
[`tools/scripts/highway-coop-pacing.txt`](../tools/scripts/highway-coop-pacing.txt).
Use a private installation, enable Couch co-op with P1 X and supply the local X3
ROM, then run the normal desktop executable with `--no-launcher --rom <X1 ROM>
--script <route>`. Set `SNESRECOMP_FRAME_TIMING` to an absolute CSV path and
`SNESRECOMP_DUMP_DIR` to a private output directory. Disable physical input
sources during automation to prevent user input contaminating the route.

The local `_research/highway-pacing` directory retains per-run `frames.csv`,
`run.log`, `positions.json`, start/end dumps, and `end-window.png` screenshots.
`walking-summary.json` covers the comparable walk; `summary.json` includes the
longer mixed movement route. ROMs, state dumps, extracted assets, and screenshots
are not part of this commit. The summarized measurements are in
[`highway-pacing-2026-09-30.json`](highway-pacing-2026-09-30.json).

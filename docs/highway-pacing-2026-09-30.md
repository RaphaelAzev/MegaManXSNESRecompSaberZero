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

## Fix and validation

The large events came from X1's graphics decompressor, not an arbitrary audio
clock overcharge. A temporary in-memory instruction/cycle probe attributed the
extra work at host frames 2556 and 2560 to `$80:B2AB..B2D0`: 2,432 and 3,520
decoded bytes, respectively, in one scheduler iteration. Ordinary nearby frames
executed about 2,000 interpreted instructions; these executed 28,334 and 39,720.
The private probe was removed after diagnosis; it is not in the shipped host.

The ROM's conditional `$8121` checkpoint tests `$0B9D` every 256 output bytes.
The original NMI can set that flag while the graphics task runs. This port
delivers NMI before its task walk, which then clears the flag. No mid-task NMI
ever sets it again, so the decompressor never takes the conditional yield and
finishes the entire resource at once. The extended APU clock correctly retains
that oversized iteration, and the desktop clock consequently waits for it.

`MmxGraphicsShouldYield` now budgets those native checkpoints against one frame,
reserving `0x18000` master cycles for a worst-case 256-byte literal batch and
its coroutine epilogue. When the next batch would overrun the budget, both the
interpreter hook and compiled primitive use the ROM's `$8127` yield path. That
path preserves the coroutine's registers/stack and resumes it next tick. The
frame origin is renewed before each simulation tick, including replays.

The change is verified for the USA ROM and bypassed for JP, initial reset,
NMI-disabled work, and forced blank. No host deadline or APU duration is clamped,
and no extra NMI is injected. This restores bounded cooperative streaming in
the existing frame-model port; it is not a claim of cycle-exact console timing.

Final headed runs repeat the same fresh-boot route and matched walking window:

| Initial walking segment, fixed | Mean | 99th percentile | Maximum | >25 ms |
| --- | ---: | ---: | ---: | ---: |
| Co-op, VSync on | 16.642 ms | 19.024 ms | 19.873 ms | 0 |
| Solo X, VSync on | 16.643 ms | 18.858 ms | 20.276 ms | 0 |
| Co-op, VSync off | 16.645 ms | 18.879 ms | 20.640 ms | 0 |

Frames 2556 and 2560 now report exactly one runtime period in all three runs.
Across the longer movement route, the co-op maxima are 24.440 ms with VSync on
and 25.409 ms with it off; the latter has one interval above 25 ms. Remaining
host jitter and physical 165 Hz/VRR cadence are not declared eliminated.

Both final co-op runs end at identical WRAM SHA-256:
`44af42501908a4a28909d11283b03b2e265d1486b77aba78c2f684cf38e1cf58`.
Their entire 32 KiB decompressed graphics cache matches the original baseline
byte for byte. Screenshots also retain the players, enemies, vehicles, and HUD.
Whole WRAM differs from the old run because streaming now spans game ticks.

There was host-load interference during an intermediate trial: four old hidden
private X2/X3 reference processes were still running from September 28-29,
three with over 21 CPU-hours. They were stopped before the final runs. An
unrelated broad file search also saturated F: during linking; final measurements
began after it finished. Thus wall-time improvements should not be attributed
solely to code; elimination of the deterministic extended-period events is the
direct evidence for the streaming fix. Intermediate and final captures remain
under `_research/highway-pacing`, including the contaminated trial.

The ROM-backed regression in `tests/mmx_graphics_pacing_test.inc` boots Highway,
detects an actual in-flight graphics yield, independently decodes all 2,432
bytes of resource `$1F` from the supplied ROM, and checks save/load and rollback
replay of WRAM, VRAM, palettes, and co-op state. It passes; the captured resource
finishes nine ticks after the checkpoint, with a maximum 1.011646 runtime periods
in the approach. Run `mmx_state_tests <X1 ROM>` in an empty private working
directory with `MMX_GRAPHICS_PACING_TEST=1`; optionally set
`MMX_ZERO_TEST_ASSETS` to a private X3 cache for the co-op variant.

That regression also exposed a separate inherited defect: the framework's
extended APU frame clock is absent from its saved execution/rollback state.
SPC/DSP replay therefore differs even when gameplay and graphics match. It is
tracked as `beads-8wg.2.95`, with private full-state comparison artifacts retained.
The game-scoped streaming fix makes no audio-state replay claim and does not
change that shared serialization format.

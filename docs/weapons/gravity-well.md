# Gravity Well investigation checkpoint

Not implemented yet. Tracking `beads-8wg.1.43`; research branch
`feat/x3-gravity-well-port`, worktree `_wt_mmx_weapon_silk`, based on integrated
weapon commit `7779c10`. The worker stopped at the account usage limit before
making code changes.

Reference attacks are X's original X3 weapon ID 6. Zero will use adapted
original Zero body poses, with source projectile art and mechanics. User ROM
extraction remains mandatory.

Verified source findings:

- Normal projectile class `$0C` / group `$0F` starts at 4 pixels per update,
  decelerates by `$20`, forms a stationary well, then retracts toward X.
  Original well animation and phase traces are saved privately.
- Charged class `$15` uses X body group `$33`, sequence 35 (decimal), with an
  upward cast. The field lasts 300 simulation updates after the cast/orb
  ascent; the source keeps X in his casting action during this effect.
- These weapons' damage-table entries are **utility response IDs**, not HP
  damage: 0 immune, normal 1 pull/dissolve, 2 shake/dissolve, 3 shake-only;
  charged 4 lift, 3 shake-only, 5 special. `$82:DB3F..DDD2` implements the
  responses. Do not feed these bytes into the ordinary numeric damage adapter.

Private artifacts in that worktree's `_research` include `gravity_probe.py`,
`gravity-probe.json`, `gravity-probe-summary.txt`, and bounded player,
normal/charged, pull and victim/lift assembly notes. The private X3 source
process uses port 4397 and an isolated copy of fixture 6.

Agreed X1 adaptation: conservative ordinary-enemy eligibility, excluding
boss/protected stage actors; native body bounds distinguish movable victims
from large anchored actors. Exact bounds/mapping still need implementation
and documentation. Preserve original enemy art while their AI is gated and
the well moves/lifts them, using a renderer helper shared with Parasitic Bomb.
Eligible destruction uses native death/drop handling, with no invented direct
damage or imported boss weakness. Triad's `animation_sequence` extractor
option can select the original X upward-cast sequence. No save ABI change is
currently planned.

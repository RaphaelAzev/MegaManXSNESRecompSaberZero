# Parasitic Bomb investigation checkpoint

Not implemented yet. Tracking `beads-8wg.1.42`; research branch
`feat/x3-parasitic-bomb-port`, worktree `_wt_mmx_weapon_fang`.
The worker stopped at the account usage limit before making code changes.

The reference is X using native X3 weapon ID 2. Zero's use in X1 is an
intentional adaptation. Original USA ROM assets must be extracted locally.

Verified source findings:

- Normal class `$08` travels at three pixels per update. Noncapturable
  ordinary targets use raw damage 2 versus buster 3.
- Susceptible enemy response rows instead convert the target into a bomb,
  class `$1E`, keeping the target's original enemy artwork. Original responses
  include homing, stationary and rolling payloads.
- Charged class `$11` maintains four targeting cursors while fire is held;
  targeted cursors launch small homing units. Searching is free; source
  `$84:A556` spends energy when a unit launches. The controlled trace shows
  one-unit decrements; verify all launch paths before implementation.
- Normal code is around `$81:9700`, charged around `$81:AC80..AFxx`;
  reseed decoding at actual handler entries because linear dumps begin in
  dispatch data. Generic contact is `$84:CF38` onward.

Private artifacts in that worktree's `_research` include
`parasitic_probe.py`, `parasitic_controlled.py`, original screenshots,
`parasitic-normal-probe.json`, `parasitic-controlled.json`, and bounded
normal/charged/special-collision assembly notes. Source reference port 4385,
private fixture 6, explicitly uses X (`$0A8E=0`).

Proposed X1 adapter: conservatively capture live ordinary enemies (damage
categories 0..5), preserve boss/protected actor immunity, and keep native
enemy RAM pose/group/resource bindings while a saved weapon actor owns the
host slot/type. The shared enemy-update gate can suspend AI; reconstruct
original sprite pieces in the renderer while the payload moves. The renderer
helper should also support Gravity Well. Snapshot ABI changes are not planned.
Payload cleanup should invoke native lethal-hit/death/drop handling; X1 death
state 4 repurposes HP to 1, so persistent HP zero is not a valid death test.

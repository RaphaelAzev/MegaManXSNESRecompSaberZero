# Optional Modern Zero

Issue: [#61](https://github.com/mstan/MegaManXSNESRecomp/issues/61).
Central tracking: `beads-8wg.1.71`. Implementation branch: `feat/zero-modern`.

Both **Add Zero** and **X / Zero Co-op** expose **Zero behavior**. The default
is **X3 Behavior**, preserving the existing charge/buster/saber sequence.
Changing either dropdown updates the other. The game-local launcher provider
in `mmx_netplay.c` mirrors this preference; no recomp-ui changes are required.
An online room carries the active co-op option in its existing mod agreement,
and leaving the room restores the offline settings.

**Modern** replaces only Zero's buster with a direct saber swing. All native
X1 and imported X2/X3 special weapons remain available, including their normal
charge/upgrade requirements. X retains his original controller and buster.

## Combat and movement

- Press Fire with the buster selected to swing immediately. Holding Fire does
  not repeat automatically. A press near the end of recovery buffers another
  swing. Ground and air poses, blade sprites and hitbox arcs come from the
  user's X3 ROM; no new sprite artwork is introduced.
- Airborne swings retain native horizontal movement and left/right steering,
  including the jump's normal vertical arc. Ground swings still plant Zero's
  feet. Landing switches the saber back to the ground behavior.
- Jump or Dash can interrupt an ongoing swing. Up/Down also interrupt it,
  preserving native ladder-grab input. Landing alone keeps the swing active.
  Cancelling never refreshes the midair jump/dash allowance or changes the
  equipped special weapon. X3 Behavior retains its original attack rules.
- A swing has two contact opportunities, nine simulation ticks apart, worth
  three damage each when the native enemy accepts the hit. Boss invulnerability
  and reflection remain in force, so two contacts do not guarantee two hits.
  This is deliberate Modern balancing, not a claim about X3's original damage.
- The long hold/recovery portions are shortened by eight ticks overall while
  retaining the existing swing poses.
- Press Jump again in the air for a second jump. Landing or wall contact
  refreshes the allowance. Jump strength uses the native action-6 velocity,
  with the existing ceiling and water handling.
- Press Dash in the air for a horizontal dash, up to 18 ticks at native dash
  speed. Release Dash, reverse direction, or meet terrain to end it sooner.
  One air dash is allowed before the next landing/wall contact. The extra
  jump and dash remain available with special weapons selected.

## Integration notes

`MmxZeroMovementTick` runs at player-controller entry `$81:815C`, before the
special-weapon adapter. Native horizontal integration at `$82:823E` receives
the air-dash speed. The native dash controller uses action `$14` and a ground
precondition at `$81:8971`; the adapter supplies that precondition only during
an air dash and removes it after the controller. Real collision probes still
run. Jump initial values come from the action-6 row at `$86:B9C3`: vertical
speed `$0553`, gravity `$40`. Native dash speed is `$0375`.

The eight-byte `MmxZeroModernState` is stored per character context, including
jump/dash allowances, remaining dash time, input buffer and saber hit phase.
The co-op projection therefore cannot spend or reset another seat's mobility.
These fields are included in saves, rewind and rollback. Game save chunk v15
extends the prior 40-byte Zero state and 4,648-byte co-op state; v14 co-op saves
are expanded on load. Older Zero prefixes remain readable. Changing behavior
clears incompatible stored combat rather than resuming an X3 combo as a direct
saber attack. Renderer captures use v14 (solo) or v15 (co-op), retaining readers
for the older layouts.

Private fixtures and extracted ROM assets stay outside version control. Public
setup still asks for the user's X3 ROM and extracts assets behind the scenes,
as documented in [zero-port.md](zero-port.md).

## Focused validation

`mmx_zero_test` covers default X3 behavior, direct attacks, contact windows,
input limits and serialized Modern state. `MMX_ZERO_MODERN_TEST=1` selects the
ROM-backed checks in `mmx_state_tests`: actual jumps/dashes, native special
weapons, both co-op assignments and deterministic save/rollback replay.
`MMX_NETPLAY_POLICY_ROOT` exercises linked dropdowns, persistence and room
activation through the real mod catalog.

Timing and damage are initial playtest values. This is an adaptation using X3
assets, not an implementation of the full X4-X6 combo/move set.

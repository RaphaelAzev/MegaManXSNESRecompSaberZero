# Imported weapons and X1 elemental reactions

Owner-approved exceptions to the original neutral-boss-damage scope, tracked
in `beads-8wg.1.48`. No X2/X3 boss weakness tables are imported.

| Imported attack | X1 collision class | X1 reaction |
| --- | --- | --- |
| Speed Burner fireball / ground flames | `$0A` | Normal Fire Wave |
| Charged Speed Burner, outside water | `$13` | Charged Fire Wave |
| Triad Thunder normal orbs / links / released bolts | `$0C` | Normal Electric Spark |
| Charged Triad Thunder damaging quake / waves | `$15` | Charged Electric Spark |

The projectile keeps its imported graphics, timing, collision box and behavior.
Only its native collision class changes. `$84:9E15..9E45` uses this class to
select the X1 damage/immunity response and publishes it in `$1F1D`. This is
early enough for native enemy scripts to recognize fire/electricity after
the shared collision call. Changing only the final damage subtraction would
leave reflected/immune stage objects and scripted reactions broken.

Ordinary enemies still use the source weapon-to-buster ratio on X1's HP scale:
the adapter reads the neutral buster entry rather than multiplying the mapped
element's damage again. Speed Burner preserves a stronger native Fire Wave
hit on susceptible enemies. Boss/armored rows use the mapped X1 damage value
and their existing invulnerability/reaction logic. Speed Burner's underwater
bubble form is explicitly excluded from fire mapping.

## Verified X1 source paths

- Penguin's bunker actor `$57`, `$87:E354`: damage row `$11`, 16 HP.
  Its normal Fire Wave entry is 3, versus a reflected buster. `$87:E367..E371`
  enters state 8 at zero HP; `$87:E462` invokes native debris, map modification
  and cleanup. The building is not deleted by a host-side tile approximation.
- Chill Penguin fire reaction: `$81:B683..B69C` recognizes `$0A/$13` and enters
  the original burning response. Other native Fire Wave reactions are retained.
- Armored Armadillo actor `$14`, `$83:B387..B398`: an electric hit (`$0C/$15`)
  with armor still present (`+$33==0`) selects state 8/substate 2 and the native
  extended reaction timer. The ordinary Electric Spark weakness hit is 3 HP.
  Native AI owns the subsequent armor-break sequence.

## Focused validation

`MMX_WEAPON_STAGE_TEST=1` enables optional private stage-fixture checks in
`mmx_state_tests`. `MMX_IGLOO_FIXTURE` takes a copied save from the earlier
bunker report (`save5.sav`): walk right and fire Speed Burner until native bunker destruction,
for both X and Zero. Player invulnerability is held only in this test to stop
the fixture's flyers from interrupting the repeated firing sequence.

`MMX_ARMADILLO_FIXTURE` takes the existing private boss fixture. Restore its
armor flag, overlap an actual imported Triad bolt with the boss, and observe
the native collision: HP 32 to 29, state 8/substate 2, then native break motion.
These checks passed. The tests also accept `MMX_RIDE_FIXTURE` for copied UI
earlier Ride Armor report (`save4.sav`), capturing Zero boarding, riding, walking, punching and
exiting. The owner game is never controlled or loaded by this harness.

The old boss fixture can emit APU guest-clock synchronization warnings during
headless replay; these checks validate gameplay/renderer behavior, not audio.
Original imported SPC effects remain the separate `.46` follow-up.

The shared state menu and its OSD label slots from zero: `Slot 6 loaded` refers
to `save6.sav`. The earlier test fixture descriptions used one-based numbers;
use the actual filenames above to reproduce those older encounters.

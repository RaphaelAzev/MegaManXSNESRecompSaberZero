# Storm Eagle: Zero misses moving platforms, helmet capsule missing

Handoff for the co-op maintainer (mstan's agent). Written from the owner's
2.0.5-alpha netplay recording on 2026-10-03, without access to the ROM or the
generated code. Nothing here changes game behavior. The diagnostics added
alongside this note (see "What the next trace will show") are meant to close
the gaps listed below.

## Owner report

Storm Eagle stage, X3 behavior, P1 = X, P2 = Zero, two-machine netplay:

1. Zero cannot land on a class of floating platform (the hovering platforms
   near the cannon enemies). Earlier floating platforms in the stage do work;
   the owner believes those are a different object class.
2. Zero cannot ride the platform that moves up and down, which X uses to reach
   the E-tank (the owner's screenshot shows a grey pillar/column).
3. At the helmet capsule, with X alive and Zero fallen, the capsule did not
   appear. The owner played with the widescreen mod (16:9). The password-save
   mod was also enabled, so the helmet may already have been granted; this is
   unconfirmed.

Inputs: `mmx-20261003-190738-424.log` (session, build `2.0.5-alpha`) and
`coop-physics-20261003-190826-424-1.csv` (73,881 rows, host frames
34718..45398, stage 5, plus 42 rows of stage 18). The `.previous.csv` segment
was not supplied, so earlier parts of the stage are missing. The session ran
after `63ba29e` ("Fix co-op landing charges, Storm platforms ...") reached
`main`, but there are no published releases, so the exact build commit is not
confirmed. P1 (X) was the world anchor on every frame-end row.

The session log also shows one rollback fork (`RB POST FORK tick=3109`)
that recovered after 60 lockstep ticks. It is far from the frames below.

## Findings

Field notes: `y` is the body origin (`$0BB0`); X and Zero stand on flat floor at
the same origin, so feet line up. Body byte `$2C` (`$0BD4`, external contact)
is `04` while standing on an object and `80/81` against a wall.

### X boards moving objects freely; Zero almost never does

Counting `$0BD4` bit 2 going from clear to set while alive:

- X: about 40 boardings across this file (and 9 in the owner's earlier
  Storm Eagle file), landing at every falling speed up to the terminal
  `vy = -1472`.
- Zero: 2 boardings, at host frames 39083 and 40830. Both are the diagonal
  flying platform near X 459 / Y 1630, which X was already riding, and Zero
  was falling slowly (`vy` -512 and -320).

### Clean miss: the column at host frame 34800

X stands on the column at X 1371 (top at origin Y 902, 9 px above the floor
at 911). Zero, at X 1358 (13 px from X, well within the column), jumps and
comes back down while the column starts rising:

| host frame | X (on column) y | Zero y | Zero prev y | Zero vy |
|---|---|---|---|---|
| 34846 | 902 -> 901 | 891 | 889 | -640 |
| 34847 | 901 -> 899 | 894 | 891 | -704 |
| 34848 | 899 -> 898 | 897 | 894 | -768 |
| 34849 | 898 -> 896 | 900 | 897 | -832 |
| 34850 | 896 -> 895 | 904 | 900 | -896 |
| 34851 | 895 -> 893 | 907 | 904 | -960 |
| 34853 | 892 -> 890 | 911 (floor) | 911 | 0 |

On 34848 Zero is 1 px above the top; on 34849 he is 4 px below it, and he
never gains `$0BD4` bit 2. X keeps riding to Y 701.

On each of these frames the trace shows the co-op platform pass doing its job:
`contact_pass` 1 with P1 projected (X is carried up by the platform delta),
then `contact_pass` 2 with P2 projected. So the second-seat pass at
`$84:AB81`/`$84:AB56` is running for this object. The native contact simply
does not accept Zero.

### Other misses in the file (less conclusive)

- 39257: X lands on a rising platform at X 550; Zero is at X 598, 48 px away,
  and falls to his death. Probably just off the edge.
- 40976..41100: Zero drops off a wall at X 541..585, below a platform X rides
  at Y ~1394, and falls to his death. Probably out of reach.

### Ruled out

- Zero's collision boxes. `MmxZeroSetCollisionRom` patches `$86:A552`
  (terrain/damage `{0,FB,6,18,0,0,FB,6,21,8}` against X's
  `{0,FF,6,14,0,0,FF,7,17,8}`) and the dash box. The bottoms match
  (-5+18 = -1+14 = 13 damage, -5+21 = -1+17 = 16 terrain); only the top and
  the terrain half-width (6 vs 7) differ.
- A skipped second-seat pass (see above).
- Netplay correction: no fork near these frames.

## Hypotheses to test against the ROM

1. `$84:AB81` landing acceptance depends on something besides feet position
   that differs for the partner seat or for Zero: for example a hitbox
   pointer such as `$0BC8`, the half-width, `$0BAA` action, or previous
   position `$0BCA/$0BCC` compared with the item's previous top. X's
   successful landings at full falling speed suggest a generous window for
   X, so compare the inputs `$84:AB81` reads for X and for Zero on 34848/34849.
2. The column and the hovering platforms may not be items `$0E..$10`
   (`platform_hook` ignores other classes), or may run contact through a path
   other than `$84:AB81`/`$84:AB56`. The new trace logs every caller.
3. The item's rider latch `.2C` per-seat projection may not apply to these
   objects if they use `.2C` for something else.

`MMX_COOP_FOLLOWUP_TEST=1` already covers "both types of Storm support with
Zero alone and both riders". Extending it with this column case (a partner
falling onto a support that starts rising as he arrives) and with a
hovering-platform landing from a jump, for both rosters, would confirm or
rule out each hypothesis.

## Helmet capsule

Co-op capsules follow the world actor; only Chill Penguin's capsule has a
full acquisition regression (`MMX_COOP_CAPSULE_FULL_FIXTURE`), and the Storm
Eagle check covers dialogue only (`docs/coop-port.md`). In this file X idles
at X 3989..4033, Y 655, camera fixed at 3840/512 (host frames 44318..45398),
with Zero fallen. The trace did not record `$1F99`, so a helmet already
granted by password cannot be excluded. Worth checking: whether the capsule
spawn reads anything seat- or character-specific (Zero's
`MmxZeroUpgradeBits` hook at `$81:971C/9793/98FC`), and whether the widescreen
view changes when the capsule object is created.

## What the next trace will show

The Co-op physics diagnostics mod now adds to `logs/coop-physics-*.csv`:

- `upgrades`: `$1F99` on every row (answers the capsule question).
- `items`: every live item slot `slot:class:x:y:state0state1state2:2C` on
  frame-end and platform rows (answers hypothesis 2 and gives each platform's
  top next to Zero's feet).
- `platform-enter` / `platform-return` rows at `$84:AB81`/`$84:AB56` and their
  returns for every item slot, recorded before co-op swaps seats, with the
  seat's full body and the JSL `caller` on entry (answers whether Zero's pass
  reached the routine and what it left in `$0BD4`/`.2C`).

During netplay it also writes `logs/coop-netplay-*.csv` with per-frame ticks,
rollback state, inputs and WRAM/co-op hashes for locating forks.

Requested from the owner: one Storm Eagle run with the mod on, covering the
column, the hovering platforms and the capsule, sending the physics CSV, its
`.previous.csv` and both players' netplay CSVs.

## Maintainer follow-up (2026-10-03)

Hypothesis 2 reproduced against the source ROM: the later supports are native
item classes `$13/$14`, outside the existing co-op `$0E..$10` filter. Their
dispatch entries in `$80:F320` lead to `$83:F27D/$F360`. Both use the same
boolean `.2C` rider latch and call `$84:AB81/$AB56`:

- `$13`: landing caller `$83:F2D8`, side-contact caller `$83:F2E1`, bounds
  `$86:DB58` (paused landing caller `$83:F343`).
- `$14`: landing caller `$83:F3FC`, side-contact caller `$83:F405`, bounds
  `$86:DB98`.

The first seat could ride these supports, but the co-op hook never projected
their latch or retried contact for the partner. Contact rows for other nearby
items explained why the old trace appeared to contain both passes. The fix
includes `$13/$14` in that existing per-seat handling; it does not change Zero's
box or native landing thresholds. `MMX_COOP_STORM_LANDING_TEST=1` exercises a
falling partner beside a current rider on `$0F/$10/$13/$14`, in both character
arrangements. The new `$13` case fails before the fix and all eight cases pass
after it, including the continuously rising `$14` support.

The helmet report remains unconfirmed. The real capsule is item `$05`; its
initializer `$81:E4C7` reads shared `$1F99 & item.0B` and removes itself if the
upgrade is owned. Helmet uses mask `$01`. That check does not read either
player's health, Zero's virtual upgrade hook, or a netplay flag. The old
trace cannot establish whether that bit was set. The focused source-ROM check
with X alive and Zero fallen initializes an unowned helmet capsule and removes
an owned one; it verifies this gate rather than recreating the old world spawn.
Keep the new `upgrades` and
`items` diagnostics for a fresh approach if the capsule is still absent with
the helmet bit clear; do not force a duplicate capsule to appear.

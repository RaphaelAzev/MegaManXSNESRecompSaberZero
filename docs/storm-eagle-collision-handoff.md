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

## Update: second recording (2.0.6-alpha, 2026-10-03 21:39)

Inputs: `coop-physics-20261003-213919-424-1.csv` (host frames 8718..9372;
its `.previous.csv` was not supplied), `coop-netplay-20261003-213919-424-1.csv`,
`net_diag.jsonl` and the session log, all from the guest (slot 1).

### The column is item `$0F`, called from `$83:F18F` / `$83:F198`

The new `items` column shows twelve live slots, all class `$0F`, bobbing
(state byte 1 cycling `02/06` and `04/08`). The new platform rows show
`$83:F18F` calling `$84:AB81` (top contact) and `$83:F198` calling `$84:AB56`
(side contact) for every column, for both seats.

### Riding works, landing as the non-anchor seat does not

- Frames 8718..8814: X and Zero (11 px apart) ride the same descending column
  with `.2C = 03`. Both seats' `AB81` calls carry their rider by the column's
  delta (Y 791 -> 792 for both), and both gain `$0BD4` bit 2.
- 8838: X dies; anchor becomes Zero. Zero then lands on columns normally
  (8856, 8964).
- Owner observation: when X warps out, Zero immediately starts colliding with
  the elevating column again. So the failure depends on co-op, not on Zero.

### Register re-entry (trace hypothesis; superseded as the cause)

The maintainer follow-up below found the actual cause against the ROM
(items `$13/$14` missing from the filter). The column rows here were nearby
`$0F` items. The register change described next remains as a defensive
correction.

`platform_hook` opens the pass at `AB81`/`AB56` entry, and at the first seat's
return it saves the return registers, projects the partner and redirects to
the routine's entry. The partner therefore re-enters `AB81` with the first
call's *return* `A/X/Y/P/DB`, not with the registers `$83:F18F` passed in.

That fits every observation: an existing rider is carried from the item's own
fields (works with any registers); a new landing goes through the full contact
test, which would read the caller's inputs (fails as the second seat); with
the partner absent or dead there is no second pass, so the remaining player
gets the native call with the right inputs (works).

The fix saves the entry registers when the pass opens and restores them before
the second seat's redirect (host-only state, validated by item pointer and S,
invalidated on state load; the 4,664-byte co-op ABI is unchanged). The return
registers are still restored afterwards as before. The physics trace now
records `regs` (`A:X:Y:P:DB`) on every hooked row, so the next recording shows
both calls' inputs directly. **Not yet verified against the ROM.** Please
confirm with a ROM-backed landing case: partner falling onto a `$0F` column
and a `$10` flying platform while the anchor rides it and while it is empty.

`contact_hook` (`$84:9B03/9B43`) uses the same re-entry pattern; it is left
unchanged here.

### Lag in this session

- **Not TURN.** ICE selected `srflx` candidates on both ends (direct UDP via
  STUN); all 368 `net_diag.jsonl` samples report `ice_path=srflx`, `turn=0`,
  and no admit stalls (`stall_ms` 0).
- **The simulation ran slow.** 9,365 ticks in 186 s, about 51 ticks/s (dips to
  37..45 ticks/s from 78 s to 143 s). The owner's 2.0.5 session ran at about
  58 ticks/s.
- **Five rollback forks** (ticks 3977, 4086 `apu`, 4736 `wram`, 4807 `wram`,
  7865 `wram`), each followed by 60 ticks of lockstep. Lockstep waits on every
  remote input, which feels heavy.
- **The diagnostics were expensive.** The physics trace started at tick ~3300,
  just before the slow stretch, and filled a 32 MiB segment by frame 8718
  (~16 platform rows a frame, hex-encoded with one `snprintf` per byte, under
  Wine). The encoder is now table-based. A run with the mod off would isolate
  the remaining cost.
- To locate the forks, compare `wram_hash` in both players'
  `coop-netplay-*.csv` around those ticks. Only the guest's file was supplied.

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

## Update: register fix tested, still failing (2026-10-04)

The owner tested a local Linux build of `main` (`375b31d`: `$13/$14` filter
plus the register re-entry change). P2 still cannot land on the `$0F` pillars,
and also walks through their sides: `$84:AB56` side contact fails for the
second seat just like `$84:AB81` landing, while riding (carry via `.2C`) keeps
working and everything works once the partner is gone.

Next hypothesis: the first seat's call writes a byte in the item slot (for
example a "contact handled" flag) that the second seat's re-entry reads and
then skips its contact test. Only `.2C` is projected per seat today. The
physics trace now records the contacted item's full 48-byte slot (`slot`
column) on every platform row, so the first seat's entry/return and the
second seat's entry/return can be diffed byte by byte. ROM-side, it would
settle quickly by reading which item fields `$84:AB81/AB56` read and write.

The Linux build previously wrote no session log (stderr only), which is why
the owner's local run produced no `mmx-*.log`. It now follows the Windows
policy (`logs/mmx-*.log` beside the executable unless `logging.ini` Console=1
or `MMX_LOG_CONSOLE=1`).

## Update: the E-tank elevator is not in the item pool (2026-10-04)

Owner recording `coop-physics-20261004-014404-3699341-1.csv` (local Linux
build, host frames 6040..6802, stage 5). The new `slot` column shows no item
byte changing in or between the two seats' `$84:AB81/AB56` calls, so the
"shared item flag" hypothesis is ruled out for the platforms that do use them
(two class `$0E` slots, callers `$83:F124/$83:F12D`).

The decisive part is frames 6400..6800: X stands on the elevator at X 1363
(`$0BD4` bit 2 set) and rides it from Y 902 up to 701, while Zero stands at
X 1362 on the floor (Y 911) *inside* it, never gaining `$0BD4` bit 2. During
that ride there are no live item slots and no `$84:AB81/AB56` calls at all.
This is the same 902 -> 701 ascent as the first recording's "column" at host
frame 34800 (X at 1371), so that case was this elevator too; the `$0F` rows
logged around it belonged to other objects.

So the elevator is an object outside the `$1628` item pool, most likely an
enemy-pool object (`$0E68 + slot*$40`) with its own rider/side contact, and
co-op runs that contact for the world anchor only. The ship lift (enemy `$48`)
is the one enemy rider co-op already handles, by retrying its `$82:D7D7`
contact query from `$87:C0AE`. The elevator probably needs the same treatment:
a second-seat retry of its contact routine, with per-seat rider state.

The physics trace now adds an `enemies` column on frame-end rows (every live
enemy slot as `slot:class:x:y:state0state1state2:2C`), which identifies the
elevator's class and whether `.2C` latches its rider. ROM-side: find that
class's handler and the contact routine it calls, then extend the existing
second-seat contact pattern to it.

## Update: the elevator is enemy $59 (2026-10-04)

`coop-physics-20261004-015640-3707798-1.csv` (host frames 4714..5345) names
it. The new `enemies` column shows enemy slot 0, class `$59`, at X `$553`
(1363), the X that X rides at. Its `.2C` reads `$80` while empty and `$81`
from host frame 5138, when X boards; it then rises from Y `$399` while Zero
stays on the floor at Y 911. Classes `$5A` and `$58` share X `$553` and move
with it (parts of the same structure).

`$0BD4` bit 2 plus a bit-0 rider latch in `.2C` is exactly what `$84:AB81`
produces for the items, so the elevator very likely calls the same helpers
from the enemy pool. `platform_hook` and the platform trace rows both
filtered on the item pool, which is why no calls were logged and no second
seat pass ran.

Change in this branch: enemy-pool slots of classes `$58..$5A` now get the same
second-seat `$84:AB81/AB56` pass as the item platforms. Per-seat riders stay
in `.2C` bits 0/1 with the object's other bits (the elevator's bit 7)
preserved; for these classes a seat rode when the helper left bit 0 set. Item
platforms behave exactly as before. Platform trace rows now cover the enemy
pool too, with the 64-byte enemy slot in `slot`.

Open questions for the ROM: confirm `$59` (and `$58/$5A`) call `$84:AB81/AB56`;
check whether the elevator's ascent tests `.2C` bit 0 only. If it does, Zero
riding alone sets bit 1, not bit 0, so he collides and stands on it but may
not start the ascent; the fix would be to report "any rider" in bit 0 for this
class.

## Update: two riders jitter; the elevator does not call AB81 (2026-10-04)

`coop-physics-20261004-023918-3736094-1.csv` (host frames 4199..5020), after
the enemy-pool platform change. Both players now ride, but jitter: `$0BD4`
bit 2 alternates between X and Zero every frame and the elevator's `.2C`
never shows two riders (always `$81`). Zero alone landed and rode normally
(frames 4743..4758), so the single rider latch is being fought over.

The trace now covers the enemy pool at `$84:AB81/AB56` and there is **no**
call with D = `$0E68` (the elevator's slot), so the enemy-pool change in
`platform_hook` never ran; it is reverted. The elevator's contact is elsewhere.

Frame 4851: a seat switch to Zero happens with no co-op pass open (the
`select-before seat 1 kp0` row), then Zero is carried 877 -> 874 in a
`contact_pass` 1 pass with Zero current; on 4852 X is current for that pass
and is carried instead. So whichever seat is current when the elevator's
contact runs takes the rider latch, and something switches the current seat
on alternate frames before it.

The trace now labels every seat switch with the `mmx_coop.c` source line of
the `MmxCoopSelect` call (`caller` column, `L<line>`), and logs the enemy pool
on switch rows while an elevator part (`$58..$5A`) is live, so the next run
shows which hook switches seats and when `.2C` flips. ROM-side, the useful
question is which routine `$59` uses for rider contact (likely within the
enemy contact path co-op retries at `$84:9B03`).

## Update: boarding order decides who gets the elevator; fix (2026-10-04)

`coop-physics-20261004-110601-3947117-1.csv` (host frames 5783..6813; built
before the seat-switch labels). Not a regression from the revert: here **X
boarded first** (host frame 6131, `.2C` `$80` -> `$81`) and Zero then fell
through it to the floor (6206 on), as in the original report. In the jitter
recording Zero had boarded first. The elevator latches exactly one rider in
`.2C` bit 0, and the first boarder owns it.

Where the latch is set: on ride frames there are no item platform calls, and
X is carried by the elevator's delta (Y 889 -> 887 on host frame 6305) inside
the first `contact_pass` 1 pass, with X current. The only source of those
passes here is `contact_hook`, so the elevator's rider contact runs inside
`$84:9B03` (enemy body contact), which co-op already retries for the second
seat. With one latch, the second seat's call saw "already ridden".

Fix in this branch: for enemy `$59` at `$84:9B03`, co-op keeps one rider bit
per seat (in the formerly reserved `object_reserved` byte, now
`elevator_riders`: bits 0/1 per seat, high nibble slot + 1; the 4,664-byte ABI
is unchanged and older saves read 0). Each seat's call sees only its own bit
in `.2C` bit 0; afterwards bit 0 holds "anyone riding" for the elevator's own
ascent logic, and bit 7 is untouched. If the elevator clears bit 0 itself,
both stored riders are cleared at the next pass. Not yet verified against
the ROM; please confirm `$59`'s rider contact is inside `$84:9B03`.

## Update: the second seat's $84:9B03 call gets the wrong registers (2026-10-04)

`coop-physics-20261004-114220-3966696-1.csv` (host frames 3673..5158, merged
`main` with the per-seat elevator latch). Seat-switch labels confirm X is
carried by the elevator (Y 845 -> 843, host frame 4961) inside
`contact_hook`'s first pass (the select at the `contact_player^1` line), so
the elevator's collision runs in `$84:9B03`. But Zero, standing inside the
elevator's footprint at X 1367 / Y 911, is never pushed out, including frames
4800..4833 when nobody rides it (`.2C` `$80`). So the latch is not the whole
story: the second seat's `$84:9B03` call never collides with the elevator.

`contact_hook` redirected the partner's retry to `$84:9B03` with the first
call's return registers, the same flaw fixed earlier for `platform_hook`.
Ordinary enemy damage contact evidently reads only the enemy slot, but a solid
object's collision appears to use the caller's inputs. The partner now
re-enters with the registers the caller passed in (host-only, invalidated on
state load and reset). The trace adds `contact-enter`/`contact-return` rows
with D, registers and the enemy slot while an elevator part is live, so the
next recording confirms which slot `$84:9B03` serves and with what inputs.

## Update: the elevator collides inside its own update (2026-10-04)

`coop-physics-20261004-115107-3972177-1.csv` (host frames 6115..6893, with the
contact trace). Two findings settle where the elevator's player collision is:

- The elevator's slot `$0E68` only ever appears at `$84:9B43` (with entry
  registers identical for both seats); `$84:9B03` is never called for it. So
  the `$84:9B03` per-seat rider latch never ran, and the contact-register
  restore had nothing to fix for it. Both are reverted.
- On a rising frame (host 6609) X moves 896 -> 895 **before** any contact pass,
  between the object passes and the first `contact-enter`, and no contact call
  moves him. The elevator carries and collides with the player inside its own
  enemy update, once per frame, against whichever seat is current (normally X).
  Co-op has no second-seat pass for that, which explains every observation:
  Zero passes through it and cannot land while X is current, and works when X
  warps out and Zero becomes the current seat.

The fix belongs in a second-seat pass around the player-interaction routine
the `$59` update calls. The ship lift's rider query is `$82:D7D7`, so the
branch routes `$82:D7D7` through the interpreter when it is a generated entry
(optional in `apply_coop_hooks.py`; a build without it only warns) and logs
each call as `lift-contact` with D, registers, slot and JSL caller. ROM-side:
find what enemy `$59`'s handler calls to test, carry and push the player, and
give that call the same second-seat retry with per-seat `.2C` rider bits that
`platform_hook` gives the items.

## Update: the elevator asks $82:D7D7; second-seat query added (2026-10-04)

`coop-physics-20261004-121627-3985163-1.csv` (host frames 3621..4428), with the
optional `$82:D7D7` trace active. The elevator's slot (`D = $0E68`, class
`$59`) calls `$82:D7D7` once per frame from its handler, one call site per
state, always with X current and no co-op pass open:

| JSL return | elevator state (+1/+2) | frames |
|---|---|---|
| `$87:E857` | `02 00` | 1 |
| `$87:E8A6` | `04 00` (idle) | 37 |
| `$87:E97D` | `08 04` | 4 |
| `$87:E9C8` | `0E 04` (waiting at the bottom) | 342 |
| `$87:E9F9` | `10 04` (rising) | 94 |

`.2C` reads `$80` at every entry: the handler clears the rider bit and asks
again each frame, as the ship lift does. So no rider state persists across
frames, and only the current seat was ever asked.

Change in this branch: for class `$59`, when the current seat's `$82:D7D7`
call reaches its RTL, co-op restores the entry `.2C` and registers, projects
the other living seat and calls `$82:D7D7` again; at the second RTL it ORs
both answers into `.2C` (bit 7 preserved), selects the first seat back, and
returns the first seat's registers unless only the partner rides. The RTL is
found at run time: each `$6B` byte in the 1 KiB after `$82:D7D7` (LoROM
`0x157D7`) is hooked, and only the one executed with the entry's stack
pointer, slot and JSL return address completes the pass. Host-only state; a
pass left open is closed at the next frame. USA only. Not yet verified
against the ROM: please confirm `$82:D7D7` resolves contact for `$0BA8` (it
appears to: X is carried before any other contact pass) and that its RTL lies
in that window.

## Update: both ride, partner carried in the same frame

Retest with the `$82:D7D7` retry: Zero (partner) now collides with and rides
the elevator. X looked jerky on the way up. In
`coop-physics-20261004-124931-4005421-1.csv` the order inside a frame is: X's
query (no movement), Zero's query (snaps him onto the top, e.g. y 875→874),
then the handler moves the elevator (893→891) and carries only the projected
body, X (874→872). At frame end X sat at `el_y−19` on all 772 riding frames;
Zero did on 638 and trailed by 1–2 px on the other 134, catching up at his
next query. The two bodies were drawn a frame apart, which reads as jitter.

Change: when the partner's query sets `.2C` bit 0, co-op records the
elevator's position (slot +5 x, +8 y). At the end of the frame, before
capture, `MmxCoopLiftCarry` moves the partner's body by however far the
elevator moved since then (ignored above 16 px, as a warp). The caller now
always gets the first seat's registers back. The state is host-only and
cleared on reset and rollback.

## Update: the column part ($5A) also queries $82:D7D7

`coop-physics-20261004-130931-4015389-1.csv`: once the elevator reached the top
(y 720) and stayed there, Zero could not climb its sides or get back on. A
second part, slot 3 (`D = $0F28`, class `$5A`, same x, 83 px below the top),
calls `$82:D7D7` every frame (792 calls), always for X only. It is the
column's body. X stopped against its left side (x 1342 = el_x − 21), while Zero
walked through it (x 1347–1392) and so had no wall to kick off. The getting hit
was incidental: Zero had jumped off the right edge.

Change: the `$82:D7D7` second-seat retry now covers class `$5A` as well as
`$59`. The frame-end carry stays tied to the `$59` top (its own slot is now
recorded, so a later `$5A` pass doesn't redirect it).

## Update: ride jitter is the shared camera; wall-slide seam

`coop-physics-20261004-132512-4023531-1.csv`: the top and sides now collide for
both seats. On the ride both bodies end each frame exactly at `el_y − 19`; the
elevator rises 2,1,2,1 px. The shared camera (`camera_hook`, the midpoint of
both stored bodies) runs late in the frame, after the elevator has carried X
but before the frame-end partner carry, so it averaged this frame's X with
last frame's Zero. The camera lagged a frame, and X's screen y alternated
95/96. The partner carry now runs at the column's (`$5A`) `$82:D7D7` entry,
right after the top has moved, and again at the camera hook; the frame-end
call remains as a fallback.

Wall slide on the column's left side (state `$12`, holding right): both seats
drop to falling (`$08`) for 1–2 frames at `el_y + 21` (X) / `+29` (Zero), then
catch again. For X, the unmodified first-seat query, the top part (`$59`)
pushes him down out of its bottom edge (y 735→738→741) and stops pushing him
sideways, while the column only starts pushing at y 742. That is a seam
between the two parts' boxes in the native routine, not a co-op path. Needs a
single-player comparison to confirm it is original behaviour.

## Update: helmet capsule still absent (netplay, Zero fallen)

`coop-physics-20261004-140424-4045650-1.csv` (+ `.previous.csv`, host frames
10909..14624, online co-op, Unified cameras). Zero (seat 2) fell into a pit at
host frame 11772 (y 1089) and stayed fallen. X reached the same spot as in the
first report: camera fixed at 3840/512, X idling at x 3981..4071, y 655
(frames ~13780..14624).

- `$1F99` is `$18` on every row, so the helmet bit (`$01`) is clear: the
  capsule's own "already owned" gate should not remove it.
- The item pool held no capsule at any point in the room. In the whole
  capture, only one item of class `$05` appears, for a single frame
  (11023, x `$0750` y `$0288`, state `01 00 00`, while both players were
  alive), far from the room. It vanished the next frame.
- The enemy pool near the room holds only two stale class `$46` slots.

So in both reports the capsule never spawns while Zero is fallen. That
points at the world spawn (or the room's capsule trigger), not the `$81:E4C7`
initializer. Still unknown: whether it spawns in co-op with both alive, and
which pool and trigger the room's capsule uses. That needs the stage's object
table / spawner code read against these coordinates.

## Update: the helmet capsule is a camera-scroll spawn ($4D in the enemy pool)

`coop-physics-20261004-141547-4053487-1.csv` (online, both alive): the capsule
is enemy class `$4D` (slot 0, x `$0FC8` = 4040, y `$02D0` = 720), not an item.
It did not spawn when the camera arrived (3765/431 → down to 3765/512 at X's
drop, x 3894, then right to 3840/512 at frame 8064), although at 3840/512 the
capsule sits inside the screen (200, 208). It spawned at frame 8432, on the
first frame the camera scrolled down again (454 → 458) after a wall jump had
pulled it up to y 454. So the spawn is edge-triggered by camera scrolling, and
co-op's arrival path (shared camera = midpoint of the two bodies; Zero 2–6 px
behind X) never crossed the trigger, presumably by a pixel or two of margin.
The first report's camera path (Zero fallen) also never re-scrolled.

Also seen: from the spawn onward the camera alternates 3840/3843 every frame.
And in the dialogue screenshot, only one peer draws Dr. Light's hologram.

Next: read the enemy spawner's edge windows (horizontal and vertical scan
margins) for this room and compare with a solo arrival.

## Update: capsule cause is widescreen spawn ownership, not co-op

With widescreen spawning on, kind-3 records are allocated only by the early
wide DC36/DCDB pass (anchor = native + margin + 32), and the native pass
rejects kind 3 (`MmxWidePolicy_SpawnRecordAllowed`). The wide cursor crosses
the helmet capsule's column (x 4032..4063) while the camera is still in the
upper corridor (y ≈ 431). DCDB's height test rejects the record at y 720, and
the column is never scanned horizontally again. The vertical scan at the
corridor drop covers only to cam + 256 (3765 + 256 = 4021), short of the
column. It finally spawned on a later vertical scroll at camera x 3840. A 4:3
native anchor reaches the column only after the camera has dropped to 512.

Fix: `$4D` (Dr. Light's capsule) joins bosses and streakers as native-pass-only
kind-3 records, restoring the authored timing in every stage. Covered in
`tests/mmx_wide_policy_test.c`. Assumes the capsule record is kind 3; it lands
in the enemy pool, as kind 3 does. A workaround on older builds:
`SNESRECOMP_WS_SPAWN=0`.

## Update: capsule confirmed fixed; boss-lift lockup (open)

The owner confirmed that the helmet capsule now spawns on arrival.

New: `coop-physics-20261004-143825-4064456-1.csv` + `mmx-20261004-143812-4064456.log`
(online, Unified, both alive). Both players dash-jump onto the ship lift
(enemy `$48` at `$1840,$0160`). At host frame 14874 X lands first: `.2C` = 1,
`eagle_lift_hook` begins scene transport (scene owner X), X takes body lock
`$46`, and Zero is still airborne (action `$08`, one frame behind). Zero's
teleport frames run (14874..14882). The native frame runs once more at 14883
(X `46 00` → `46 04`), and then no player or object updates run again. The
lift stays in state `02 02` for the rest of the capture. The log shows why:

    [brk] architectural BRK at $32:0000 ...
    edges: $80F3B4>$81EC50(aot_call) $000000>$81EC98(entry) $81EC9D>$828398(aot_call)
           $80F3B4>$50D2ED(external) $50D2ED>$320000(return) $320000>$00FFAC(vector)

So the object dispatcher at `$80:F3B4` called `$81:EC50` (the airship door's
handler: the `door_hook` sites are `$81:EC98/ECC6/ECC7`), and then dispatched to
the garbage address `$50:D2ED`. `door_hook` takes no action while a scene owner
is set, so `$81:EC98` here is only the interpreter entry. Not yet determined:
which slot's handler pointer or state index was corrupt, and whether this
predates the `$82:D7D7` routing on this branch (it is in `OPTIONAL` and
interpreted now; its RTL observers act only during a class `$59/$5A` pass).
`MMX_COOP_EAGLE_FIXTURES` covers this lift with two riders landing together.
This capture differs: the partner lands one frame after the scene began.

Also reported, without trace coverage: in the ship interior (end-of-stage
ladder tube and the entry port before the boss door), player sprites show on
the wrong layer or disappear (custom renderer, `new_renderer=1`).

## Update: `$82:D7D7` routing is not the boss-lift cause; canister regression

9d4c006 gated `$82:D7D7`'s interpreter routing to the E-tank elevator slots.
The owner's retest (`coop-physics-20261004-151358-4079217-1.csv`) still locks at
the ship lift. Same pattern: the lift reaches state `02 02` at host frame
10704, Zero's scene-transport frames run, the native frame runs once (10713,
world tick 45→46), and the world tick never advances again. The owner also hit
it earlier with X fallen and Zero riding alone (no scene transport, no partner
retry), on the build that routed every caller. The lift locks with the routine
both compiled and interpreted, so the routing is not the cause.

The gate itself regressed: with `$82:D7D7` compiled for every other caller,
weapon shots stop hitting Storm Eagle's destructible flame canisters. So those
canisters use `$82:D7D7`, and the compiled routine behaves differently from
the interpreted one for them. The gate is reverted (always interpreted under
co-op, as in f24c7a8). Worth checking: whether canisters take hits on `main`
before this work, with co-op on and off. If they don't, the generated
`bank_82_D7D7` has a translation fault the interpreter avoids.

Next for the lift lock: reproduce solo (co-op off), offline co-op, and on a
build before this branch's elevator work (`8ce45fd`).

## Update: boss-lift crash is the interpreted airship door state `$81:EC98`

The owner ran the stage solo (co-op off): everything works, lift included.
In co-op, with X fallen and Zero riding alone, both peers crash identically:

    [brk] architectural BRK at $32:0005
    edges: $80F3B4>$81EC50(aot_call) $000000>$81EC98(entry) $81EC9D>$828398(aot_call)
           $80F3B4>$50D2ED(external) ...

`$81:EC98` is a co-op `TARGETS` entry (door_hook's `$81:EC98` pass). With co-op
enabled, the generated code hands it to the interpreter unconditionally.
Interpreted during the lift ride, the airship door's `$81:EC98` state leaves
the dispatcher at `$80:F3B4` jumping to `$50:D2ED`. The generated state (solo)
does not. In both crash captures door_hook had nothing to do there (scene
owner set, or partner fallen).

Change: `tools/apply_coop_hooks.py` takes per-entry `POLICIES`. The doors
(`$81:E70D`, `$81:EC98`) enter the interpreter only when door_hook can open a
pass (`door_route`: co-op active, no menu/scene owner, no open door pass,
partner alive, and for EC98 `$1F41` clear). Otherwise they run generated, as
in single player. Ordinary co-op door crossings keep their existing path.
Underlying interpreter/generated mismatch at `$81:EC98` not yet isolated.

Canisters (owner retest, co-op, both players together): shots still pass
through them with `$82:D7D7` interpreted again, so 9d4c006 was not their
cause either. Solo is fine. Open; the physics trace has no shot/enemy-HP
fields to show it yet.

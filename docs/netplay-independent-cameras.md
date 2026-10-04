# Independent cameras for online co-op

Online co-op gives each peer the view of its assigned session seat. Couch
co-op retains the shared camera and its horizontal separation limit. This
policy is automatic when the existing co-op mod runs in an active netplay
session; there is no additional mod or launcher setting.

A living player transported out for a scripted scene immediately watches the
scene owner's camera, including departure and arrival. The existing native
scene runs once. The partner returns beside its owner with personal stats
preserved. A fallen player watches the survivor and is never revived by scene
transport. A voluntarily withdrawn player also watches the remaining player.

The reference is Sonic 2's campaign viewport implementation in
`../segagenesisrecomp/SonicTheHedgehog2Recomp`, particularly
`game/sonic2_runtime.c`, `game/sonic2_video.c`, and its
`docs/CAMPAIGN-VIEWPORT-PROTOTYPE.md`. Both games retain one deterministic
simulation and select a local view only for presentation.

## Ownership and rendering

`src/mmx_coop_view.c` derives the non-anchor view from that player's body,
clamped to authored camera limits. The native camera remains the world
anchor's camera and drives shared scripts. On scene handoff both views use
that native camera immediately; there is no separate camera convergence wait
before placing the returning partner.

The bounds are `$1E56` minimum X, `$1E58` maximum X, `$1E5A` minimum Y and
`$1E5C` maximum Y. The native origin is `$1E4D/$1E50`. A second view follows
body X minus 128 and body Y minus 160. Online motion retains stage bounds and
native pit death, rather than the couch co-op 224-pixel tether.

`MmxBeforeFrame` sets the common online policy from `snes_netplay_active()` and
co-op activation. `snes_netplay_local_slot()` selects the renderer's seat.
The local controller/device index is not the session seat and is not used
for this decision. Builds without netplay select the shared view.

`src/mmx_renderer.c` reprojects world pieces, both bodies, teleport beams,
weapon effects and fallback OAM, keeping HUD coordinates anchored. Online
views use the pre-OAM expanded queues so an object outside the anchor's
hardware screen can still appear on the other peer. Foreground tiles come
from the complete ROM-backed map rather than circular VRAM history. BG2
follows the original `$00:DF08` modes: half/half, full/full, half/full and
full/half. Script/actor surfaces retain their shared scene coordinates.
Spark's light windows and Launch's water plane account for the local origin.
Rendering does not write WRAM or select a simulation actor.

## Shared world activation and native AI

Native spawn records remain authoritative. `$F800` is the 32-pixel-column
pointer table (256 columns plus the terminal pointer at `$FA00`). Flag
entries begin at `$FA02` and contain a live byte, Y word and ROM descriptor
word; descriptors in bank `$85` contain type, Y, ID, subtype and X.

After `$00:DC36` finishes its ordinary scan at `$DCD7`, an online rescan calls
the original `$00:DCDB` allocator for records in either player's rectangle.
The hook at `$DD2D` filters individual records. Mechanisms and boss encounter
controllers use native entry timing; ordinary enemies use spawn hysteresis
and the agreed aspect margin. The scan does not allocate the space between
two distant views. Allocation flags, object pools and collected-item flags
remain native.

A rollback-owned visit bitmap prevents a defeated enemy from being allocated
again during a stationary rescan or the anchor's later native scan. Leaving
both activation windows rearms an ordinary cleared record. Native collected
flags remain intact. The original pool capacities still apply; this change
does not increase enemy, projectile or item slot counts.

The common lifetime/visibility helpers at `$82:806E`, `$82:808F` and
`$82:80B4`, plus Ride Armor's private `$83:8948` cull, test the union of
complete rectangles. They cannot combine one player's X range with the
other player's Y range. This keeps distant actors alive without activating
the empty gap between players.

Native enemy and enemy-projectile updates temporarily project the nearest
living player's body for AI. Enemy loops `$00:D4EA/$D507` and projectile
loops `$00:D48D/$D4AA` restore the caller's actor at their balanced loop
continuations. Contact retries remember the first projected seat and test
the other seat, preserving the native result and actual Slimer capture owner.
Shared scene/menu updates retain their existing owner.

Imported X2/X3 projectiles query their creator's derived simulation view for
viewport lifetimes and screen effects. They never use the local peer seat.
`tools/apply_coop_hooks.py` routes compiled routine boundaries through the
existing interpreter hook path only when required; uncompiled native helpers
already use that path.

## Save and rollback state

MMX extra state version 16 appends `MmxCoopViewWorldState` after the existing
co-op contexts. It contains the spawn visit bitmap, stage initialization and
balanced actor/contact continuation ownership. Older snapshots load with an
empty bitmap, which is rebuilt from native live flags on the next scan.

The online-session gate, local view seat and presentation offsets are excluded
from simulation state. Both peers simulate the same regions and receive the
same actors regardless of which view they display. Reset and stage entry
clear the visit history.

## Validation

Focused checks use owned ROMs/assets and private scratch directories:

- `MMX_COOP_VIEWS_BOOT_TEST=1`: retail boot into Highway; P1 waits at X 128
  while P2 reaches X 742. Both retain 16 HP. Native enemies update visibly
  outside P1's activation rectangle. Enemy AI, spawning and shots replay from
  a snapshot, and a cleared remote enemy is not reallocated while both views
  remain stationary.
- `MMX_COOP_VIEWS_TEST=1`: separated movement, distinct rendered views,
  read-only presentation, horizontal/vertical rectangle policy, snapshot
  replay, and actual departure/return from a distance with either scene owner.
  Camera handoff occurs on the first departure frame; HP survives the return.
- `tools/test_netplay_pair.py --independent-views`: two actual local UDP/lobby
  peers, 480 frames, separation beyond 250 pixels, matching positions and HP,
  and a delayed input change causing rollback on both peers. Each peer saves
  its own rendered view. The test uses isolated installations and ports.
- Existing renderer unit checks and Slimer ownership/replay checks pass.

Highway captures were visually reviewed for both rosters. Vertical window
policy is checked separately; this is not a full vertical-stage or campaign
qualification. Boss doors, capsules, room scrolls, unusual stage mechanisms
and two-machine internet play remain important owner playtest targets.

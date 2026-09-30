# Couch co-op implementation notes

Tracking: `beads-8wg.1.34`. Branch: `feat/couch-coop`, worktree
`F:/Projects/snesrecomp/_wt_mmx_coop`. Base: weapon mods merged through PR #53
at `7e2dcc3`. Scope and owner decisions are in
[the roadmap](zero-weapons-coop-roadmap.md#next-separate-simultaneous-couch-co-op-mod).

## Roster and release boundary

P1 chooses X or Zero; P2 receives the counterpart. The brief fixed P1=X/P2=Zero
answer was withdrawn. Character choice belongs to the co-op settings. SELECT
exchange is disabled. Co-op and the single-player exchange package must be
mutually exclusive. The original X3 ROM path is shared with the Zero/X3 weapon
packages; extraction happens internally. No source ROM or extracted art belongs
in Git or the eventual downloadable mod.

The initial code is a **development foundation**, not a playable co-op release.
It has no launcher activation package yet. A private ROM-backed test enables
it directly. Do not announce joining, dual rendering, damage, pickups, camera,
death, menus or transitions as complete based on the controller test.

## Source findings (X1 USA, 2026-09-29)

The player controller is `$81:812E`. It saves P and D, sets DP=$0BA8, and begins
its body at `$81:8136`. Its common epilogue is `$81:819C` (PLD, PLP, RTL).
Re-entering at `$81:8136` from that epilogue can run the second player's
controller under the existing guest stack frame. Restore P1's post-controller
registers before the one shared epilogue; preserve elapsed guest cycles.
Do not invent a nested interpreter call without a matching guest return frame.

This runs only the player routine twice. It does **not** run the scheduler,
enemy AI, stage events, DMA, or audio frame twice. Additional ownership passes
are still required for collisions, projectiles, player effects and drawing.
The boundary hooks use the interpreter. `apply_coop_hooks.py` routes generated
entries for this one routine through the existing paired/dispatch bridge when
co-op is enabled, preserving the caller's real JSL frame. Child routines keep
their normal generated implementations. Stock and exchange mode retain their
original path.

| Native location | Ownership and handling |
| --- | --- |
| `$0BA8..0C37` | Player body/controller, 144 bytes; includes current HP, charge and input |
| `$0C38..0C97` | Player armor objects |
| `$0C98..0E17` | Player charge/small effect objects |
| `$0E18..0E67` | Ride armor; stage object, not copied into player context |
| `$0E68..1227` | Enemy pool; world state, not copied |
| `$1228..1427` | Eight 64-byte native player projectiles |
| `$1F83..86` | Four subtanks; low nibble is reserve, high bits shared unlocks |
| `$1F87..96` | Eight X1 weapon energy words; retain shared high unlock bits |
| `$1F99`, `$1F9A` | Shared upgrades and maximum HP |
| `$7E:FFC0..FFC5` | Native configurable action button masks |

In particular, copying eighteen bytes beginning at `$1F85` as “buster plus
weapons” would overwrite two subtanks. Native buster selection has no weapon
energy word. Subtank UI source: `$00:CD16..CD37`; pickup unlock source:
`$81:E643..E65D`.

Native action input conversion is `$00:E543..E5F6`. It copies previous actions
from `$0BDE..DF` to `$0BE0..E1`, maps the buttons through the six configured
masks, and writes new press edges to `$0BE2..E3`. The co-op adapter applies
that mapping to seat 2, preserving native in-game control configuration.
Seat inputs come from `RtlGetPadState`, with the engine's 12-bit button format;
they must not be read directly from SDL. `RtlRunFrame` packs P2 at bit 12.

The existing shared SNES launcher profile already allows two players.
MMX's desktop-host descriptor omitted `num_players`, so it advertised one.
USA now advertises two; JP remains unchanged. No recomp-ui fork is needed for
the controller assignment cards.

## State and remaining integration

Validated controller checkpoint: private ROM-backed checks pass with scheduler
bounce both disabled and enabled. Each setting checks both roster orders,
P2 walking while P1 stays still, P1 walking while P2 jumps, one world-counter
advance per frame, and byte-exact full snapshot replay with both input streams.
The existing Zero unit test also passes. These checks do not cover the remaining
systems listed below and do not make co-op ready for playtesting.

`MmxCoopPlayer` owns native body/effects/projectiles, per-player weapon energy,
subtank reserves, Zero combat/animation state, imported weapon state, and input.
World progression and unlocks remain in native RAM. Switching the projected
player also updates the existing character collision-table patch.

The game save tail advances to MMXT v14 only when co-op is enabled. Stock,
exchange-only and weapon saves retain their existing v3/v8/v13 layouts. The
tail includes both players and the controller continuation metadata. Legacy
saves reset the co-op context; normal public loading must still enforce the
mod-set compatibility policy. State storage alone does not establish netplay
compatibility.

Remaining integration, in order:

1. Independent native movement and exact save/replay for both roster orders:
   **controller checkpoint passed**, including generated dispatch.
2. Advance and draw both players' effects/projectiles; resolve shared enemy
   fractional damage and cross-player time/freeze effects.
3. Check body damage and pickups for either player without duplicating enemy
   AI. Collector alone receives HP/energy; shared unlocks remain shared.
4. Add safe Start-to-join, source teleports and world freeze. Reject unsafe
   ground with the original error sound. No rejoin after death.
5. Draw both characters with source art and the fixed four-column HUD. Preserve
   native foreground priority; no blanket sprite priority override.
6. Independent pause inventory and subtanks; shared camera and boundaries;
   one-player death, team wipe/checkpoint, boss doors and cutscene ownership.
7. Add the mutually exclusive launcher package and P1 character choice,
   build a playtest executable, and run focused two-controller acceptance.

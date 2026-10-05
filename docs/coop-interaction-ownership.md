# Co-op interaction ownership

This is an index of every place co-op decides which player a game interaction
belongs to. It exists so these rules can be reviewed together later. In
particular, some interactions that now follow "the player who drives the
world" should probably follow X specifically: Dr. Light's capsules, and
whatever else the review decides.

All hooks are in `src/mmx_coop.c` unless noted. Addresses are the USA ROM.

## Terms

- **Seat**: player 1 or player 2 (`state.players[0/1]`). Either seat can be X
  or Zero (`character`).
- **Current**: the seat whose body is projected into `$0BA8` right now. The
  game only knows one player; co-op swaps bodies in and out with
  `MmxCoopSelect`.
- **Anchor, or world actor**: the seat that drives the world (`state.anchor`).
  Its pass runs the native scripts, camera, scenes and stage logic. It is
  normally P1. It moves to the other seat when:
  - the anchor dies and the partner is alive (`select_world_survivor`);
  - the other seat drives a door or the ship lift (see below).
- **Partner**: the seat that is not the anchor (`state.anchor^1`).
- **Retry, or second pass**: co-op lets a native routine run for the current
  seat, then selects the other seat and runs the routine again from its
  entry. This is how enemies, platforms and pickups react to both players.

None of these rules check `character`. "World actor" never means "X".

## 1. World actor only

The partner never triggers or advances these.

| Interaction | Where | Rule today | Review note |
|---|---|---|---|
| Dr. Light capsules (enemy `$4D`) | `contact_hook`, `capsule_slot()` | No partner retry of `$84:9B03/9B43` for a `$4D` slot. Only the current (anchor) pass touches the capsule. Added after Zero's touch advanced the helmet capsule from dialogue to upgrade mid-text. | **X-only candidate.** Today, if X is fallen, Zero is the anchor and can take X's armor. |
| Scripted scenes: boss intros, capsule dialogue, stage events | `scene_tick` → `begin_scene` | Starts when the anchor's world flags show a scene (`$1F0C`, `$1F23`, `$1F48`, or `$0C16` with `$1F31/$1F3B`). The partner beams out and returns beside the anchor afterwards. | Review per scene: capsule and Dr. Light scenes are X's. |
| Capsule acquisition and armor demo | `scene_tick` comment (`$87:CD20`, `$87:CE06`) | The scene stays owned by the anchor through the recorded-input demonstration. | **X-only candidate** (same as capsules). |
| Native camera and room scroll | `camera_hook` (`$00:DEA0/DEAB/DEBF/DECA`) | Unified view: the shared camera follows the midpoint of both bodies. The camera itself runs once, in the anchor's pass. Online Independent views: see `docs/netplay-independent-cameras.md`. | No change expected. |
| Pit death below the camera | `camera_hook` (`$00:E12D`) | The native bottom check covers the current body. The same threshold is applied to the partner. | Both seats; listed here because it runs in the anchor's camera code. |
| Enemy spawning and stage scripts | `view_world_hook`, `MmxCoopViewsSpawnWorld`; native `$00:DC36/DCDB` | One simulation. Spawns follow the native camera (anchor). Online Independent views also admit records in the other view. | No change expected. |
| Stage death and checkpoint | `death_hook` (`$00:9D9E`, `$00:9AC7`, `$81:8A5C..8B0B`) | A death with a living partner runs the native death pose, but the partner keeps playing (`solo_death`). A whole-team death resets to P1 at the checkpoint. | No change expected. |
| E-tank elevator carry | `lift_rtl_hook`, `MmxCoopLiftCarry` | The handler carries the projected body; co-op carries the partner by the same distance. | Both ride; not ownership. |
| Sprite priority (`.11` bits 4-5) | `MmxCoopSyncPriority` (frame end) | Stage sections write the priority to the world actor once (Storm Eagle's ship sets priority 3). The living partner copies the world actor's bits every frame. | No change expected. |

## 2. Both players, through a partner retry

Each player gets the native result. Behaviour is the same for X and Zero.

| Interaction | Where | Rule today | Review note |
|---|---|---|---|
| Enemy body contact (damage, solid enemies) | `contact_hook`, `$84:9B03` (returns `$84:9B42`) | Retried for the partner after the current pass. The first seat's registers are kept unless only the partner hit. Excludes Dr. Light's capsule (`$4D`). | Check other one-shot story objects here (anything that advances a script on touch). |
| Projectile contact scan | `contact_hook`, `$84:9B43` (returns `$84:9B7D`) | Retail stops at the first contact, so the partner is scanned only after a real miss. | — |
| Slimer capture | `contact_hook` (`slime_owner`), `slime_hook` (`$83:A934/A939`) | Whoever the puddle catches owns its pin and escape states. | — |
| Item platforms `$0E/$0F/$10/$13/$14` | `platform_hook`, `$84:AB81/AB56` (returns `$84:AC34/AB80`) | Top and side contact retried for the partner, with the caller's entry registers. `.2C` rider bits are kept per seat. | — |
| Storm Eagle E-tank elevator (enemy `$59` top, `$5A` column) | `lift_contact_hook`, `lift_rtl_hook`, `$82:D7D7` | The rider query is retried for the partner and both answers are OR-ed into `.2C`. The handler gets the first seat's registers back. | — |
| Armored Armadillo minecart (enemy `$2B`) | `lift_contact_hook`, `cart_hook`, `$82:D7D7`, `$88:9821..9867` | Query each player, move the cart once, then apply its native carry code separately to both riders. `.2C/.38` retain per-seat rider bits in WRAM; bit 0 remains the native "any rider" test. | Old single-rider snapshots are imported on first contact. |
| Pickups (health, energy, Sub Tanks, Heart Tanks, 1-ups) | `pickup_hook`, `$84:9C0E..9D06`, `$00:D2E6..D31B` | Contact retried for the partner. The first seat to touch an item owns it (`pickup_owner`) and receives its effect. | **Review:** Heart Tanks and Sub Tanks are shared progression. Decide whether Zero may collect them, or only X. |
| Player-terrain and object helpers | `object_hook`, `$00:D2BD..D47F`, `$81:9D67` | The anchor's helper call is replayed for the partner, then the anchor's registers are restored. | — |
| Dash effects | `dash_effect_hook` (`$81:9C86`, `$80:F478/F47C`) | Each seat's dust and effects belong to that seat. | — |
| Enemy AI target | `view_actor_hook` (`$00:D4F6/D515/D499/D4B8`, returns `D4F9/D522/D49C/D4C5`) | Online co-op: each enemy's update runs with the nearest living player projected, so it aims at and reacts to that player. Couch co-op: the native current body. | Includes story actors; check capsule/Light objects if online behaviour differs. |

## 3. Either player triggers, and that player becomes the world actor

| Interaction | Where | Rule today | Review note |
|---|---|---|---|
| Doors and boss doors | `door_hook`, `$81:E70D` (to `$81:E724/E725`) and `$81:EC98` (to `$81:ECC6/ECC7`) | The door's contact runs for both seats. The seat that opens it becomes the anchor, and the partner is transported through. Interpreter routing is gated by `door_route()`: only when the hook can open a pass. | **Review:** should a boss door wait for X, or any player? |
| Storm Eagle ship lift (enemy `$48`) | `eagle_lift_hook`, `$87:C0AE/C0B4` | Either seat's contact starts the ride. The actual rider becomes the anchor, and the partner beams out. | Same question as doors. |
| Pause / weapon menu | `menu_hook`, `$80:9E68/9EAC/C579/E57F` | The player who presses Start owns the menu until it closes. | Per-player by design. |

## 4. How the world actor is chosen

- Stage start, checkpoint and death reset: P1 (`MmxCoopFrameTick`, `death_hook`).
- Anchor dies with the partner alive: the partner becomes the anchor
  (`select_world_survivor`). This is why "world actor" can be Zero.
- Door or ship lift driven by the other seat: that seat (sections 3).
- Nothing selects the anchor by `character`.

An occupied Ride Armor keeps its pilot as the anchor. The native boarding
action `$2C` identifies the owner, including in older saves made after a
partner's return selected the wrong actor. Returning P1 does not take control
away from a seated P2; either character can pilot. Both native pilot groups
`$6A/$6B` use the adapted Zero cockpit pose.

During pickup refills, both controllers pause along with native terrain
collision. Heart Tanks leave `$1F19` clear, so their active upgrade task
(`$0B`, state `2/6`, with `$1F13/$1F16` paused) is recognized separately.
This check precedes cutscene detection: an airborne collector's saved action
must not start partner transport.

## Making an interaction X-only

The rows marked as candidates need a decision about Zero when X is not
available (fallen, withdrawn, or Zero is P1):

1. Leave the object inert until X is alive and present.
2. Let Zero trigger it as today.
3. Hand the interaction to X's seat even when Zero is the anchor. This needs
   an explicit seat selection around the object's contact, like
   `slime_owner`, rather than `state.anchor`.

The check would be `state.players[seat].character==MMX_COOP_X`, applied where
each row's hook decides which seat runs.

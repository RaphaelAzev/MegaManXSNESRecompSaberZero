# X2 Strike Chain backport

Tracking: central Beads `beads-8wg.1.40`, under weapon-port issue
`beads-8wg.1.32`. Implementation branch: `feat/x2-strike-chain-port`.

X2 weapon ID 6 works for X and Zero using the original normal and charged hook
and chain graphics. The charged attack requires X1's arm upgrade. This port uses
X1's live terrain, enemy collision, damage immunities, and collectible objects.
The implementation is isolated in `src/mmx_weapon_chain.inc` with small combat
and renderer registrations. No save-state ABI or asset-cache change is needed.

## Original USA ROM findings

These are CPU addresses, established by decoding the source instructions and
tracing a private X2 reference runtime. The owner's game and save states were
not used for the investigation.

| Address | Behavior |
| --- | --- |
| `$81:9816..98B0` | Normal dispatch, initialization and rejection/refund of duplicate proxy shots |
| `$81:98DF..9917` | Normal startup: original sequence 0, then 16-tick extension timer |
| `$81:9918..9977` | Extend 4 pixels/tick; release fire, floor/ceiling, or timer starts retraction; wall contact starts pull |
| `$81:9978..99F8` | Retraction, wall pull, and carried pickup phases |
| `$81:99F9..9A31` | Kill continues normal chain; surviving hit retires it; either successful contact spends another half energy |
| `$81:9A4D..9A9C` | Follow player displacement, bounded to 12 pixels, and current arm height |
| `$81:9A9D..9B1C` | Nine cosmetic links; positions at eighths between arm and hook |
| `$81:9B29..9B90` | Current muzzle and return/anchor crossing checks |
| `$81:9B91..9BCE` | Pull allowed at body-to-hook distance 12..<64 normal / 12..<128 charged; opposite input cancels |
| `$81:9BCF..9C00` | Facing change, hurt/death, teleport and special body-state cancellation |
| `$81:9C3A..9C79` | Grab ordinary weapon energy, health and lives; hold with item `+$28` and release on cleanup |
| `$81:A6A4..A732` | Charged dispatch and 6-pixel/tick initialization |
| `$81:A765..A7FC` | Charged startup, 18-tick automatic extension and retraction; pickup scan during retraction |
| `$81:A7FD..A865` | Charged wall pull and pickup retrieval |
| `$81:A866..A899` | Kill reward, restore phase after enemy contact, clear contact counter, spend 1/16 energy |
| `$81:849D..84E3` | Normal link sequence 4 and charged link sequence 10 |
| `$88:C43E..C451` | Keep the firing pose while the tether is active |
| `$86:AF34` | Half-energy launch cost; charged duplicate initialization also refunds this entry |
| `$86:B790`, `$86:BA17` | Centered hook collision boxes, half extents 5x5 normal / 8x8 charged |
| `$86:F4C8` | Neutral damage row: buster 3, normal chain class `$0C` = 5, charged class `$15` = 1 |
| `$88:DA9B..DB78` | Native hit result dispatch: surviving hit state 8, lethal hit state 6 |
| `$88:DFF5..E035`, `$86:B0CE` | Kill reward row 7: equal small-health/small-weapon-energy probabilities |

The normal startup reaches its terminal animation marker after two ticks. A
held hook extends for fifteen movement ticks at 4 pixels/tick (60 pixels beyond
the original 16-pixel muzzle). A tap can retract before any extension. Charged
startup reaches its terminal marker after eleven ticks (3+4+4); it extends for
seventeen movement ticks at 6 pixels/tick (102 pixels beyond the muzzle), even
after fire is released. Retraction uses the same speed toward the current arm.

Both launch costs are **0.5 energy**. A reference trace starting at 28 produced
27.5 after a normal shot and 27 after holding a full charge and releasing (the
initial normal shot plus the charged launch). `$86:AF4A` is not the charged
launch-cost entry; interpreting that zero as a free charged launch was rejected
against the actual runtime trace.

The source keeps the shooting arm out for the entire tether. Runtime samples
showed body firing timer `+$50=15` throughout both normal and charged chains,
then counted down after cleanup. The port refreshes X1's native firing timer,
preserving standing, running, jumping and wall firing animations.

## Art and damage

The existing user-ROM extractor supplies X2 group **71 (`$47`)**, all 24 poses,
its original animation records, and pose DMA table `$85:A068`. Sequences 0/3/2
are normal startup/flying/anchored hook; 6/7/9 are charged equivalents. Sequence
4 cycles link poses 5..10 at three ticks each; sequence 10 cycles charged link
poses 20..23 at three ticks each. The renderer reconstructs the nine source
cosmetic actors without spending native projectile slots or inventing art.

Original data is read from the owner's supplied X2 ROM through the shared ROM
picker/cache workflow. No ROM, extracted sprites, or generated asset pack is
checked in. See `docs/x-weapons-port.md` and `docs/x-weapons-source-notes.md`
for the distribution and source-ROM requirements.

The normal hook deals **5/3 of neutral X1 buster damage**, plus the shared
cumulative-third rounding. It continues after a kill and ends after striking a
surviving enemy. Charged contacts deal **1/3 of neutral buster damage each**,
with the source's per-tick contact reset and 1/16 energy drain. A charged kill
allocates a genuine X1 small health or weapon-energy pickup; collection stays
in the native item handler. Boss/armored responses retain X1's neutral damage
and immunity handling; X2 boss weaknesses are not imported.

## Host adaptations and limits

- The pull moves X/Zero 4 or 6 pixels toward the anchored hook after ordinary
  player movement, preserving source arm height. It sweeps against X1 terrain
  so a hook cannot force the character through a solid wall. X1 moving objects
  and stage scripts retain their own collision handling.
- Ordinary X1 pickup pool `$1628..1927`, stride `$30`, maps the same kinds
  1/2/4. Held pickups retain native lifetime, collection and refill behavior.
  Zero's arm reaches farther than the native pickup collection box, so an item
  retracts the final few pixels toward his body before release. Ordinary HP
  collection still heals only the active character.
- The 50/50 charged kill reward uses source RNG arithmetic seeded from saved tick state;
  it does not reproduce X2's global RNG sequence or alter X1 enemy drop tables.
- Enemy shields and invulnerability remain native. The charged hook continues
  after a blocked hit, while the normal hook ends, matching the source phases.
- X2's original SPC sound bank is not imported by this handler. X1's shared
  weapon firing/charge audio remains in use.

## Focused verification

`MMX_WEAPON_CHAIN_TEST=1` selects the focused checks in
`tests/mmx_weapon_chain_test.inc`, using a private headless X1 fixture. They
cover both characters' tap/held/charged attacks, launch energy, X1 arms gating,
original art availability, held firing pose, and exact replay; actual terrain
wall attachment and both pull speeds; opposite-direction cancellation; native
pickup retrieval and collection; normal enemy collision/damage/contact cost;
and charged contact rounding, repeated-hit drain, and guaranteed kill reward.

Private renderer captures check the normal and charged original hook/link art.
The source-reference evidence and test fixtures remain ignored research output;
the reproducible source addresses and assertions above are the durable record.

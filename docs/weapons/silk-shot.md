# X2 Silk Shot backport

Tracking: central Beads `beads-8wg.1.38`, under weapon-port issue
`beads-8wg.1.32`. Implementation branch: `feat/x2-silk-shot-port`.

This implements native X2 weapon ID 3 for X and Zero: all five original material
forms, normal bounces and four-way fragments, leaf trails, and the eight-piece
charged gathering/held/release attack. It uses X1's arm-upgrade requirement,
player movement, muzzle placement, native enemy collision and damage immunity.
The implementation is in `src/mmx_weapon_silk.inc`; its small registration points
are in `mmx_weapon_combat.c`.

## Source addresses and observed behavior

Addresses below are SNES CPU addresses in the original USA X2 ROM. Instruction
decoding and private reference-runtime traces were used; the owner's playtest
was not touched or loaded from a state.

| Source | Meaning |
| --- | --- |
| `$81:91C0..9267` | Normal initialization, material selection, velocity and palette |
| `$81:9289..92D0` | Normal bounce count, rebound loss, gravity and wall contact |
| `$81:92D1..9313` | Four normal fragments |
| `$81:931F..9382` | Leaf bundle launch, deceleration, upward curve and breakup |
| `$81:93DE..951F` | Normal fragment initialization and movement |
| `$81:952C..9564`, `$81:84E7..8538` | Leaf trail allocation, original animation and 60-tick life |
| `$88:C475..C4E7` | Start gathering while fully charged fire is still held |
| `$81:A136..A335` | Charged gathering, held formation, release and impact |
| `$81:A413..A501` | Release conditions, formation offsets and gather positions |
| `$86:B6DB`, `$86:B6F7` | Normal and fragment animation IDs per material |
| `$86:B6E9`, `$86:B705`, `$86:B735`, `$86:B73C` | Gravity, fragment velocities, bounce counts and rebound loss |
| `$86:B910`, `$86:B938`, `$86:B958`, `$86:B978`, `$86:B988` | Charged piece animations, gather offsets, radial velocities, formation offsets and flips |
| `$86:AF2E`, `$86:AF44` | Normal cost 1, charged cost 2 (8.8 values) |
| `$86:B74E`, `$86:B990` | Material-specific damage classes |
| `$86:F4C8` | Ordinary-enemy damage row, basic buster = 3 |

Normal shots start at horizontal speed 2 pixels/tick and upward speed 3. They
use source material-specific gravity and bounce loss. A solid side or final
bounce scatters four fragments; enemy contact also scatters the main shot.
Fragments retain their original graphics and source velocities, with native
enemy collision until they leave the visible projectile bounds.

| Material | Normal class | Charged class | Normal / charged raw damage | Gravity (8.8) | Ground contacts before breakup | Rebound loss (8.8) |
| --- | --- | --- | --- | --- | --- | --- |
| Stone | `$09` | `$12` | 15 / 30 | 96 | 2 | 384 |
| Metal balls | `$18` | `$1F` | 15 / 30 | 64 | 3 | 256 |
| Robot scrap | `$1B` | `$20` | 15 / 30 | 64 | 1 | 512 |
| Leaves | `$1C` | `$21` | 5 / 10 | 32 | Curved launch, then falling leaves | Special |
| Crystal | `$1E` | `$22` | 15 / 30 | 64 | 127 | 16 |

Stone, metal and scrap fragments move at 4 pixels/tick on each axis; crystal
fragments move at 5. Leaves launch at 5 horizontally, decelerate by 64/256 each
tick and curve upward for 20 ticks. They then slow and scatter into four
original animated leaves falling at 1–2 pixels/tick for 60 ticks. The launch
sheds the original cosmetic leaf actor every four simulation ticks, also with
a 60-tick lifetime and alternating-frame visibility. RNG uses the existing
saved-tick deterministic source-arithmetic helper; it does not claim to share
X2's entire global RNG stream.

Once X1 reaches full charge with the arm upgrade, eight pieces appear at the
original 256-pixel gather offsets. They converge at 8 pixels/tick for 32 ticks,
then follow the character's muzzle in the source formation. The player may move
while holding the cluster. Releasing fire launches all pieces together at
4 pixels/tick, upward speed 1, gravity 32/256 and downward cap 5. Each piece
spends 1/4 energy on release, totaling 2 for eight pieces. Gathering itself is
free; the initial press still fires the normal one-energy shot. Releasing early
or entering a source-equivalent hurt/death/teleport condition releases gathered
pieces. Native charge sound is stopped and the ordinary X1 release allocation
is suppressed so it cannot add an extra shot.

Charged terrain/enemy impact scatters each piece along its original eight-way
velocity; leaves instead drift downward. A subsequent enemy contact removes
the dispersed piece. Already-hit enemies remain excluded for that piece during
its transition, preventing double counting one continuous contact. Ordinary
damage is the raw value divided by 3 and scaled by the target's X1 buster damage:
5 / 10 buster hits for solid materials, 5/3 / 10/3 for leaves. Existing shared
fractional carry preserves ratios. Boss/special-category neutral damage and
X1 immunity stay unchanged; no imported weaknesses are added.

## Original assets and X1 material choice

All gameplay poses 5–29 have explicit original DMA records at `$85:9FEF`.
The previous foundation note saying other forms required stage graphics was
incorrect. Only tiny unused poses 1–4 inherit stage tiles. All gameplay artwork
can be extracted directly from the user-provided X2 ROM.

Original group `$48` contains:

- Stone: poses 5–8; metal: 9–12; robot scrap: 13–18.
- Leaves: 19–25, using palette `$05:BAC0`.
- Crystal: 26–29, using palette `$05:BB00`.

Stone, metal and scrap use `$05:BAE0`. Palette selection comes from
`$86:B6F0` through the palette records rooted at `$86:817A`; resources `$46`,
`$56`, `$C4` select those three palettes. The earlier cache mislabeled poses
19–29 as scrap and colored leaves/crystal with the stone palette.

The descriptor now has three cached groups (72, 73, 74), with an optional
`source_group: 72` selecting the original layout and animation tables. Each
cached group keeps the correct palette and native pose numbers. Python and
native extraction agree byte-for-byte. The on-disk format remains `MMXWEAP5`;
with Tornado Fang integrated, the combined cache has 638 nonempty poses and
454,607 bytes. No ROM,
extracted pixels, palettes, capture, or reference screenshot is committed.
The launcher regenerates the local cache from the user's ROM automatically.

X2 chooses material through authored stage events (`$1F1B`). X1 has no such
events, so this port deliberately maps environments:

| X1 stage ID (`$1F7A`) | Environment | Material |
| --- | --- | --- |
| 0 | Highway | Metal |
| 1 | Launch Octopus | Stone |
| 2 | Sting Chameleon | Leaves |
| 3 | Armored Armadillo | Stone |
| 4 | Flame Mammoth | Robot scrap |
| 5 | Storm Eagle | Metal |
| 6 | Spark Mandrill | Robot scrap |
| 7 | Boomer Kuwanger | Metal |
| 8 | Chill Penguin | Crystal |
| 9 / 10 / 11 / 12 | Fortress stages | Stone / scrap / metal / crystal |

This table is an X1 adaptation, not an original X2 stage mapping. X2's special
material values 5/6 produce health/energy in specifically authored hidden
rooms. X1 has no equivalent rooms; this port does not add arbitrary pickup
generators. Static X1 terrain uses the existing sweep adapter. Moving stage
actors do not automatically become new Silk collision surfaces. Original X2
sound sample import remains outside this weapon-specific change; native X1
firing/charge sound cleanup is retained.

## State and validation

The implementation uses the existing eight native projectile slots and sixteen
saved cosmetic slots. Charged pieces compete for the original finite slot pool;
unavailable slots are skipped. Cosmetic leaves do not consume damage slots.
No combat/save/capture ABI version changes are required. State validation
accepts only the material's correct group and bounded phase/piece identity.

`MMX_WEAPON_SILK_TEST=1` adds focused ROM-backed checks to `mmx_state_tests`:
both characters' normal launch/cost/fire limit, real terrain bounces and
fragments, no-arm gating, held gathering, release speed/cost/audio cleanup,
terrain dispersion, deterministic normal and moving-held save/replay, original
leaf curves/trails/fall lifetime, first-full-tick release, and real X1 enemy
collision/damage. Releasing exactly as full charge becomes available also
launches the complete attack without requiring a prior gathering tick. Captures
of held and released clusters and leaf particles are available locally for
visual review. Original material sheets were rendered directly from ROM and
inspected. Native/Python extraction parity, copier-header support, wrong-ROM
rejection and cache preservation also pass.

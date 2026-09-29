# X2/X3 boss-weapon port

Scope and sequencing: [character/weapons/co-op roadmap](zero-weapons-coop-roadmap.md).
Tracking: central Beads `beads-8wg.1.32`, branch `feat/x3-zero-port`.

## Asset extraction foundation

`tools/extract_x_weapons.py` builds a local `MMXWEAP1` cache from both original
USA ROMs. It validates normalized ROM hashes, accepts copier headers, reads
the original sprite layouts and DMA lists, and preserves original palettes.
The source-controlled descriptor contains addresses only; ROMs and extracted
graphics remain local.

```powershell
python tools/extract_x_weapons.py ../MegamanX2Recomp/mmx2.sfc ../MegamanX3SNESRecomp/mmx3.sfc build-zero/port-work/x-weapons.bin
```

The current cache contains 572 poses (415,681 bytes), including native weapon
icons in the corresponding pose groups. This is an asset foundation, not a
claim that the weapons are already playable in X1.

Sources were checked in the local recomp projects using private, paused
reference runs. The owner's playtest was never loaded from these fixtures.
Normal and charged releases were recorded for each native weapon ID. Artwork
samples were rendered directly from the extracted indexed pixels and inspected.

| Native ID | X2 weapon / sprite group | X3 weapon / sprite group |
| --- | --- | --- |
| 1 | Crystal Hunter / `$10` | Acid Burst / `$05` |
| 2 | Bubble Splash / `$44` | Frost Shield / `$06` |
| 3 | Silk Shot / `$48` | Triad Thunder / `$0B` |
| 4 | Spin Wheel / `$46` | Spinning Blade / `$0C` |
| 5 | Sonic Slicer / `$41` | Ray Splasher / `$0D` |
| 6 | Strike Chain / `$47` | Gravity Well / `$0F` |
| 7 | Magnet Mine / `$0F`, charged `$13` | Parasitic Bomb / `$10` |
| 8 | Speed Burner / `$25`, charged `$26` | Tornado Fang / `$13` |

Both games use the sprite layout root at `$8D:8000`. Weapon-selection graphics
are the original seven-byte bulk DMA records: X2's pointer table is
`$86:9664`, X3's is `$86:97AD`, indexed by `$3E + weapon * 2`. Per-pose graphics
use the six-byte DMA records in bank `$85`. Palette addresses and group DMA
roots are in `tools/data/x_weapon_assets.json`.

Triad Thunder also loads its normal lightning graphics from `$86:9976` when
fired; the ground-wave frames replace that region through their own pose DMA.
Crystal Hunter's inherited frames use its original setup CHR transfer.
Silk Shot currently extracts its original scrap form (poses `$13-$1D` plus the
icon). Its other forms borrow stage graphics and require a separate X1 terrain
adaptation. Tornado Fang includes the 36 frames covered by its player weapon
DMA table; the additional layouts do not use that table.

The binary stores sixteen weapon entries, each with game/weapon IDs, original
X body and weapon palettes, then groups of cropped indexed sprite frames.
Every frame retains its signed position relative to the actor origin. Empty
entries retain native pose numbering for explicitly omitted Silk Shot forms.

## Remaining implementation

Connect the cache to the runtime, then add persistent weapon selection/energy
and L/R pause pages. Implement the actual normal and charged attacks, native
sound/effect cleanup, X/Zero firing origins, terrain/enemy interaction and
meaningful special behaviors. Gate charging on X1's arm upgrade. Keep ordinary
damage and existing X1 progression. Co-op remains a later, separate mod.

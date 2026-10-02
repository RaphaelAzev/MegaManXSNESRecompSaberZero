# Public mod: user-supplied source ROMs

Owner requirement, recorded 2026-09-29. Applies to the Zero/Select mod, the
X2/X3 weapon expansion, and the later mutually exclusive co-op mod. Scope:
[roadmap](zero-weapons-coop-roadmap.md). Technical findings:
[source notebook](x-weapons-source-notes.md).
Zero implementation/provenance: [Zero port handoff](zero-port.md).

## Distribution contract

The user supplies their own X1, X2 and X3 ROMs for the complete mod. A public
download must not contain ROMs or extracted game assets. This covers **all**
imported content, including Zero's body animations, saber, teleport effects,
HUD badge, menu/title poses, weapon projectiles, icons, palettes, animation
records, and any future imported audio or effects. These are not exceptions
because they are small or embedded in another file.

| Enabled content | Required user-supplied source |
| --- | --- |
| Base X1 game and its native assets | Supported X1 ROM |
| X3 Zero, character exchange and X3-derived visuals | Supported X3 ROM, in addition to X1 |
| X2 weapons alone | Supported X2 ROM, in addition to X1 |
| X3 weapons alone | Supported X3 ROM, in addition to X1 |
| Complete X2/X3 boss-weapon expansion | Supported X2 and X3 ROMs, in addition to X1 |
| X/Zero couch co-op | Supported X3 ROM, in addition to X1; X2/X3 weapon packs remain optional |

Extract needed data locally from the supplied ROMs. Ship the mod's code,
extractors, address descriptors and documentation. Keep generated caches on
the user's machine; do not bundle a developer's caches or download them as a
substitute for supplying the source ROMs. No ROM-finding/downloading service
is part of setup.

## Launcher setup and automatic extraction

The launcher exposes **X3 Zero**, **X2 Weapons**, **X3 Weapons** and
**X / Zero Co-op**. Co-op and **Add Zero** claim the same character plugin, so
selecting one turns the other off. Users select
original USA `.sfc`/`.smc` ROMs, never extracted `.bin` files. X2/X3 weapon packs
also work with X alone when Zero is disabled. Pause L/R skips disabled packs.

Zero, co-op and X3 weapons bind `shared_key = "megaman-x.source.x3"`; choosing,
replacing or clearing any of these pickers changes the same canonical X3 path
and immediately appears on the others. X2 uses `megaman-x.source.x2`. The shared
runtime persists these values as `[[shared_resource]]` entries in mod state.
An enabled feature with a missing or incorrect ROM cannot launch. Original
ROM SHA-256 is checked after stripping an optional 512-byte copier header:

- X2: `f3246755f608a1e1dc9c848b61da3b824c7853b29b3be40df6fc7f2793a887ed`
- X3: `65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7`

`src/mmx_source_assets.cpp` performs native extraction at activation. There is
no Python/runtime tool installation or user extraction step. It validates the
ROM again and atomically writes private caches in `cache/mmx-source/` beside
the executable: `x3-zero-v7.bin`, `x2-weapons-v5.bin`, `x3-weapons-v5.bin`.
Each launch regenerates from the selected ROM, so stale/developer caches are
never an implicit fallback. Native extraction matches the reference tools
byte for byte. Copier headers produce identical output; wrong ROMs are rejected.

The weapon descriptor compiles into address-only C++ tables during the normal
developer build. Zero uses the same verified source addresses as the reference
extractor. Separate eight-entry weapon caches use `MMXWEAP5`; the loader also
accepts the combined sixteen-entry development cache in that format. Version 4
added each source game's dedicated 16x16 gameplay HUD footer, separate from its
pause-menu icon. Version 5 adds a palette per sprite group and up to three
groups, allowing original shared explosion art with its own source palette.
Compressed static graphics are decoded from the selected ROM using address-only
descriptors. Old weapon caches are regenerated from the selected ROMs.
Source pack selection and this artwork change do not alter gameplay saves.

Shared framework support originated in `795fc99` and is integrated at
`8566fdb` (framework PR #131), the same tested pin used by Zero 0.0.1
(central issue `beads-8wg.2.90`); game integration is tracked in `beads-8wg.1.35`.
The framework provider test covers both sharing directions, independent keys,
persistence, clearing, copier headers and incorrect-ROM launch rejection.
ROM-backed game checks cover independent packs without Zero, pause page
skipping, native-width composition, firing, exact save/replay and loading both
packs without discarding the first pack's art.
The desktop fresh-boot check activated Zero and both weapon packs using only
the selected source paths and generated all three private caches. No save state
was loaded. `tests/check_source_extraction.py` reproduces byte parity, header
normalization and wrong-ROM rejection without overwriting a valid cache.

## Developer reference tools

- `tools/extract_zero.py` produces the local `zero-x3.bin` cache from X3.
- `tools/extract_x_weapons.py` produces the local `x-weapons.bin` cache from X2
  and X3 using `tools/data/x_weapon_assets.json`. It checks normalized source
  hashes and accepts a 512-byte copier header.
- These remain reference/debugger tools. Launcher activation requires source
  ROMs and generates its own caches; it does not use those developer files.
- Addresses, extraction logic and behavioral findings belong in source
  control; ROMs, decoded graphics, asset caches, private save states and
  research captures do not belong in public release packages.

## Release boundary and remaining weapon work

Zero 0.0.1 has shipped separately in PR #52 with the native ROM picker and
private extraction. Its package audit stages only the tracked mod catalog,
rejects private state/cache/ROM files, validates DLL closure, and passed a fresh
cold boot from a ZIP install. The weapons branch retains that packaging path.

The X2/X3 weapons remain development content: four combat implementations and
twelve fallbacks. Finish their behavior/fidelity and audit every added resource
before publishing a weapon release. Future sound imports must also come from
the user's ROMs. Recheck the final weapon distributable from a clean install,
including both independent packs and their shared X3 selection with Zero.

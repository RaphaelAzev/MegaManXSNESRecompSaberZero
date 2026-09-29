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
| Complete X2/X3 boss-weapon expansion | Supported X2 and X3 ROMs, in addition to X1 |
| Later X/Zero co-op with the completed weapon set | The same X1/X2/X3 sources |

Extract needed data locally from the supplied ROMs. Ship the mod's code,
extractors, address descriptors and documentation. Keep generated caches on
the user's machine; do not bundle a developer's caches or download them as a
substitute for supplying the source ROMs. No ROM-finding/downloading service
is part of setup.

## Existing development implementation

- `tools/extract_zero.py` produces the local `zero-x3.bin` cache from X3.
- `tools/extract_x_weapons.py` produces the local `x-weapons.bin` cache from X2
  and X3 using `tools/data/x_weapon_assets.json`. It checks normalized source
  hashes and accepts a 512-byte copier header.
- Local development currently supplies those caches beside the executable.
  This is **not yet the public source-ROM setup flow**.
- Addresses, extraction logic and behavioral findings belong in source
  control; ROMs, decoded graphics, asset caches, private save states and
  research captures do not belong in public release packages.

## Required before public release

1. Add a setup flow to select the required source ROMs, validate supported
   revisions, and extract all required caches locally. Clearly identify an
   unsupported or missing source and which feature needs it.
2. Record source hashes and cache format/extractor versions so incompatible
   or stale caches are rejected or rebuilt. Never silently use a developer's
   machine paths as a fallback.
3. Audit every imported resource, including embedded arrays and future sound
   imports, for extraction from user-supplied sources. Keep a source/address
   record in the extraction descriptors and investigation notebook.
4. Stage releases from an explicit package manifest; reject ROMs, extracted
   caches, source-derived test fixtures and research artifacts. `.gitignore`
   alone is not a packaging safeguard.
5. Validate setup from a clean install with no developer caches: missing and
   unsupported sources, supported sources, regeneration, and a fresh boot.
   Test Zero and the weapon set through the same setup path users will use.

This document records the release requirement and remaining work. It does
not claim that the public setup flow or packaging audit is complete.

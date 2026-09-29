# Public mods: user-supplied source ROMs

The user supplies the supported X1 ROM and an original USA X3 ROM for Zero.
The later X2/X3 weapon expansion requires those games' respective ROMs. This
contract covers all imported sprites, palettes, animation records, collision
data, badges and future audio. No extracted content belongs in a public package.

## Zero 0.0.1 setup

The launcher asks for a **ROM**, never an extracted cache. Its X3 file resource
uses shared key `megaman-x.source.x3`, reserved also for the weapon follow-up.
The provider validates normalized SHA-256, accepts 512-byte copier headers,
persists the path locally, and rejects unsupported ROMs before activation.

Supported X3 USA normalized SHA-256:
`65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7`.

`src/mmx_source_assets.cpp` repeats validation and extracts Zero assets natively
from the selected ROM, atomically publishing `cache/mmx-source/x3-zero-v6.bin`
beside the executable. Failed validation leaves the previous cache untouched.
Activation always requires the selected ROM; it never falls back to developer
assets. Runtime extraction requires no Python or additional download.

The native output is byte-for-byte equivalent to `tools/extract_zero.py`.
The cache contains the original 117 body poses, 21 saber body poses, 14 saber
blade poses, palettes, four HUD tiles, animation records, muzzle offsets and
saber bounds. [Zero's source notebook](zero-port.md) records the ROM addresses.
Code stores addresses and integration logic; imported bytes stay in the cache.

## Release staging

`tools/make_release.ps1` stages the executable, launcher assets, default config,
README, Zero release notes and checked-in mod catalog. It does not copy the
runtime's writable `mods` directory, key bindings or private caches. Catalog
staging rejects untracked files, `state.toml`, ROMs, binary asset caches, saves
and compositor captures. Runtime DLL dependencies are audited from PE imports.
ROMs, extracted cache files and private fixtures are used only for local tests.

## Follow-up weapons contract

Both weapon packs live on the separate `feat/x2-x3-weapons` branch. X3 weapons
reuse `megaman-x.source.x3`; changing it in either feature updates the other.
X2 uses `megaman-x.source.x2` and normalized USA SHA-256
`f3246755f608a1e1dc9c848b61da3b824c7853b29b3be40df6fc7f2793a887ed`.
Both remain subject to the same private extraction and release audit rules.
Co-op is a later mutually exclusive mod; see the [roadmap](zero-weapons-coop-roadmap.md).

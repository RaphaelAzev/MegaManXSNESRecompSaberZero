# Mega Man X 1.7.0-rc.2

This is a Windows testing prerelease. **1.6.6 remains the latest stable release.**

## Changes since 1.7.0-rc.1

- Fix garbled P2 X sprites when P1 plays Zero, including the reported Highway
  netplay case. X's new animation pose now uses its matching sprite graphics
  during movement, jumping, and firing. The problem also affected offline co-op.
- Preserve native gameplay, graphics-transfer timing, and save/rollback formats.
  Validation covers both character assignments, deferred graphics transitions,
  captured-frame restore, and local two-peer netplay startup.

This includes the previous preview's Zero, X2/X3 weapons, couch co-op, netplay,
and startup/Highway pacing fixes. See the
[1.7.0-rc.1 notes](https://github.com/mstan/MegaManXSNESRecomp/releases/tag/v1.7.0-rc.1)
for the full feature list.

## Setup

Extract the entire Windows ZIP and run `MegaManXSNESRecomp.exe`. Supply your own
**Mega Man X (USA) (Rev 1)** ROM. Zero and co-op also require your own **X3 USA**
ROM; X2 weapons require **X2 USA**. Select these ROMs in Mods; extraction is
automatic, and Zero/co-op/X3 weapons share the same local X3 selection.
No ROMs, extracted character/weapon assets, save files, or personal settings
are included. SHA-256 checksums accompany the download.

For netplay, **update both PCs to this same build** and configure local
**Player 1** input on each. The room assigns it to the network seat. Netplay
starts from a fresh boot. See the
[netplay guide](https://github.com/mstan/MegaManXSNESRecomp/blob/v1.7.0-rc.2/docs/netplay.md).

## Known limitations

- Real netplay testing is underway; a complete two-machine campaign has not
  been qualified. The reported P2 X corruption was reproduced and fixed, with
  automated co-op and local-peer checks; broader remote playtesting continues.
- A shared-engine issue remains in audio-clock save/rollback serialization.
  Audio state can differ after replay even when gameplay and graphics match.
  Netplay remains experimental while this is resolved.
- Native 60.0988 Hz output still has uneven refresh holds on a fixed 165 Hz
  display; physical scanout/VRR behavior is not certified.
- This prerelease supplies a Windows package. Linux/macOS remain buildable from
  source; no new Linux or macOS binary is attached to this testing release.

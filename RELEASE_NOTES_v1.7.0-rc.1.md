# Mega Man X 1.7.0-rc.1

This is a Windows testing prerelease. **1.6.6 remains the latest stable release.**

## Changes since 1.6.6

- Play as **X or Zero**, with Zero's imported animations, buster/saber combo,
  and single-player Select switching.
- Optional **X2 and X3 weapon mods** add all sixteen boss weapons and charged
  attacks, with adapted enemy damage and additional X1 boss weaknesses.
  Charged special weapons require X1's arm upgrade.
- **Couch co-op** puts X and Zero on screen together, with independent health,
  weapons and energy, shared subtanks, and a camera that keeps both in view.
  Pickups benefit their collector. A fallen player stays out until the next
  stage or team restart. Choose P1's character in Mods; P2 gets the counterpart.
- **Two-player netplay** uses the shared recomp-net lobby and rollback support.
  Co-op is required online; optional weapons and the fixed original/16:9/21:9/32:9
  view are settled by the room. Offline settings and saves are kept separate.
- Fix a Windows diagnostic-logging stall that delayed startup, and an X1
  graphics-streaming stall that caused repeatable Highway hitches in both solo
  and co-op play. Matched walking tests reduced worst frame intervals from
  roughly 35–44 ms to 20–21 ms on the test machine.

## Setup

Extract the entire Windows ZIP and run `MegaManXSNESRecomp.exe`. Supply your own
**Mega Man X (USA) (Rev 1)** ROM. Zero and co-op also require your own **X3 USA**
ROM; X2 weapons require **X2 USA**. Select these ROMs in Mods; extraction is
automatic, and Zero/co-op/X3 weapons share the same local X3 selection.
No ROMs, extracted character/weapon assets, save files, or personal settings
are included. SHA-256 checksums accompany the download.

For netplay, both players must use this same build and configure their local
**Player 1** input. The room assigns it to their network seat. Netplay starts
from a fresh boot. Escape or disconnect returns to the launcher; create or join
a new room for another match. See the
[netplay guide](https://github.com/mstan/MegaManXSNESRecomp/blob/v1.7.0-rc.1/docs/netplay.md).

## Known limitations

- Internet support is included, but a full campaign and a real two-machine
  internet/controller playthrough have not been qualified. Existing validation
  includes local UDP peers, independent equipment, intentional rollback, and
  actual desktop startup/pacing checks.
- A shared-engine issue remains in audio-clock save/rollback serialization
  (`beads-8wg.2.95`). Audio state can differ after replay even when gameplay and
  graphics match. The netplay build remains experimental while this is resolved.
- The Highway streaming fix does not eliminate every possible source of
  display jitter. Native 60.0988 Hz output still has uneven refresh holds on a
  fixed 165 Hz display; physical scanout/VRR behavior is not certified.
- This prerelease supplies a Windows package. Linux/macOS remain buildable from
  source; no new Linux or macOS binary is attached to this testing release.

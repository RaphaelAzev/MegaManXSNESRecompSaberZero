# Mega Man X 2.0.1

This release fixes two co-op encounter bugs from 2.0.0-alpha.

## Fixes

- **Thunder Slimer puddles trap the correct player.** A puddle touching Zero
  could previously pin X instead. Capture now stays with the player it hit,
  including when only one player is alive and after save/load or rollback.
- **Both players return in Flame Mammoth's boss room.** The final door's
  scrolling introduction could leave X or Zero hidden for the fight. Solid
  conveyor belts now qualify as safe ground for the partner's return.

These changes apply to offline co-op and netplay. Both fixes were confirmed
in an offline player test. Automated checks cover both door drivers, puddle
ownership, lone survivors, deterministic save replay and the existing co-op
controller regression suite. Storm Eagle's entrance also completed in a
separate replay with both players present.

## Setup and updating

Download the Windows ZIP or Linux x86_64 AppImage. Supply your own
**Mega Man X (USA) (Rev 1)** ROM. Zero and co-op require your own **X3 USA**
ROM; X2 weapons require **X2 USA**. Select source ROMs in Mods; extraction is
automatic. No ROMs, extracted assets or private test saves are included.
SHA-256 hashes are provided in `SHA256SUMS.txt`.

For netplay, **both players must update to 2.0.1** and configure their
local **Player 1** input. Lobby rooms are separated by version. See the
[netplay guide](https://github.com/mstan/MegaManXSNESRecomp/blob/v2.0.1/docs/netplay.md).

Existing saves remain loadable. A save made while a puddle had already trapped
the wrong player cannot recover the original intended victim; retry from
before that contact to test the fix.

## Known limitations

- The separately reported camera lock after Thunder Slimer's defeat has not
  reproduced locally and remains under investigation.
- A full two-machine internet campaign has not been qualified yet.
- After a rollback, audio can differ briefly even when gameplay and graphics
  match.

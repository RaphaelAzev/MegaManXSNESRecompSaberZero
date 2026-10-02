# Mega Man X 2.0.2-alpha

This release fixes co-op platform travel, Storm Eagle's entrance, and two
X1 weapon graphics issues.

## Fixes

- **Moving platforms carry both players.** Zero could fall through or be left
  behind because platform contact only handled one player. Each living player
  now gets independent platform contact and can jump off normally.
- **Both players return inside Storm Eagle's airship door.** The returning
  partner can no longer teleport across the closed door and become stranded.
- **Storm Eagle's lift carries the party into the fight.** Either player can
  trigger the lift. Their partner teleports out during the ascent and ship
  destruction, then returns safely when the boss introduction ends.
- **Charged Chameleon Sting keeps its native visual effect.** X's color cycle,
  blinking and armor visibility now stay together during co-op.
- **Electric Spark's wall split keeps its original graphics.** The other
  player's weapon changes no longer corrupt the projectile or its fragments.

These changes use the shared co-op implementation for offline play and netplay.
Local ROM-backed checks cover both players driving the door and lift, independent
platform riders, health preservation, deterministic save replay, and native PPU
comparisons for the weapon effects. A two-machine netplay session has not yet
been repeated for these fixes.

## Setup and updating

Download the Windows ZIP or Linux x86_64 AppImage. Supply your own
**Mega Man X (USA) (Rev 1)** ROM. Zero and co-op require your own **X3 USA**
ROM; X2 weapons require **X2 USA**. Select source ROMs in Mods; extraction is
automatic. No ROMs, extracted assets or private test saves are included.
SHA-256 hashes are provided in `SHA256SUMS.txt`.

For netplay, **both players must update to 2.0.2-alpha** and configure their
local **Player 1** input. Lobby rooms are separated by version. See the
[netplay guide](https://github.com/mstan/MegaManXSNESRecomp/blob/v2.0.2-alpha/docs/netplay.md).

Existing saves remain loadable. To test the encounter fixes, use a save from
before the affected platform, door or lift sequence.

## Known limitations

- The camera-lock report after Thunder Slimer's defeat remains unreproduced.
- A reported pause-menu hang in fixed 32:9 is still under investigation.
- A full two-machine internet campaign has not been qualified yet.
- After a rollback, audio can differ briefly even when gameplay and graphics match.

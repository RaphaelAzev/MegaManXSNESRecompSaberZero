# Mega Man X 2.0.3-alpha

This release adds optional **Modern Zero** and removes the Windows debug
console from normal launches.

## Modern Zero

In **Mods**, open **Add Zero** or **X / Zero Co-op** and set **Zero behavior**
to **Modern**. Both dropdowns share the same setting. **X3 Behavior** remains
the default, with its original charged buster/saber sequence.

- Press Fire with the buster selected to swing the saber immediately.
- Zero keeps all X1/X2/X3 special weapons and their existing upgrade rules.
- Press Jump again in the air for a second jump; press Dash for an air dash.
- Airborne saber swings retain normal movement and steering. The hitbox follows
  Zero as he moves and turns, while ground swings still plant his feet.
- Swings use the existing X3 ground/air art, shorter recovery, and two modest
  damage opportunities. Native boss invulnerability remains in effect.
- Works with Zero in either co-op seat. The host's behavior choice is included
  in the netplay settings agreement.

This is an adaptation using X3 assets, not the complete X4-X6 move set.
Requested in [#61](https://github.com/mstan/MegaManXSNESRecomp/issues/61).

## Windows launcher

Normal launches now open only the launcher/game. Diagnostics are written to
`logs/mmx-<date>-<time>-<process-id>.log` beside the executable. Simultaneous
netplay instances keep separate logs. Read-only installations use
`%TEMP%/MegaManXSNESRecomp/logs` instead.

To restore a live console, change `Console = 0` to `Console = 1` under
`[Logging]` in `logging.ini`, then restart. Explicit stdout/stderr redirection
still works. Requested in
[#50](https://github.com/mstan/MegaManXSNESRecomp/issues/50).

## Setup and updating

Download the Windows ZIP or Linux x86_64 AppImage. Supply your own
**Mega Man X (USA) (Rev 1)** ROM. Zero and co-op require your own **X3 USA**
ROM; X2 weapons require **X2 USA**. Select source ROMs in Mods; extraction is
automatic. No ROMs, extracted assets or private test saves are included.
SHA-256 hashes are provided in `SHA256SUMS.txt`.

For netplay, **both players must update to 2.0.3-alpha** and configure their
local **Player 1** input. Lobby rooms are separated by version. See the
[netplay guide](https://github.com/mstan/MegaManXSNESRecomp/blob/v2.0.3-alpha/docs/netplay.md).

Older saves remain loadable. Saves written by this release use an extended
character state; keep backups if you plan to return to an older executable.

Validation includes ROM-backed jump/air-dash trajectories, saber movement and
hitbox alignment, original and imported special weapons, both co-op character
assignments, older saves, deterministic rollback, and linked launcher settings.
The existing X3 behavior passes its runtime checks. A fresh two-machine internet
session has not been repeated for this release.

## Known limitations

- The camera-lock report after Thunder Slimer's defeat remains unreproduced.
- A reported pause-menu hang in fixed 32:9 is still under investigation.
- A full two-machine internet campaign has not been qualified yet.
- After a rollback, audio can differ briefly even when gameplay and graphics match.

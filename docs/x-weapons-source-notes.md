# X1/X2/X3 weapon source reference

This is the durable investigation notebook for the weapon backport. Update it
when a source address, runtime observation or mapping is established. Scope:
[roadmap](zero-weapons-coop-roadmap.md); implementation/checkpoints:
[port notes](x-weapons-port.md). Work is tracked in central `beads-8wg.1.32`.

Addresses below are SNES CPU LoROM addresses unless explicitly marked RAM or
normalized ROM offset. Sources are the local original USA ROMs in the X1, X2
and X3 recomp projects. ROM hashes are checked by
[`x_weapon_assets.json`](../tools/data/x_weapon_assets.json). Do not commit ROMs,
extracted art or Ghidra databases. Reference sessions use private fixtures,
never the owner's running game.

## X1 integration map

| Address / RAM | Established purpose and port use |
| --- | --- |
| RAM `$0BA8` | Player object; X position `+$05`, Y `+$08`, facing `+$11` bit `$40` means right |
| RAM `$0BDB` | Native selected weapon, even ID; extended weapons retain native buster resources |
| RAM `$0BDD` | Active player projectile count; every owned allocation/retirement must balance it |
| RAM `$1228..$1427`, stride `$40` | Eight native projectile slots; port ownership tag `$5758` at `+3E` |
| RAM `$0E68..$1227`, stride `$40` | Ordinary enemy slots; enemy current HP `+27`, collision box pointer `+20` |
| RAM `$1F99` bit `$02` | Actual arms upgrade; required for charged X2/X3 weapons |
| RAM `$1F99` bit `$08` | Dash upgrade, NOT arms; Zero's innate dash must not enable charged specials |
| RAM `$1F88 + weapon_index*2` | Native X1 unlock/energy byte; extended inventory must not modify it |
| RAM `$1F12` | Native weapon-energy HUD state; zero rebuilds after extended selection changes |
| `$81:815C`, `$81:8165` | Player tick entry/end; Zero and extended combat input handling |
| `$81:9D47` | Newly allocated buster object available in X; mark extended shots before native effect dispatch |
| `$00:D3E5` / interpreted `$00:D3E7` | Projectile-active read; owned host simulation replaces native class update |
| `$84:9C16` / interpreted `$84:9C19` | Enemy/projectile hitbox read; suppress duplicate contact as appropriate |
| `$84:9E6E..9E76` | Native positive-damage subtraction; use original buster damage and existing immunity rules |
| `$81:A578..A5B5` | Native buster muzzle coordinates, facing and player-position addition |
| `$82:8174..8235` | Original fixed-point acceleration/movement; source vertical velocity is positive UP |
| `$00:C67F` | Pause menu input: L/R page cycling |
| RAM `$1EC8` | Native pause direct-page base; cursor `+0A` = `$1ED2`, candidate `+0B` |
| `$00:D907..D9F9` | Native weapon HUD setup/draw, using virtual selection/energy reads |
| `$81:E042..E103` | Collected weapon-energy item refill: freeze, four-frame increments, native sounds/cleanup |
| `$81:E11A..E169` | Auto-refill scan of owned X1 weapons; X reaches `$12` after exhausting the scan; RAM `$0000` holds remaining 8.8 energy |
| `$00:9EF9..9F0E` | Native full-inventory refill; extend to all 16 added weapons |
| Normalized ROM `$37F80..37F9F` | Verified unused bytes used for eight owned projectile collision boxes; separate from Zero's `$37FB0` saber boxes |

Item pool fixture: RAM `$1628`, stride `$30`. Kind `+0A=1` is weapon energy;
kind 2 is health. `+0B & 127` distinguishes small (1) from large (0), and bit
7 prevents fixture expiration. Kind 3 is NOT weapon energy. A wrong item kind
is an invalid test fixture, not evidence about the energy hooks.

Checkpoint death preserves native weapon energy. This was checked against an
owned X1 weapon at five energy while an extended weapon retained twelve.
Health refill is deliberately separate: both character HP pools refill on
respawn; ordinary HP pickups heal only the active character.

## X2/X3 source assets and runtime mapping

| Source fact | X2 | X3 |
| --- | --- | --- |
| Player object in measured source fixtures | RAM `$09D8` | RAM `$09D8` |
| Source projectile pool | RAM `$10D8..$1317`, stride `$40` | Same |
| First weapon energy pair | RAM `$1FBA` | RAM `$1FBB` |
| Arms upgrade byte | RAM `$1FD0`, bit 2 | RAM `$1FD1`, bit 2 |
| Sprite-layout root | `$8D:8000` | `$8D:8000` |
| Animation-group directory root | `$2F:A000` | `$3F:8000` |
| Static weapon-selection DMA root | `$86:9664` | `$86:97AD` |
| Menu compressed-resource table | `$86:FA01` | `$86:F732` |
| Resource `$4C` source | `$22:8FB9` | `$21:E7E4` |
| Menu icon palette, normalized ROM offset | `$2CEE0` | `$62DA0` |

Weapon-selection DMA is indexed by `$3E + weapon_id*2`. The descriptor records
each per-pose DMA table, group and palette. Animation directories contain
16-bit offsets into records `(duration, flags, pose)`; flag `$80` adds a signed
16-bit relative loop displacement from the following offset. The extractor
walks all sequences, validates bounds, and stores these source records.

Native IDs X2: 1 Crystal Hunter, 2 Bubble Splash, 3 Silk Shot, 4 Spin Wheel,
5 Sonic Slicer, 6 Strike Chain, 7 Magnet Mine, 8 Speed Burner.
Native IDs X3: 1 Acid Burst, 2 Frost Shield, 3 Triad Thunder, 4 Spinning Blade,
5 Ray Splasher, 6 Gravity Well, 7 Parasitic Bomb, 8 Tornado Fang. X3's pause
order differs: 1,7,3,4,5,6,2,8. Do not confuse menu order with actor IDs.

## X3 Spinning Blade observations

Normal actor class `$0A`, group `$0C`: initial horizontal speed `$0400`,
deceleration `$20` per frame toward the return direction. Twin vertical speeds
separate by `$10` per frame and clamp at magnitude `$C0`. Normal damage bounds
are `(0,0,13,10)`. A normal pair costs one energy; repeated presses while the
pair is active do not create more pairs in the source runtime.

Charged actor class `$13`, same group: extends 80 pixels from the muzzle in
four-pixel steps, can make a full 64-step orbit, then retracts. Normal stationary
source muzzle measured at player `(14,-4)`, blade fully extended at `(94,-4)`.
Idle extension times out around frame 141 and retracts; exact transition/control
timing still needs a final fidelity pass. A charged release costs three energy.
The initial press used to begin charging can separately emit a one-energy pair.

Animation sequences: 0 normal spin; 1 impact; 4 charged spin; 5 tether extension;
6 tether retraction; 7 muzzle; 8/9 tether rotations. Tether pose 66 points left
in unflipped source art, 42 up, 50 right, 58 down. Actor facing flips matter:
using a world-angle pose and then flipping it again reverses the tether.

## Limits of the current reference traces

The original 100-frame normal/charged recordings are discovery material, not
proof of every charged form. The X2 fixture is near a wall. Some charged forms
have eligibility constraints or activate while held. In particular, Sonic
Slicer, Frost Shield, Gravity Well and Tornado Fang require follow-up source
checks before implementing from those initial traces. Keep verified facts
separate from inferred behavior; add new findings below as weapons are ported.

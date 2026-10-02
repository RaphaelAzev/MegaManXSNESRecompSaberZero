# X3 Zero mod 0.0.1

This optional mod adds X3 Zero to Mega Man X (USA Rev 1). Enable **X3 Zero**
in the launcher's Mods page, select your **Mega Man X3 USA ROM** (`.sfc` or
`.smc`), then launch. The game validates the ROM and prepares the required
assets locally. No Python, manual extraction, or asset download is needed.
The mod is disabled by default. It requires this release's host executable;
copying its manifest into an older host does not install the native plugin.

## Playing

- Enable **Add Zero** and pick the **Starting character** (X by default); that
  character starts the game and appears on the title and menus. Zero uses X1's
  normal controls and earned special weapons.
- Hold fire for progressively stronger charges: small green shot, full shot,
  two-shot sequence, then two shots plus saber at maximum charge. Press fire
  again for each stored follow-up. The saber waits for preceding beams to finish.
- Press **Select** while standing idle on solid ground to exchange X and Zero.
  Original blue/red teleport effects play while stage simulation pauses.
- Each character has separate current health. Health pickups and subtanks heal
  the active character only. Heart tanks increase the shared maximum.
- Active-character death loses a life normally; respawn refills both pools.
- X1 charged special weapons still require the arm upgrade. X keeps native X1
  abilities/progression. Zero has his basic dash and buster/saber abilities.

Zero uses original X3 body, saber, teleport and health badge artwork, including
the pause portrait and title cursor. The title selection shot still works.
The 1-up head deliberately remains X's original artwork.

## Scope and known limits

This is the first playable mod release, not a claim of exact X3 emulation or
complete campaign validation. It retains X1's buster projectile engine and
audio; original X3 projectile/effect/audio fidelity remains follow-up work.
Some scripted poses, ride armor, capsules and unusual terrain still need
broader playtesting. Separate mod save profiles are not yet enforced: keep
your modded saves separate from stock and weapon-development saves.

**X2/X3 boss weapons and simultaneous co-op are not included.** Weapons are
developed on `feat/x2-x3-weapons`; co-op follows only after weapons finish.
The source-ROM selection uses a shared X3 key so the future X3 weapons option
can reuse it automatically. X2 weapons will require their own X2 ROM.

The download includes no ROMs, extracted art, local ROM paths, saved games or
asset caches. See [source provenance](mod-source-roms.md) and the complete
[technical handoff](zero-port.md) for integration details and validation.

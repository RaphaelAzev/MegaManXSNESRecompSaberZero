# Mega Man X 1.7.0

Mega Man X 1.7.0 adds Zero, X2/X3 weapons, co-op and two-player netplay.

## New since 1.6.6

- **Add Zero.** Play as X3 Zero, with his imported animations, buster/saber
  combo and separate health. Press Select while standing on the ground to
  switch between X and Zero. **Starting character** picks who starts the game
  and appears on the title and menus (X by default).
- **Zero slides through tight gaps.** Zero can dash through passages sized for
  X. If the dash ends inside one, he keeps sliding until he has room to stand,
  like the slide in later Mega Man games. Hold the opposite direction to turn
  around.
- **X2 and X3 weapon mods** add all sixteen boss weapons and their charged
  attacks, with adapted enemy damage and extra X1 boss weaknesses. Charged
  special weapons require X1's arm upgrade.
- **Co-op** puts X and Zero on screen together, with separate health, weapons
  and energy, shared subtanks, and a camera that keeps both in view. Choose
  P1's character in Mods; P2 plays the other one. Co-op and Add Zero are
  alternatives: selecting one turns the other off.
- **Two-player netplay** uses the shared lobby and rollback support. Co-op is
  required online. Optional weapons and the original/16:9/21:9/32:9 view are
  set by the room.
- **Simpler Mods page.** Widescreen now has one option, view aspect ratio. Its
  HUD always anchors to the screen edges, and expanded sprite capacity is
  always on. **Password Saves** is on by default.
- Fix a startup stall from Windows diagnostic logging, garbled P2 X sprites
  when P1 plays Zero, and repeatable Highway hitches from graphics streaming.

## Setup

Download the Windows ZIP or Linux AppImage below. Supply your own
**Mega Man X (USA) (Rev 1)** ROM. Zero and co-op also need your own **X3 USA**
ROM; X2 weapons need **X2 USA**. Select these ROMs in Mods; extraction is
automatic. No ROMs or extracted assets are included. SHA-256 hashes are in
`SHA256SUMS.txt`.

For netplay, both players need this same build and their local **Player 1**
input configured. See the
[netplay guide](https://github.com/mstan/MegaManXSNESRecomp/blob/v1.7.0/docs/netplay.md).

## Known limitations

- A full two-machine internet campaign has not been qualified yet.
- After a rollback, audio can differ briefly even when gameplay and graphics
  match (`beads-8wg.2.95`).

# Zero life head

`life-head.png` is the new 16x16 front-facing life icon, with transparency.
Its runtime pixel source is `MmxZeroLifeColor` in `src/mmx_zero.c`; the game
uses the palette extracted from the owner's original X3 ROM. The PNG previews
those pixels. This is an adaptation for the mod, not an original X3 life icon:
vanilla X3 retains X's life icon when Zero is selected.

The initial side-view gameplay-head crop was rejected in playtesting.
The replacement uses a symmetric helmet, green forehead crystal, white cheek
armor, blue eyes and a closed mouth, fitted directly to the native 16px grid.

The built-in image-generation tool was used for a frontal design reference.
The game sprite was then redrawn as the explicit 16px source grid so it has
no resampling blur or off-grid detail. The generated concept is retained locally
in `_research/zero-life-concept.png`; it is not a shipped game asset.

Generation prompt: “Create a game-ready replacement 1-up/life icon: ZERO from
Mega Man X3, viewed straight from the front. Use the attached Mega Man X1
life-head icon only as a style and scale reference. Red helmet, white cheek/ear
armor, central green forehead crystal, fair face and blue eyes. Symmetric frontal
head, no shoulders, no body, no side view. Authentic SNES pixel art on a 16x16
logical grid, enlarged with nearest-neighbor scaling; flat limited colors,
transparent background, no text, one centered icon.”

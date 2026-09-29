# Zero life head

`life-head.png` is a transparent 24x24 front-facing Zero life icon, shared by the weapons menu and life pickups. It replaces the rejected 16x16 head. The extra room preserves the tall helmet fins, low V-shaped brow, cyan crystal, white side blades, gray cheek pieces and simple peach face from the owner's references. It is new mod art, not an extracted X3 life icon.

The built-in image-generation tool produced the reference-guided raster. Asset import cropped its opaque bounds, sampled it onto the 24px grid without interpolation, and reduced it to 16 colors. The runtime uses native BGR555. No eyes or mouth were added. The original concept remains in the local ignored research directory as `zero-life-front-concept.png`.

Run `python tools/compile_zero_life.py` after editing the PNG. This deterministic asset compiler writes `src/mmx_zero_life.h`; no image library is needed at runtime. The larger pickup retains the original collection rules and hitbox.

References supplied by the owner:

- `dacu4gt-2bc6f245-ea43-438a-90e7-3caaa5343b46.png`: frontal Zero helmet identity.
- `512x512.png`: original X life icon's simple pixel-art style.

Built-in generation prompt:

> Use case: style-transfer. Asset: a production SNES Mega Man X 1-up life pickup sprite. Reference 1 provides Zero's front-facing helmet identity and silhouette. Reference 2 provides the VERY simple low-resolution pixel-art language of the original X life icon. Create ONE front-facing Zero head sprite that bridges these references. It must be a genuinely coarse 24 by 24 logical pixel sprite shown enlarged nearest-neighbor, centered, transparent background, no text or extras. Strict 24x24 pixel grid, each pixel a large uniform square, no smaller details. Recognizable long swept angular red helmet fins forming a low V-shaped brow, pale cyan central diamond, dark crimson crown, white side blades, gray cheek/ear blocks, simple peach face opening, dark outline. The forehead fins should point upward and outward, not cat ears. Use the original X icon's restrained blank face style: NO eyes, mouth, nose or facial expression. Only around 12 flat SNES-like colors. Height about 24 logical pixels, width 22. Preserve helmet silhouette from reference 1, but simplify ALL details to match reference 2. No smooth vector curves, no shaded illustration, no antialiasing, no drop shadow, no glow. Actual transparent background.

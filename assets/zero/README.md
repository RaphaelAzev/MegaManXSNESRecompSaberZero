# Zero life head

`life-head.png` is a transparent 17x16 front-facing Zero life icon, shared by the weapons menu and life pickups. It is one pixel wider than X's 16px canvas. The red helmet fins, blue crystal, white eye highlights and dark pupils remain readable at native resolution. It is new mod art, not an extracted X3 life icon.

The built-in image-generation tool produced the reference-guided raster. Import cropped its opaque bounds, sampled onto the native grid without interpolation and reduced it to 16 colors. Final pixel hinting preserves the white pixels beside each pupil and the face between the eyes. The runtime uses native BGR555 colors. This replaces the oversized 24px version.

Run `python tools/compile_zero_life.py` after editing the PNG. This deterministic compiler writes `src/mmx_zero_life.h`; no image library is needed at runtime. Pickup collection rules and hitboxes remain native.

Owner references:

- `dacu4gt-2bc6f245-ea43-438a-90e7-3caaa5343b46.png`: frontal Zero helmet identity.
- `512x512.png`: original X life icon's pixel-art style and eyes.

Final built-in generation prompt brief: preserve Zero's angular red helmet and tall fins; make a compact front-facing SNES life head near X's 16x16 size, with a blue crystal, white eyes and dark pupils, a limited flat palette, hard pixel edges and transparent background. Do not turn the helmet into a recolored X helmet.

Selected generated source: `exec-5c607f14-4679-4487-a6c9-357ce0a02e87.png`. Only the imported native-resolution asset is required by the project.

# Saber Zero upstream footprint

This is the upstream-facing audit for the Saber Zero fork. The comparison base
is `main`; the audit covers the eight upstream files identified for this plan.
The working tree also has line-ending-only noise in `recomp/funcs.h` and
`src/mmx_renderer.h`; that noise is intentionally left alone and is not part
of this report's cleanup.

## Decision record

The following seams are approved shared infrastructure rather than a second
Saber implementation hidden in upstream code:

- The generic `MmxZeroExtension` callbacks: `pre_player`, `player_end`,
  `weapon_tick`, `damage`, `hitbox`, `legacy_intent`, `charge_cap`,
  `collision_rom`, `state_reset`, `legacy_slash_request`, `response`, and
  `burst_origin_y`.
- Generic renderer player-overlay providers and frame snapshots; a world-sprite
  provider with a limit of 8 sprites; and a debug-rectangle provider with a
  limit of 64 rectangles.
- Ride Armor pilot overlay alignment against the native OAM result.
- The character-plugin exclusion guard, interpreted `ExtPrePlayer` and
  `ExtPlayerEnd` dispatch, and the `$049E45` response seam.
- The `apply_zero_hooks.py` dispatcher calls that generate those extension
  dispatch points.
- The `.gitignore` asset exceptions approved by the project owner for the shipped donor assets.
- The single `CMakeLists.txt` include that brings the Saber build and tests into
  the configured project.

These are the approved seams audited below; no upstream gameplay redesign is
proposed here.

## File-by-file audit

### `.gitignore`

Changed hunks:

- The broad `assets/` rule is scoped to the repository asset root, then the
  Saber donor directory, its credits file, PNG sprite files, and OGG sound files
  are explicitly unignored.
- Repository-local generated/private material and cache output receive ignore
  rules.

Purpose: keep the normal asset tree ignored while allowing the shipped Saber
Zero donor assets and their credit file to be tracked. This is the approved
donor-asset exception approved by the project owner; it is not a runtime seam.

Default path: source text is not byte-identical because the ignore policy is
different, but the game and all runtime behavior are behavior-identical when
Saber Zero is disabled. The intended filesystem behavior is deliberately
different only for the approved donor files.

Proof: the `saber-assets` and `saber-package` ROM groups load the shipped donor
sidecars through the package path, and the final worktree audit confirms that
only the approved asset exceptions are visible to Git.

### `CMakeLists.txt`

Changed hunk: one `include(cmake/saber.cmake)` at the end of the top-level build
definition.

Purpose: register Saber sources, generated-hook validation, and Saber tests in
the existing CMake build. It is a build-graph seam, not a change to the native
game's package selection rules.

Default path: the CMake file and resulting executable are not byte-identical to
upstream because the extra sources and tests are compiled. With the Saber
feature disabled, the new extension and renderer providers are unset, so the
runtime path is behavior-identical to upstream.

Proof: `cmake --build build-mingw` compiles every configured target; CTest runs
the generated-hook, Zero, renderer, and Saber unit checks; the ROM runner's
`zero-hook-parity` and `saber-package` groups exercise the disabled/upstream
Zero path and package activation boundary.

### `src/mmx_renderer.h`

Changed hunks and symbols:

- Adds the fixed-width integer include needed by the provider records.
- Adds `MmxRenderPlayerOverlayPlane` and `MmxRenderPlayerOverlay` for an
  optional body/blade overlay, including dimensions, origins, palette, layer,
  and facing.
- Adds `MmxRendererPlayerOverlayProvider`,
  `MmxRendererSetPlayerOverlayProvider`, and
  `MmxRendererPlayerOverlaySnapshot`.
- Adds `MmxRenderWorldSprite`, `MmxRendererWorldSpriteProvider`,
  `MmxRendererSetWorldSpriteProvider`, and
  `MmxRendererWorldSpriteSnapshot`. The implementation accepts at most 8
  records per frame.
- Adds `MmxRenderDebugRect`, `MmxRendererDebugRectProvider`,
  `MmxRendererSetDebugRectProvider`, and
  `MmxRendererDebugRectSnapshot`. The implementation accepts at most 64
  records per frame.

Purpose: give a generic renderer seam to the Saber player art, Saber wave
world sprites, and optional hitbox rectangles. None of these public names are
Saber-specific, so another provider can use the same interface.

Default path: the header is not byte-identical, and the compiled renderer has
new code, but all three providers default to `NULL`. Their snapshots are
inactive/empty, so a disabled Saber package adds no overlay, wave sprite, or
debug rectangle to a frame.

Proof: `saber-render-snapshot`, `mmx_saber_renderer_draw`,
`saber-wave-render`, `saber-hitbox-debug`, and `saber-ride-pilot` verify the
provider snapshots and draw results; the idle renderer checks verify empty
snapshots and the unchanged native path.

### `src/mmx_renderer.c`

Changed hunks and symbols:

- Adds the private `PilotOverlayAlignment` state and reset path. It records a
  frame-local offset and validity for aligning the drawn Ride Armor pilot to
  the native OAM object.
- Adds provider storage, setter implementations, snapshot storage, and the
  frame reset/copy path. `MmxRendererBeginFrame` invokes providers and clamps
  world sprites to 8 and debug rectangles to 64 before drawing.
- Implements the public setter/snapshot functions
  `MmxRendererSetPlayerOverlayProvider`,
  `MmxRendererPlayerOverlaySnapshot`,
  `MmxRendererSetWorldSpriteProvider`, `MmxRendererWorldSpriteSnapshot`,
  `MmxRendererSetDebugRectProvider`, and `MmxRendererDebugRectSnapshot`.
- Adds the private `pilot_overlay_oam_match` and
  `align_wide_pilot_overlay` calculations. They compare the native Ride Armor
  bounds and facing with the authored overlay bounds and apply the measured
  translation.
- Adds the capture reset for that alignment state.
- Adds player-plane row conversion and drawing, world-sprite row conversion
  and drawing, RGB555 debug-rectangle conversion, and the draw branches that
  render the player overlay, skip the native Zero body only while an overlay is
  active, render stage world sprites, and render debug rectangles.
- Keeps the two test-only probes
  `MmxRendererRidePilotBoundsForTest` and
  `MmxRendererRidePilotFacingForTest` in this upstream file. They expose the
  compositor's private measured bounds and facing to the ROM test without
  copying the alignment algorithm into the test.

Purpose: this is the implementation of the generic header seam. The player
provider supplies Saber and Ride Armor art, the world provider supplies the
wave, and the debug provider supplies the optional hitbox overlay. Alignment
stays in the renderer because it depends on the native OAM/compositor result.

Default path: the source and binary are not byte-identical. When providers are
unset, the frame snapshots are empty and the added draw branches are skipped;
the native player/compositor behavior is behavior-identical. The alignment
helpers are also inactive unless a Ride Armor provider publishes an overlay.

Proof: `saber-render-snapshot` checks that one frame holds one copied overlay
snapshot, `mmx_saber_renderer_draw` checks inactive/native output, and
`saber-ride-pilot` checks both measured bounds and horizontal facing. The wave
and hitbox groups cover the other provider paths.

The test-only probes stay here because moving them would either duplicate the
private OAM alignment decision in a test or make the test unable to verify the
actual renderer translation and facing. They do not create a production API.

### `src/mmx_zero.h`

Changed hunks and symbols:

- Adds `MmxZeroLegacyIntent` with `held`, `pressed`, and `released` states.
- Adds the generic `MmxZeroExtension` record with the approved callbacks:
  `pre_player` and `player_end` for frame boundaries; `weapon_tick`, `damage`,
  and `hitbox` for object results; `legacy_intent` for mapped input;
  `charge_cap` for a charge limit; `collision_rom` for temporary collision
  records; `state_reset` for owner lifecycle; `legacy_slash_request` for a
  finisher request; `response` for native collision-response values; and
  `burst_origin_y` for paired burst origins.
- Adds `MmxZeroSetExtension`, `MmxZeroExtPrePlayer`, and
  `MmxZeroExtPlayerEnd`.
- Adds `MmxZeroChargeFlashPaletteIndex` so a provider can select the charge
  palette without changing the existing `MmxZeroBodyColors` result.
- Adds `MmxZeroResponse` as the generic response dispatch function.

Purpose: preserve the existing Zero implementation while giving an owner a
small, explicit extension surface. Saber uses every callback listed above,
including the charge cap, translated legacy intent, finisher request, collision
windows, paired max-shot height, priority response, and damage/hitbox ownership.

Default path: the declarations are not byte-identical, but the extension
pointer is `NULL` until an owner sets it. The existing Add Zero and co-op
activation paths do not install an extension, so their Zero behavior is
behavior-identical to upstream. The new response function returns its original
value when no extension is installed.

Proof: `zero-extension` exercises callback dispatch, `zero-hook-parity`
compares the normal Zero hooks, `zero-response-seam` exercises both response
paths, and the X3 Zero special/charge groups compare the untouched upstream
character behavior. CTest also covers `mmx_zero_character`.

### `src/mmx_zero.c`

Changed hunks and symbols:

- Stores the extension pointer, dispatches the pre-player/player-end callbacks,
  and calls `state_reset` from Zero reset/health lifecycle points.
- Adds `legacy_charge_cap`; the upstream fallback remains 201, while Saber
  returns 200.
- Refactors charge-flash color selection into
  `MmxZeroChargeFlashPaletteIndex`, with `MmxZeroBodyColors` preserving the old
  color result when the index is unavailable.
- Splits burst-sequence lookup into `burst_sequence_for`, adds the native
  emission-Y lookup, and dispatches `burst_origin_y`; Saber uses it to make the
  two max shots share a height, while the null-extension path returns the old
  origin.
- Dispatches `collision_rom` after the native collision-window edits; Saber
  installs its measured Saber collision records and wave records there.
- Builds the default legacy intent, lets an extension override it, and uses it
  for Saber X charge/release ownership. The `legacy_slash_request` callback can
  start the X3 finisher without changing the native Zero owner.
- Routes weapon tick, damage, and hitbox results through their callbacks after
  the existing Zero-owned result has been computed. Adds `MmxZeroResponse`,
  which returns the native result unless an extension changes it.

Purpose: these are the implementation points behind the header seams. The
fallback branches retain the old charge thresholds, projectile cleanup,
per-swing hit protection, damage, hitbox, muzzle, and collision behavior.

Default path: the file is not byte-identical, but with a null extension the
new branches take their upstream fallbacks. It is behavior-identical for the
upstream character packages. The one new interpreted response call is also
behavior-identical because its no-extension result is the native value and its
flags are reconstructed by the plugin exactly as before.

Proof: `zero-hook-parity` covers the normal callback path, `zero-extension`
covers non-null dispatch, `zero-response-seam` runs interpreted and compiled
response cases, and `x3-zero-specials`/the charge groups compare upstream X3
Zero projectile and charge behavior.

### `src/mods/mmx_zero_plugin.c`

Changed hunks and symbols:

- The hook at `$815C` calls `MmxZeroExtPrePlayer` before the existing slide,
  movement, weapon, and Zero-player work.
- The hook at `$8165` calls the existing `MmxZeroPlayerEnd`, then
  `MmxZeroExtPlayerEnd`.
- The interpreted `$049E45` response block now passes the native value read
  from `$86:EF37` through `MmxZeroResponse` and writes the low byte and the
  same Z/N/P flags back to the CPU state.
- `$049E45` is registered with the hook PC list.
- The Zero activation path returns when co-op is active or when the alternate
  Saber character feature owns the shared character mode. The Saber-specific
  comment was renamed to a generic alternate-character comment; the guard and
  its behavior are unchanged.

Purpose: connect the generated/interpreted Zero entry points to the generic
extension without changing the normal Add Zero or co-op owner. The `$049E45`
seam is needed because the priority response must see the pre-i-frame native
value in both compiled and interpreted execution.

Default path: the file and generated plugin are not byte-identical. With Saber
Zero disabled, the exclusion guard is false, the existing Zero activation is
selected, extension callbacks are no-ops, and the response value/flags follow
the upstream path; runtime behavior is behavior-identical.

Proof: `zero-hook-parity`, `zero-response-seam` (both response modes), the
upstream X3 Zero special tests, and the full generated-hook build checks cover
the dispatch points. The `saber-package` group verifies that the separate
Saber activation owns the extension only when its feature is enabled.

### `tools/apply_zero_hooks.py`

Changed hunks and symbols:

- Adds `RESPONSE_BLOCKS` for the high-bank and low-bank response blocks and
  `RESPONSE_PCS` for `$849E45` and `$049E45`.
- Includes the response PCs in `REQUIRED` so a changed generated layout fails
  validation instead of silently losing the seam.
- Finds the native `$EF37` response value and inserts the generated
  `MmxZeroResponse` dispatcher call.
- Extends the generated `$815C` and `$8165` dispatcher calls with
  `MmxZeroExtPrePlayer` and `MmxZeroExtPlayerEnd`.

Purpose: keep the generated hook edits reproducible and make the compiled and
interpreted response paths use the same Zero extension API.

Default path: the generator output is not byte-identical, but its generated
extension calls are no-ops when no owner installs an extension. The generated
upstream Zero behavior is therefore behavior-identical. The required-PC check
also protects upstream regeneration from silently drifting.

Proof: the full build reruns the generated-hook validation; `zero-hook-parity`
checks the dispatcher; and `zero-response-seam` runs both the interpreted and
compiled paths.

## Minimal cleanup and test-only code

The only behavior-neutral cleanup made in an upstream-facing file is the
comment in `src/mods/mmx_zero_plugin.c`: it now describes an alternate
character package instead of naming the Saber package in a shared upstream
file. No production symbol or guard was renamed because the generic Zero API
and the feature IDs are the approved integration contract.

No leftover debug prints or dead production code were found in the audited
hunks. Existing `fprintf` calls are activation/error diagnostics, not probes.
The renderer's two test-only Ride Pilot probes remain in the renderer for the
private-state reason documented above; removing them would lose coverage or
duplicate the compositor algorithm.

## Merge and budget risks

- The renderer change is a large roughly 359-line upstream hunk in a core
  compositor. It has the highest upstream merge-conflict and semantic-review
  risk because it touches frame snapshots, native Zero drawing, world-sprite
  limits, debug drawing, and private Ride Armor/OAM state.
- The renderer provider callbacks publish pointers that must remain valid for
  the frame snapshot. The fixed limits of 8 world sprites and 64 rectangles
  are safe bounds but should remain visible in future merges.
- The `$049E45` response insertion and the generated-PC checks depend on the
  generated instruction shape and bank mirror. Any upstream regeneration needs
  the hook script and both response-mode tests rerun together.
- The upstream Zero header/source now has an extension ABI and lifecycle
  obligation: an owner must clear its extension during its own reset. A future
  owner must not assume `MmxZeroDisable` clears it.
- The single CMake include adds Saber sources/tests even when the package is
  disabled, so build size and target graph are larger than upstream even though
  the default runtime path is unchanged.
- The `.gitignore` change intentionally opens only the approved Saber donor
  asset patterns. Broadening those exceptions would change the project owner
  decision and needs separate review.

No other apparent budget overrun was found in the eight-file footprint. The
renderer size and the generated response seam are the two items that deserve
explicit project-owner review before an upstream merge.

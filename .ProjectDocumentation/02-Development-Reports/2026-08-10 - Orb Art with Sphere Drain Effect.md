---
title: 2026-08-10 - Orb Art with Sphere Drain Effect
date: 2026-08-10
tags: [dev-report]
summary: The user's Health (gargoyle) and Mana (angel) orb compositions replace the vanilla flasks entirely, with a generated drain effect that dims only the sphere - the statue/frame art stays lit. Spheres render at 132px, 50% larger than the vanilla flasks per the user's spec. Vanilla flask rendering deleted.
---

# Orb Art with Sphere Drain Effect

## Context

Immediately after approving the compact middle HUD ([[2026-08-10 - Compact Middle HUD Redesign]]), the user green-lit the orb art pass ("you are reading my mind. go!") with a mid-work addition: make the orbs 50% larger than the vanilla flask placeholders. The two source compositions were already staged: `Health Orb.png` (1254x1254 - a gargoyle perched over a red sphere in a bronze ring, skull ornament) and `Mana Orb.png` (612x408 - an angel resting over a blue sphere). Both shipped with real alpha channels, so no background segmentation was needed this time.

## The drain effect

One source image per orb is enough - the empty state is generated, not authored. At quantization time (same global-palette pipeline as the plate, see [[2026-08-10 - Middle HUD Art Asset Pipeline]]), each orb gets TWO 8-bit surfaces: the bright original, and a dark variant with RGB scaled to 40% **only inside the sphere's measured circle** - the statue, wings, and frame are pixel-identical in both. Each frame draws the dark surface, then reveals the bright one bottom-up across the sphere's vertical span proportional to current HP/mana (full-width row blits are safe precisely because everything outside the circle matches). Empty = fully dimmed sphere under a still-lit statue; full = the original art.

## Measurements and sizing

Sphere circles located by color dominance (red/blue channel tests): health sphere 529px diameter centered (708,726) in source; mana sphere a perfect 178px circle at (272,196). Both scaled to a common **132px screen diameter** (= 1.5x the ~88px vanilla flask): health composition 225x308 on screen, mana 274x283, each pinned 2px off its bottom corner. All geometry (rects, rect-local sphere centers, shared 66px radius) lives in hud_layout as usual; `prep_orbs.ps1` (scratchpad) regenerates the assets at any sphere size by changing one number.

## Vanilla flask code retired

- `control.cpp`: `DrawFlaskTop`/`DrawFlask`/`DrawFlaskUpper`/`DrawFlaskLower` (file-local) and `DrawLifeFlaskUpper`/`DrawLifeFlaskLower`/`DrawManaFlaskUpper`/`DrawManaFlaskLower` (public) deleted, along with `pLifeBuff`/`pManaBuff` and the `ctrlpan\p8bulbs` load. `pBtmBuff` (panel8) survives solely as the multiplayer chat panel's backdrop. `DrawFlaskValues` (the current/max text) survives, now centered on each orb's sphere.
- `hud_layout`: the flask-era `GetHealthOrbAnchor`/`GetManaOrbAnchor` + tune displacements replaced by `GetHealthOrbRect`/`GetManaOrbRect` + sphere-center/radius accessors.
- `scrollrt.cpp`: orbs draw in DrawView's always-on tail (where the flask "upper" halves used to); the "lower" gated draws are gone; value text and the <=640 dirty-rects read the new rects.
- `hud_art.cpp` refactored from single-plate to a small multi-asset structure (plate + 2 orbs) sharing the load/quantize/palette-snapshot machinery.

## Crash on session start (fixed, v1.0.61)

The user's first launch hit a debug assertion before even spawning in town: "operator*() called on empty optional". Cause was an ordering bug in this file's own lazy-loading: assets load on their first draw call, but quantization was a single all-assets pass gated only on palette change. The orbs draw earlier in the frame than the plate (DrawView's always-on tail vs. the belt block), so the first quantize pass ran while the plate's pixels were still unloaded - and since it flipped the "quantized" flag, the plate's surface was never built, leaving its draw dereferencing an empty optional every frame after.

Fixed by loading all three assets together on any first draw, and requantizing whenever a loaded asset is missing its surfaces (rather than on palette change alone), so first-draw order is irrelevant. Belt-and-braces empty checks added before each blit.

## Size reverted to 88px (v1.0.63)

The 50%-larger spheres read as too big in play ("way too big"), so the bump was reverted: both spheres are back to an 88px diameter, matching the vanilla flask orbs they replace. Compositions are now 150x205 (health) and 183x189 (mana). Regenerating is a one-number change in `prep_orbs.ps1`; the sphere centers were re-measured on the newly processed assets rather than scaled arithmetically, since the ring/claw occlusion makes derived values unreliable.

The drain effect itself was confirmed working in play at this point - a mid-fight screenshot at 28/70 HP showed the sphere's upper portion correctly dimmed with a clean waterline and the gargoyle fully lit.

## Orb art v2 (2026-08-11, v1.0.71)

The user replaced both orbs, having found the first pair's gargoyle and angel too large against their spheres. In v2 the sphere dominates and the ornament (a dragon head, a winged mask) is a pedestal beneath it.

Two things made these harder to process than the plate:

**No alpha channel.** Unlike "Middle HUD v2.png", these exported as flattened 24-bit RGB with the transparency painted in as a near-white backdrop (244-255, desaturated). A flat brightness threshold was not an option: each sphere carries a pure-white specular highlight that would have been punched straight through. Removed instead by flood-filling inward from the image borders, which cannot reach an enclosed highlight, followed by three passes shaving the anti-aliased fringe off the silhouette.

**Scale by the glass, not the core.** The first attempt sized each orb by its coloured core, which left health with a 104px sphere and mana with a 128px one - visibly mismatched, because the core is a different fraction of the glass in each. The glass circle is now solved geometrically instead: the sphere's top touches the content's top edge and the ornament only appears lower down, so shallow rows are pure sphere, and for a circle with its top at depth 0 a row at depth `y` has half-width `hw` with `r = (hw² + y²) / 2y`. Taking the median across ten depths gives a clean radius immune to the wings below and to single-row noise. Both spheres now render at 88px - matching the flask orbs they replaced - with compositions 105x96 (health) and 97x96 (mana).

The drain effect needed the glass circle for a second reason: dimming only the core would have left the glass rim permanently lit.

This processing moved from PowerShell into a small C# tool (`scratchpad/OrbPrep.cs`, compiled with the in-box .NET compiler) - the flood fill and per-pixel passes are far too slow through `GetPixel`/`SetPixel`, and PowerShell's array-literal parsing kept mangling the arithmetic.

## Orbs anchored to the plate (v1.0.73)

Corner-anchoring was the original design brief, but in play on a wide canvas it stranded the orbs far out at the screen edges, disconnected from the rest of the HUD. They now sit flush against the middle plate's outer edges - health to its left, mana to its right - bottom-aligned with it, so the whole HUD reads as one centred unit at any resolution and the spheres rise above the shorter plate.

The gap is a named constant (`OrbGapFromPlate`, currently 0) should the orbs ever want breathing room from the plate.

## Sphere centers corrected (v1.0.62)

The user's screenshot from the crashed build showed both spheres fully bright at 1/70 HP and 0/10 mana. That turned out to be the crash rendering garbage (dereferencing the unbuilt surface after "Ignore"), not a drain bug - the effect was validated offline by replicating DrawOrb's exact math in PowerShell against the processed assets (`scratchpad/sim_drain.ps1`), which produced correct dimmed-top/bright-bottom results at every fill level.

That validation did expose a real calibration error: the first-pass sphere centers were estimated from source-space colour bounding boxes, which the bronze ring and gargoyle claws skew. Re-measured directly on the processed assets (`scratchpad/measure_processed_orbs.ps1`) and corrected: health (138,179) -> (132,182), mana (97,134) -> (99,134). The radius was always exact, being fixed by construction at prep time.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.62. Awaiting the user's screenshot. Watch-fors: the drain reveal line's look mid-fill (hard horizontal edge by design - a wave/meniscus would need dedicated art), the red sphere's quantization against the global palette's limited red ramp, composition overlap with the char panel/stash flyouts at 960px (the left panel bottom sits well above the orb, but worth an eye), and value-text legibility over the bright spheres.

## Related

- [[2026-08-10 - Compact Middle HUD Redesign]]
- [[2026-08-10 - Middle HUD Art Asset Pipeline]]

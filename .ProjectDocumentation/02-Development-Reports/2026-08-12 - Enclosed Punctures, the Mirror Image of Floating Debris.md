---
title: 2026-08-12 - Enclosed Punctures, the Mirror Image of Floating Debris
date: 2026-08-12
tags: [dev-report]
summary: The Iron Helm icon shipped full of small holes inside its own silhouette - not the open face, but scattered gaps enclosed by opaque material on every side. Root cause is the exact mirror image of the floating-debris bug fixed earlier this session for the other six icons - a thin connecting detail thinning below survival during downscale, just inverted (a hole loses its bridge to the background instead of a fragment losing its bridge to the item). Fixed as an opt-in pipeline pass, not a global default, after finding it would have silently overturned a different icon's already-reviewed design call.
---

# Enclosed Punctures, the Mirror Image of Floating Debris

Sent the shipped Iron Helm icon on a white background per a previous request. The response was direct: "this asset is full of punctures within the contour of the item. I don't approve it."

## Distinguishing real holes from a diagnostic artifact first

The immediate prior turn had shown a *deliberately* blackened version of the same icon - every transparent pixel manually flipped to black, as a way of visualizing the alpha channel on request. Worth ruling out before doing anything else: was "full of punctures" describing that diagnostic overlay (which was never in the shipped file), or the actual asset? Checked directly - one connected-component pass over the real `oracool_items.cel` frame found exactly one opaque island, 1,406 pixels, no floating debris. That answered the wrong question, though: the report was about *holes*, not detached fragments, and a single-connected-component check says nothing about whether that one component has enclosed gaps in its middle.

The right check is a border flood fill through the transparent pixels: anything reachable from the canvas edge is true background; anything left over is sealed in on every side by opaque material. Run against the shipped icon, that found 291 enclosed pixels - both a small cluster near the browband and, it turned out, the entire face-opening area. Both were genuinely there, not diagnostic artifacts.

## Same bug as the floating debris, opposite polarity

The debris-and-torn-edges pass earlier this session ([[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]) found that a ~4x downscale into a 56px cell thins fine connecting details below the surviving-alpha threshold, leaving small opaque fragments stranded with no bridge back to the main shape. This is the same failure, inverted: at full source resolution, the helmet's shadowed interior is validly connected to the crop's actual background by a thin dark channel. That channel is exactly the kind of detail the downscale erases. Once it's gone, whatever was on the far side - background, at cut time - is stranded the other way: still transparent, but with no path left to the true outside, sealed in by opaque material instead of floating loose from it.

Fixed with a new Pass 4 in `ItemIconCel.cs`'s `PostProcess`: flood-fill from the final 56px cell's own four edges through transparent pixels, then fill anything unreached with opaque black. Deliberately symmetric with the existing debris sweep - same connectivity logic, opposite fill direction.

## Why this isn't a global default

Re-running the full seven-icon build with the new pass unconditionally on showed the belt's own opaque-pixel count jump by 212 - far more than any of the other five. Checked before assuming that was correct: an earlier pass this session ([[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]) had explicitly reviewed the belt's hollow middle and confirmed it against the source art as intentional - "a closed loop with the buckle visible and daylight showing through the middle, the same way a real belt looks laid flat." Looking at the actual source crop again now, that middle region does show real leather-grain texture, which arguably argues the *opposite* conclusion - it may be underside material that the original cut simply couldn't tell apart from a dark background, not deliberate daylight. But a plausible alternative reading isn't the same as being sure enough to overturn a specific call that was already made, checked against source art, and written down. Applying the new pass to all seven would have silently re-decided that for the user, as a side effect of a fix for a different icon nobody had asked to revisit.

Made the pass opt-in instead: a `fillPunctures` flag threaded from a new tenth spec field, through `PostProcess`, defaulting to off. `build_item_icons.cmd` enables it only for the helm's spec. Verified by decoding both the old (git `HEAD`) and newly-built `oracool_items.cel` and hash-comparing all seven frames individually: the six worn-item frames came back byte-identical, only the helm's changed.

## Verification

Same closed-loop MPQ check as every prior asset change this session: extracted the packed CEL back out of the rebuilt `oracool.mpq` and hash-matched it against source. Debug build clean at `ORACOOL_VERSION` **1.1.25**, tests **349/351** (same two pre-existing failures). Rendered the corrected icon on both white and the actual dark panel tone for final review - the scattered browband punctures are gone and the face opening is now a clean solid fill rather than a checkered window, with the belt's own already-reviewed gap left completely untouched.

## Related

- [[2026-08-12 - The Iron Helm, a New Item at an Old Slot]]
- [[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]
- [[2026-08-12 - Item Icon Quality Pass]]

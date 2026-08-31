---
title: 2026-08-12 - Green-Screen Chroma Key Replaces the Blanked Six
date: 2026-08-12
tags: [dev-report]
summary: A batch of ten green-background renders arrived - the full nine-piece set from the earlier ChatGPT prompt, plus an overview sheet. Sorted and processed with a new chroma-key extraction path, deliberately kept separate from the existing dark-canvas pipeline. Result un-blanks the six item icons pulled in v1.1.22 and replaces the vignette-sourced helm, all with zero enclosed punctures and zero manual threshold tuning - the class of bug chased twice this session structurally can't occur with a green background.
---

# Green-Screen Chroma Key Replaces the Blanked Six

Ten more files landed in the art vault's root, with instructions this time: analyze, rename, sort, process - and when processing, "only make transparent the GREEN pixels."

## What arrived

Nine individual renders, each a single item on a flat green background, plus a "Leather Set v1" composite sheet showing all nine together as an overview. This is the payoff of the ChatGPT prompt drafted earlier this session for a full nine-piece replacement set: shoulders, bracers, gloves, belt, legs, boots (the six pulled in [[2026-08-12 - Pulling the Disliked Armor Icons Pending a Redo]]), plus a redone helm, plus armor and shield (two pieces with no Oracool item at their slot yet). All nine individual renders are a consistent 1254x1254; sorted into `02-source-art/items/` with `-v2`/`-v3` suffixes matching whichever version number wasn't already taken there, the composite into `03-concepts/canvas-previews/` alongside the project's other assembled-mockup sheets.

## Why a green background changes the pipeline, not just the input

Every icon-quality investigation this session - torn edges, floating debris, then enclosed punctures - traced back to the same root cause: the existing extraction method (`ExtractWithAlpha`) separates item from background by brightness, and a dark item next to a dark background is fundamentally ambiguous by brightness alone. The only fix available was connectivity (flood fill from the canvas edge), and connectivity has its own failure mode once a downscale thins away the thread connecting two regions that were validly joined at source resolution - a bridge disappearing leaves either a floating island or a stranded hole, depending on which side of it you're standing on.

A green background sidesteps the ambiguity entirely: nothing in this item family - leather, bronze, iron - is remotely green, so "is this pixel green" cleanly separates background from content with no connectivity analysis required, and therefore no downscale-thinning failure mode to chase. Added `ExtractWithGreenKey` and `ContentBoxByGreenKey` to `ItemIconCel.cs` as a parallel path, selected per spec via a new `mode` field (`ItemIconCel.exe`'s spec syntax grew a corresponding tenth/eleventh field) - `ExtractWithAlpha` and every existing dark-canvas spec are completely untouched, so nothing already shipped is at any risk from this addition.

The key itself is a soft ramp on green-minus-max(red,blue) rather than a hard cutoff (thresholds measured directly off three of the actual renders: background sits at excess 190-245 everywhere sampled, comfortably clear of any plausible item colour), so anti-aliased edge pixels get partial alpha instead of a binary in/out decision - PostProcess's existing edge compositing toward the dark panel tone then handles blending them, reused rather than reinvented.

Result, checked with the same border-flood-fill puncture detector built for the helm fix: **zero enclosed punctures on eight of nine icons**, and 9 stray pixels on boots - two orders of magnitude below the belt's confirmed-real 212px gap, left alone rather than auto-filled. No `fillPunctures` needed anywhere in this batch; no per-image threshold tuning either, unlike every prior single-render icon this session.

## What shipped and what didn't

Seven of the nine slot into item types that already exist in `itemdat.cpp` - the six worn slots (currently `IDROP_NEVER` placeholders, blanked since v1.1.22) and Iron Helm (shipped v1.1.23-25, sourced from a dark-vignette render). Swapping their art needed no new engineering, just pointing `build_item_icons.cmd`'s specs at the new green-screen sources - un-blanking the six and replacing the helm's source in one pass.

Armor and shield are genuinely new: no Oracool item currently occupies `ILOC_ARMOR` or `ILOC_SHIELD`. Cut and verified clean (0 punctures each) so they're ready to review, but deliberately not wired into `oracool_items.cel` or `itemdat.cpp` - matching the exact two-step precedent the helm itself set (process and preview first, "ship it as an item" is a separate, explicit decision with its own naming/stats/tier questions).

## Verification

Same closed-loop chain as every asset change this session: decoded the packed `oracool.mpq` back out and hash-matched `data\inv\oracool_items.cel` against source, confirmed all three asset channels (loose, oracool_assets, Debug build mirror) agree. Debug build clean at `ORACOOL_VERSION` **1.1.26**, tests **349/351** (same two pre-existing failures as every build this session).

## Related

- [[2026-08-12 - The Iron Helm, a New Item at an Old Slot]]
- [[2026-08-12 - Enclosed Punctures, the Mirror Image of Floating Debris]]
- [[2026-08-12 - Pulling the Disliked Armor Icons Pending a Redo]]
- [[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]

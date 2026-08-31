# Batch 01 — the Paladin gets colour (v1.9.66)

**Date:** 2026-08-26
**Version:** 1.9.65 → 1.9.66

---

## The drop

Found by sweeping, and **not where the standing note said to look**. The zone is the sibling
`Diablo/Oracool.MPQ/`, one level above the project, not `Diablo Orcl V1/Oracool.MPQ/`.

`colorful-skill-icons-batch-01-paladin-combat.zip` — 26 MB, 32 entries. Eleven finished icons, the
2 MB source renders they were cut from, a manifest, a QA report, contact sheets, and an
`ENGINE_HANDOFF.md` that states the constraints back:

> Final individual cells are 56x56 RGBA with a fully opaque 48x48 centre and exact 4-pixel
> transparent margin. Alpha contains only 0 and 255. Visible RGB values use town palette indices
> 192-207 and 255. Frame indices are 0-8, 29, and 30.

Every one of those is a constraint this engine actually has — the hard alpha cutoff at 128, the
128-255 matchable palette range, the 56×56 cell. The art arrived already fitting the hole.

## What the frame numbers mean

0-8 are the Paladin's Combat Skills page. 29 and 30 are Hammer of Faith and Blessed Shield — the two
rows appended to the END of the Paladin block in 1.7.40, which is why they sit apart from their
page-mates. The numbering matches the reference sheet generated on 2026-08-25, so the batch indexes
straight into `ClassTreeIconIndex` with nothing to translate.

## The strip had to grow

The shipped `paladin_tree_icons.png` was **31 frames** — the Paladin block as it stood before the
Passive Skills page appended 18 more rows. The block is 49 now.

So this batch is also the change that makes the strip the right length. `DrawStripIcon` derives its
cell size from the strip's height and refuses an index past the end, which is why the passive rows
have been drawing bare plates rather than reading garbage — and why widening the strip is safe.

## Compositing, and why not just use the supplied strip

The batch ships a 49-frame review strip with the eleven icons in place and everything else
transparent. Using it directly would have been one copy — and would have **erased the twenty aura
icons at frames 9-28**, which are not in this batch and are not coming back on their own.

So the new strip is built by composition: the existing 31 frames first, then the eleven new cells
over the top, then transparency to frame 48.

Raw `LockBits` pixel copies throughout, never `DrawImage`. Any resampling or alpha blending would
invent colours outside the 192-207/255 set the art was authored against, and the quantiser would
then map them somewhere arbitrary.

## Verified rather than assumed

| check | result |
|---|---|
| strip is 49 frames | 2744×56 ✓ |
| frame 1 (Smite) vs the source icon | **0 differing pixels** |
| frame 29 vs the old strip | 2304 differing — exactly the 48×48 centre, so it replaced |
| frames 12 and 20 (auras) vs the old strip | 0 differing — preserved |
| alpha values present | only 0 and 255 ✓ |
| frame 40 (a passive) | fully transparent ✓ |

The 2304 figure is the useful one: 48×48 is the opaque centre the handoff specifies, so the replaced
cell differs over exactly the area it should and nowhere else.

MPQ repacked into both build trees — a normal build does not do it.

## Still placeholder

Frames 9-28 are the auras, which keep their existing art. Frames 31-48 are the Passive Skills, which
have no art at all yet and continue to draw an empty plate with a red X. The other five classes are
untouched; this batch is the Paladin's Combat Skills page only.

---
date: 2026-08-16
version: 1.7.28
area: The inventory and stash window backgrounds
---

# Two Painted Panels, and Checking Their Numbers

`oracool-inventory-stash-background-pack-v1.0.0` — a matched pair of 340×720 painted panels,
"Cathedral Reliquary": near-black stone under gothic arches with thin gold tracery, drawn 1:1 with
no scaling.

The panels the fork has been wearing were **procedural** — `DrawThemedFill` under
`DrawOrnateBorder`, the shared treatment every window got when the composed stone panel was retired.
These two windows now wear real art instead. Every other window keeps the theme.

## The interesting part was verification

The pack's `geometry-spec.md` names `Source/oracool/inventory_layout.h`, `Source/inv.cpp` and
`Source/qol/stash.cpp` as its "authoritative sources". The user's clarification is that it was
generated from **screenshots**, so those citations are the generator's framing rather than files it
read — which makes checking the numbers the job rather than a formality.

They check out. Derived from the headers rather than taken on trust:

| Element | Pack | Header computes |
|---|---|---|
| Panel | 340×720 | `InventoryPanelSize {340,720}` |
| Item grid | (30, 428, 280, 196) | `(340−280)/2 = 30`; `624 − 7×28 = 428` |
| Tab strip | (30, 400, 280, 28) | `TabRowY = GridOrigin.y − 28 = 400` |
| Shoulders x | 65 | `EquipColCentre − HeadFlankGap − 56 = 142 − 21 − 56` |
| Amulet x | 219 | `142 + 56 + 21` |
| Stash grid | (25, 161, 290, 464) | `StashGridLeft = (340 − 10×29)/2 = 25` |

The one that would have been easiest to get wrong it got **right**: the stash grid's pitch is
**29px**, not the inventory's 28 (`StashCellPx = INV_SLOT_SIZE_PX + 1`), and the spec calls that out
explicitly with "do not normalize this". A screenshot-derived spec that had assumed 28 would have
drifted a pixel per column — ten by the last one.

## Background only, on purpose

The art carries no text, no slot boxes and no grid — just the panel, its arches and its border. The
title, class silhouette, equipment slots, grid, tabs and footer are all still drawn in code on top,
which keeps the layout the **code's** rather than the art's. That is the same separation that was
established when the old composed panel was retired for baking the class figure in and giving every
class the same one.

## Two safety notes

**Skip-zero is safe here.** The blit uses `BlitFromSkipColorIndexZero`, and this art is fully
opaque, which normally raises the question of whether a real colour could quantize to index 0 and
punch a hole. It cannot: `NearestGlobalPaletteIndex` only ever searches indices **128-255**, so a 0
in the output can only have come from genuine transparency.

**The art is droppable, not required.** Both windows keep the procedural fill and bevel as a
fallback behind `HasInventoryPanelArt()` / `HasStashPanelArt()`. Delete either PNG and that window
returns to the shared theme rather than rendering as a hole.

## What was not run

The pack ships `tools/build_assets.py`, `tools/verify_assets.py` and a
`docs/reference-implementation.patch`. None were executed or applied — the PNGs are the deliverable,
and wiring them by hand against the real layout header is both safer and how every coordinate above
came to be checked.

## Verified

**418 tests, the same two pre-existing failures.** MPQ repacked to 80 files. The zip's own SHA-256
matches the `.sha256` beside it.

Worth a look in play: the two panels open together at 1280×720 and the arches should frame the grid
without crowding the outer equipment columns — the pack's own overlay preview says they clear, but
that is its render, not the engine's.

---
title: 2026-08-12 - Seven More Sets, the UI Kit, and Two Bugs the Audit Found
date: 2026-08-12
tags: [dev-report]
summary: Seven new armor sets (63 items, taking the total to 143), the modular stone panel/frame kit, and the v3 tab buttons all imported. Auditing on the way through turned up two real defects - a silent uint8_t overflow that had already shipped, giving the game's best armor its worst durability, and neighbouring-cell bleed that left 45 of 143 icons off-centre and under-scaled.
---

# Seven More Sets, the UI Kit, and Two Bugs the Audit Found

Three asks: ship the seven new armor sets, ship every element of the modular frame kit, ship one specific tab-button sheet. Two more arrived mid-flight - audit the existing items for centring, and make the belts bigger.

## The overflow that had already shipped

Before generating 63 more rows on the same formula, checked whether the formula was safe at the top of its range. It wasn't, and hadn't been for a version already in the user's hands.

`ItemData::iDurability` is `uint8_t`. The Diamond tier's multiplier produced 264 and 270:

| Item | Table says | Actually stored |
|---|---|---|
| Diamond Leggings | 264 | **8** |
| Diamond Armor | 270 | **14** |

The two best armour pieces in the game had the worst durability of anything in it - worse than plain leather. Nothing warned: the build was incremental and had not recompiled `itemdat.cpp`, so no narrowing diagnostic was ever emitted, and a wrapped value looks like a perfectly ordinary small number in the table. Found by scanning every `AllItemsList` row's numeric fields against the width of the member each one lands in, rather than by reading. Both clamped to 255, and the generator for the new tiers now clamps every `uint8_t`-bound field and hard-fails on anything that would still overflow, so a future tier cannot reintroduce it quietly.

That constraint also set the ceiling for the new tiers: armour's `iMaxAC` base of 17 means a multiplier above ~15 saturates AC, so the seven new multipliers stop at 13.4 rather than continuing the earlier curve's steeper climb.

## Neighbouring-cell bleed: 45 of 143 icons off-centre

User report, mid-flight: "some of them don't align in the center of their slots, but in the right or left." Measuring every icon's content bounding box against its cell centre found 45 off by 2px or more, and the margins named the cause: content jammed hard against one edge with a large gap opposite (`onyx_legs` at L0/R20, `fallen_shield` at L16/R0).

The composite sheets' cells are plain thirds of the canvas, so a neighbouring item's edge often intrudes a few pixels into a cell's search box. `ContentBoxByGreenKey` took a plain min/max over every non-green pixel, so the box included that sliver. The fit then centred *that* box - pushing the real item to one side - and scaled it down to make room for a fragment that `PostProcess`'s island sweep deleted a moment later anyway. Both symptoms, one cause.

Fixed at the cause: the content box is now computed from connected components, keeping only those at least 25% of the largest. The threshold is measured, and deliberately *not* an "is it touching the cell edge" test, which would have been wrong - a legitimate second boot touches the edge too. Component sizes as a fraction of each cell's largest:

- real paired items (two boots, two gloves): **85.7%, 97.5%**
- bleed slivers: **4.7%, 2.5%, 2.2%, 1.5%, 0.2%**

Nothing observed lands between 5% and 85%, so 25% sits in a wide empty gap. Result: bleed dropped in 86 of 143 cells, off-centre count **45 → 0**, and 55 icons gained coverage because they no longer shrink to accommodate a sliver. Verified no paired item lost a piece - the largest loss anywhere was 4.4%, consistent with removing bleed rather than content.

## Belts

"Some belt icons are super tiny. Make them bigger. Don't worry about the belt sticking slightly out of its slot." The bleed fix had already recovered most of this on its own (`iron_belt` went from 33x17 in a 56x28 cell to 54x28), but belts are still the one shape where a contain-fit leaves the cell short: a belt is a wide ring whose widest point is its horizontal diameter. Added an optional per-spec `fitScale` that deliberately overflows the cell and lets the draw clip the excess, set to 1.25 for all 16 belts. Fifteen of sixteen now fill the cell completely; `obsidian_belt` is the exception, and its detached strap-tail piece falling outside the frame is an improvement.

(`fitScale` parses with `InvariantCulture` on purpose - this machine's locale uses a comma decimal separator, so a plain `double.Parse` would have rejected or misread `"1.25"`.)

## The seven sets

Ruby, Onyx, Glacial, Cyborg, Fallen, Seraphic, Spectral - 63 items, taking the Oracool total to 143. Names are single distinctive words rather than the sheets' titles ("Red Diamond" -> Ruby, "Angelic Gold" -> Seraphic) so each gives a unique, unambiguous `give*set` prefix that does not collide with the existing nine.

Two layout variants had to be handled per sheet, verified by cropping and inspecting every cell rather than trusting the thumbnails: Red Diamond, Black Diamond and Ice follow the standard grid, while Cyborg, Dark Angel, Angelic Gold and Ghostly have **helm and shoulders swapped**. Blind reuse of the existing rects would have put helmets on the shoulder slot for four of seven sets. Cyborg needed one further special case: its shoulders cell holds two separate pauldrons, and per the user's instruction only the right-hand one is cut (measured: both together give a 408x224 box that leaves each pauldron ~26px wide in a 56x56 icon; the right one alone is 190x224 and doubles the icon's coverage).

A splice bug caught in passing: the first attempt at inserting the new entries into `cursor.cpp` used "insert after the last match" for both the width and height arrays, and both anchors resolved to the same line - producing 80 widths and 206 heights. Caught by counting the parallel arrays before building, not by the compiler.

## The UI assets

**Tab buttons** (`ui\inventory_tabs_v3.png`, 280x84). Deliberately built to the exact shape of the existing `inventory_tabs.png` - a 10x3 grid of 28x28 cells - so it is a drop-in candidate rather than a new format. The important detail is the row order: `DrawInventoryTab` is called as `(..., selected ? 1 : 0)`, so row 0 must be **inactive** and row 1 **active**, while the source sheet is laid out Active | Inactive | Clicked left to right. Copying the columns straight across would have inverted every tab highlight in the game, so the mapping is explicit in the cutting script.

**Frame kit** (18 PNGs, `ui\panel_frame_*`). A tileable background texture plus 3 horizontal bars, 5 vertical bars, 5 corners, 2 T-shapes and 2 crosses. Element rects come from connected-component detection, and each extracted file's opaque pixel count matches its detected component area exactly, which is the check that the rects are right. Only sections 1 and 2 of the sheet ship - the assembled examples and the "HOW TO BUILD" diagrams are documentation of how to combine the pieces, not assets.

The corners are named `corner_1..corner_5` rather than top-left/top-right/etc. That is a deliberate refusal to guess: the block yields five components where a "corners" label suggests four, and one of them (62x99, visibly larger than its neighbours) is two shapes that touch. A wrong orientation label is worse than no label when the panel is assembled, so the naming stays neutral until the layout work confirms each one.

## Where everything lives

All of it ships inside `oracool.mpq` (now 32 files, 1.48 MB): item icons as frames in `data\inv\oracool_items.cel`, UI art as individual PNGs under `ui\`. As always the build writes to all three channels - `oracool_assets` (the only tree the packer reads), the loose `assets` fallback, and the Debug build mirror.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.30**, confirmed embedded in the exe. Tests **350/352** - the same two pre-existing failures as every build this session. Closed-loop MPQ check on five representative files (the icon CEL, the tabs sheet, and three frame-kit elements): extracted back out of the packed archive and hash-matched against source, all identical.

Not verified: how any of this looks in a running game. The icon previews are palette-accurate, but the frame kit and tab buttons are not wired into any UI yet - that is the next piece of work, by design.

## Related

- [[2026-08-12 - The Eight-Tier Set Expansion]]
- [[2026-08-12 - Green-Screen Chroma Key Replaces the Blanked Six]]

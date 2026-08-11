---
title: 2026-08-11 - Burger Menu Icon Row
date: 2026-08-11
tags: [dev-report]
summary: The burger menu's text list becomes a row of 11 three-state icons cut from the user's art sheet, with the lit state doubling as a "this panel is open" readout. Includes the click-routing gap that left the icons inert on first build.
---

# Burger Menu Icon Row

## Choosing between the two sheets

The user supplied two icon sheets and asked which to use. **v1** (grey / gold / red-glow) was recommended over **v2** (grey stone, three brightnesses) on four grounds, the first being decisive:

1. **State legibility at size.** These render at ~30px. v1 separates its states by *colour*, which survives downscaling intact; v2 separates by brightness alone, and its Inactive and Clicked states are both darkish - at 30px they would be near-indistinguishable. A functional problem, not a stylistic one.
2. **Palette.** The plate and orbs are dark stone with bronze/gold trim. v1 continues that material language; v2's cool silver-grey reads as a different set.
3. **The states match the behaviour.** Six of eleven entries are toggles that stay open, so v1's third state maps naturally onto "this panel is currently showing".
4. **Contrast.** v1's solid silhouettes beat v2's embossed dark-on-grey at small sizes.

v2's one advantage was a correct icon set (no Chat or Friendly Fire, both dropped from the menu earlier). Immaterial - those two are simply left unextracted from v1's sheet.

Both sheets drew **Zoom In and Zoom Out as separate buttons** where the menu carried a single "Toggle Mini-Map Zoom". The art was the better design and `MiniMapZoomIn`/`MiniMapZoomOut` already existed, so the toggle was split - which lands the menu at exactly 11 entries.

## Extraction

`scratchpad/IconGrid.cs` located the grid by row/column projection, then `IconPack.cs` cut and packed it. Two things needed care:

- **Threshold.** The first pass used a brightness cut of 45 and detected only the icon glyphs, because the button plates are dark charcoal (15-40) on a pure-black sheet (0-3). Dropping the cut to 8 resolved the plates; the band filter (height and row-sum) then separates buttons from caption text.
- **Aspect.** Rows 1-2 are portrait (~78x103) and row 3 square (~90x98), so no single cell shape fits both without visible distortion. Each button is scaled to a common 33px height with aspect preserved and centred in a uniform 30x33 cell. The three mini-map icons therefore come out slightly wider than the other eight; correcting that properly is an art-side change.

Rounded corners are cut by flood-filling the black backdrop inward from each crop's corners. Output is `assets/ui/menu_icons.png`, 90x363 - column = state, row = entry - loaded through the existing `hud_art` asset machinery, so it shares the palette-quantisation and requantise-on-palette-change behaviour.

## Behaviour

The row sits centred on the plate (350px against its 356), just above it, clearing the XP counter. Grey idle, gold hovered, lit when the entry's panel is open; momentary actions borrow the lit frame for a 140ms flash. Hovering names the icon in the cursor tooltip, since the icons are wordless.

The row **stays open** after a click. With a text list, closing on selection made sense; with state-showing icons, staying open lets several panels be toggled in one pass while watching them light up. Click away, or the Menu cell, to dismiss.

## The icons were inert on first build

Clicking did nothing, while hovering worked. `CheckHudMenuClick` was only reachable from `LeftMouseDown`'s *world* branch - which is where the "click away to close" case lives - but the row had just been added to `isOverHud`, so clicks on it went to the HUD branch instead, which only knows about the plate. The row fell in the gap: HUD enough to be skipped by one branch, not plate enough to be seen by the other. Hovering was unaffected because that runs through `CheckPanelInfo`, an entirely separate path.

Fixed by testing the row explicitly ahead of the HUD/world split. The world branch keeps its click-away-to-close role.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.76. Needs a play-test of each of the eleven actions, and confirmation that the lit state tracks panels being opened and closed by other means (keyboard shortcuts) as well as by the icons.

## Related

- [[2026-08-11 - Skills Deferred, HUD Click-Through and Tuning]]

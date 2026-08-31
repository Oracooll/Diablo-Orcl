# The Plate That Fit

**Version:** 1.8.40
**Date:** 2026-08-19
**Tests:** 470 total, 468 passing. The two standing baseline failures only.

## v3 is the drop-in the limestone package was not

Two hours ago Unit E was resized Small -> Medium because the limestone package did not fit: 1497x297
content at aspect 5.04 against the plate's 5.53, 24bpp RGB on black needing keying, and eight slot
openings that did not line up with the source rects the layout maps. Three jobs, one of them a
rewrite of the HUD's hit-testing.

`hud-plate-v3.png` is a different proposition entirely. Measured before anything was touched:

| | limestone v1.0.0 | **hud-plate-v3** | current art |
|---|---|---|---|
| content | 1497 x 297 | **1505 x 274** | 1505 x 272 |
| aspect | 5.04 | **5.49** | 5.53 |
| format | 24bpp RGB on black | **32bpp ARGB** | 32bpp ARGB |

**1505 is `PlateSrcSize.width`, exactly.** v3 is drawn on the same source grid the layout already
uses.

## Verified before writing any code

The claim that mattered was not the canvas but the eight slot openings. So: crop v3 at its alpha
bounding box, scale to 356x64, and draw the **existing** `LmbWellSrc`, `RmbWellSrc` and the six
`BeltCellSrcX` rects over it - scaled the way `ScalePlateRect` scales them, edges first.

All eight hug their openings. Not approximately: the green well rects sit inside the LMB and RMB
frames and the six red belt rects sit inside theirs, on both axes.

That is the whole difference between this being an afternoon and being a line in the backlog. The
measurement came first and it decided the size of the job, rather than the job being started and the
measurement discovered halfway through.

## One constant moved, and it moves nothing

```cpp
constexpr Size PlateSrcSize { 1505, 274 };   // was 272
```

`ScalePlate` divides by **width** only, so no source rect shifts by a pixel. The height feeds
`PlateScreenSize.height`, and 272 and 274 both scale to 64. Cropping to 272 instead would have
clipped two rows off the bottom frame to avoid touching a constant that does not matter.

The `static_assert`s in `hud_layout.cpp` - the ones tying `LevelUpIconSize` and `SkillWellIconSize`
to the scaled well rects - still hold, which the build proves rather than the reasoning.

`PlateBottomMargin` stays 0. The standing note from every bottom-HUD package is *do not add a
procedural bottom offset*; the art is drawn screen-bottom flush.

## The Menu and Portal icons

v3 leaves those two boxes blank on purpose - the plate supplies the frame, the runtime supplies what
sits in it. `DrawBurgerMenuButton` and `DrawTownPortalIcon` already blit the three-state strips cut
in v1.8.38 into belt cells 0 and 5, so the new icons land in the new boxes with no further wiring.

Composed and checked at 4x: both sit centred in their cells with clearance inside the frames, the
portal's hover state included.

## `tools/CutHudPlate.ps1`

New, and it asserts rather than trusts. Before cutting it checks the master is 1536x1024, that the
row above the crop is empty, and that the first row of the crop is not. If a future master moves the
band, the script fails loudly instead of quietly cutting a different crop and leaving the layout
pointing at the wrong pixels - which is the failure that would look like a rendering bug and be
traced back to the wrong place.

The crop is written out (16, 336, 1505x274) rather than re-detected each run, for the same reason:
a silent re-detect makes every run a new measurement.

## Files

- `tools/CutHudPlate.ps1` - new.
- `Packaging/resources/oracool_assets/ui/middle_hud.png` and the `assets/ui` mirror.
- `Source/oracool/hud_layout.cpp` - `PlateSrcSize.height` 272 -> 274, with why.
- `Diablo Orcl V1/07-Backlog/Pipeline.md` - Unit E shipped. 34 rows.

## To look at

The plate is the one thing on screen that everything else is anchored to, so the check is whether
anything anchored to it moved: the LMB and RMB skill icons should sit centred in their wells, the
four belt potions in cells 1-4, the level-up plaque directly above the LMB button, and the XP bar
below the row. If any of those drifted, `PlateSrcSize.height` is the only constant that changed and
the only place to look.

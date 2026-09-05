# The dark layer over the character sheet (v1.9.284)

**Date:** 2026-09-06
**Request:** (a cutout of the sheet) "the entire area in the attached cutout from hero stats sheet to be covered by dark transparent layer to increase readability of hero stats screen."

## History

The side panels had a dark backdrop from 2026-08-19 until 2026-09-02, when the user had it removed from all six canvases after the stone was recut dark. This brings it back for the character sheet alone; the removal note in hud_art.h still stands for the other five.

## What was done

- `oracool::SidePanelCanvasInner` (hud_art.h): the 340x720 canvas's inner opening, panel-relative, {22,25} 296x670 - the bezels end at x=21/318 and y=24/695 as measured on 2026-09-05.
- `oracool::DrawSidePanelDim(out, origin)`: two half-transparent passes over that opening, the ~75% the books and the item tooltip use.
- `DrawChr` calls it right after the canvas art and before the title, so the title band is under it too, as in the cutout. The themed fallback (no art) is untouched.

## Test

`OracoolAudit.TheCharacterSheetDimCoversTheOpeningAndSparesTheBezels` - on a white 340x720 surface every pixel inside the opening darkens and every pixel on the bezel band stays white; the edges are asserted at 22/318 and 25/695.

Suite 631/632, the standing dungeon-generation failure only.

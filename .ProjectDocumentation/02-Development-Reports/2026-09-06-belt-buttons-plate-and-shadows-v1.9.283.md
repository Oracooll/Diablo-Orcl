# The plate under the belt buttons, and item shadows (v1.9.283)

**Date:** 2026-09-06
**Request:** "place backing also in portal/burger menu slots. try to render shadows behind potions when placed in the belt to feel more 3D."

## What was done

- `DrawTownPortalIcon` and `DrawBurgerMenuButton` (hud_art.cpp) call `DrawBeltSlotPlate` on their cell before blitting the icon, so all six cells of the row wear the same black-grey-gold plate, in every button state.
- `oracool::DrawBeltItemShadow(out, position, sprite)`: the item sprite through an all-black palette table, drawn half-transparent (`ClxDrawBlendedTRN`) two pixels down and right. `DrawInvBelt` draws it after the plate and before the outline and the item, so the potion lifts off the plate. Transparent runs are skipped by the sprite encoding, so the table need not spare index 0.

## Test

`OracoolAudit.TheBeltItemShadowIsTheSpriteSilhouetteOffsetTwo` - the flask drawn on white: every opaque sprite pixel darkens the pixel two right and two down, no transparent one does, and the first two rows and columns stay white. The mask is filled with a sentinel because the flask paints a few pixels as literal colour 0.

Note for the file: tests that mount the archives and call `InitCursor` cannot share one process (the second `InitCursor` after a `FreeCursor` faults); ctest runs each test in its own process, so the suite is unaffected, but `--gtest_filter` across two of them will crash.

Suite 630/631, the standing dungeon-generation failure only.

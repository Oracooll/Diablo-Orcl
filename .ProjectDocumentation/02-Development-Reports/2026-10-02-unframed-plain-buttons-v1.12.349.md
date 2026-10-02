# 2026-10-02 - Frames off the plain buttons (v1.12.349)

**Date:** 2026-10-02. Debug only. All 915 tests pass. The Debug `diablo.ini` was unchanged by ctest. Commit 652287dd.

## What the user asked
- "frames around buttons in artisans ui's that dont use vanilla gold backing should not have that frame"
- Side tabs: "dont touch side tabs".
- Salvage: "remove frames from Salvage page of Griswold".

## What changed
- **"Vanilla gold backing"** was read as the gold stone plate, `ui\shop_button_frame.png`.
- **Frames removed** (from v1.12.315):
  - Gillian's four reroll choices.
  - Ogden's YES/NO.
  - Griswold's Salvage page: the 8 plates and CONFIRM/CANCEL.
- **Back where they were:** each of these buttons returns to its place before v1.12.348. That version moved them only to clear a frame.
  - Gillian's rows are full width again.
  - YES/NO is at (110,626).
  - The Salvage plates are on rows 344/410.
  - CONFIRM/CANCEL sit 16 px above the results frame's foot.
- **Frames kept**, because these buttons sit on the gold plate:
  - Gillian's dice and Imbue icons;
  - Ogden's arrows;
  - both Transmutes;
  - the vendors' service rows.
- **Side tabs:** untouched, as the user asked.

## Checked
- Renders from `OracoolPreview.DISABLED_ButtonFrameGaps`: salvage confirm, Gillian's reroll offers and Ogden's runes confirm.

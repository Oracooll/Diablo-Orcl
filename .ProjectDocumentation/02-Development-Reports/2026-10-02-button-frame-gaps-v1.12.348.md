# 2026-10-02 - Button frame gaps (v1.12.348)

**Date:** 2026-10-02. Debug only. All 915 tests pass. Commit 139fe133.

User: "now that we introduced frames around vendors/artisans buttons you need to move them a bit in in order to have a 6px gap between their frames and surrounding ui elments."

## How it was measured
- **The frame:** `DrawButtonBezel` reaches 6 px past every button. The drop shadow reaches 3 px further on the left and bottom only, so it stays inside the gap.
- **The renders:** `OracoolPreview.DISABLED_ButtonFrameGaps` (in the audit test) draws 23 window states before and after the change.
- **Edges:** frame edges were found by comparing each render with its canvas PNG.
- **Painted edges:** these were sampled from the canvas files:
  - stone border inner edge at x 21 and x 318;
  - vendor grid band from y 159;
  - Gillian's big frame from y 289;
  - Ogden's board band from y 429;
  - craft well foot at y 543;
  - canvas foot at y 695.

## What moved
Only position constants changed, so every hit test and every draw still comes from the same rect function.

- **Griswold, Wirt and Adria service rows** (`ShopServiceSlotAt`):
  - y 121 → 113.
  - Left group 24/60/96 → 34/70/106; right group 210/246/282 → 200/236/272.
- **Griswold's Refresh until:** (120,627) → (120,643).
- **The vendor tab column:** it stands 12 px off the panel (`ShopTabColumnClearance`). It is shared by the shops, the Salvage page, both workshops and the Cube.
- **Gillian:**
  - Icon row: top 247 → 243, right anchor 172 → 161, first plate 22 → 34.
  - Price line: `ServicePriceGap` 2 → 12.
  - Alternatives:
    - inset 4 → 13;
    - width -8 → -26, so the rows are 18 px narrower (a compromise: the board box cannot grow);
    - first row 26 → 33.
- **Ogden:**
  - Upgrade/Downgrade plates: y 391 → 383. `BoardTitleLift` 70 → 62 keeps the title in place.
  - YES/NO pair: (110,626) → (101,631). It sits left of centre to clear the orb (a compromise).
  - Craft Transmute: y 546 → 556. Levski's cube page got the same change.
- **Salvage:**
  - Plates up 2 px.
  - CONFIRM/CANCEL bottom gap 16 → 12.

## Not moved
- **Hidden controls:** the shop page arrows never draw (one page). The fallback control row and the old Cube skin's plates draw only when canvas art is missing.

## Needs a look in game
- The service rows.
- The tab column, now off the panel.
- Gillian's narrower alternatives.
- Ogden's off-centre YES/NO pair.

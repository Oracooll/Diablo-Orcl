# 2026-10-02 - Tab frames off, Griswold's row spread (v1.12.350-351)

**Date:** 2026-10-02. Debug only. All 915 tests pass at both builds. The Debug `diablo.ini` was unchanged by ctest.

## v1.12.350: no frames on the side tabs
- **The request:** "i now notice there are frame around the tabs buttons. remove them."
- **Frames removed:** `DrawSideTabGround` is gone, and with it the frame on every tab column:
  - the vendors';
  - Griswold's Salvage page;
  - Gillian and Ogden;
  - the Cube.
- **Column position:** `ShopTabRect` is flush with the panel again. v1.12.348 had moved the column 12 px out to make room for the frames.

## v1.12.351: Griswold's service row
- **The request:** "the 6 top griswold buttons look to crowded, we need to introduce gaps there". The user chose an even single row.
- **New positions:** six plates across x 34..305, at 34/82/129/177/224/272. That puts 13-14 px between plates, and their frames now stand 1-2 px apart instead of overlapping.
- **The limit:** six 34 px plates cannot have 6 px between frames inside this border.
- **Adria:** her one Recharge plate shares slot 3, so it moves with the row from x 106 to x 129.
- **Renders:** `scratchpad/gaps351`.

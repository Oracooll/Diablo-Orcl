# A darker outline, a bigger Cube title, Wirt's grids full, Wirt's window closes at three tiles (v1.12.094)

**Date:** 2026-09-20 · **Version:** v1.12.094 · **Tests:** 831/831

Four asks in one message (user, 2026-09-20), each one place in the code:

1. **"make the gold outline way darker."** `GridFrameGold` (inv.cpp) moves from `PAL16_YELLOW + 4` to `+ 10`, deep in
   the yellow ramp; the backing test asserts the new index.
2. **"increase font size of Levski's Cube title."** The painted Cube UI's title is `FontSize30` (was 24) in a band of
   40 px at y 30 (was 36 at y 32), still centred where the sample's plate sits.
3. **"Fill entire grid with stock in Wirt's shop/gamble grids."** `BOY_ITEMS` and `GAMBLE_ITEMS` go from 12 and 11 to
   60 each - over-supplied like Griswold's shelves - and `SetupTownStores` cuts both tabs to one page with
   `TrimShopStockToOnePage`, so each 10x16 grid comes out full with no hidden reserve. The gamble cycles its eleven
   slot types (`GambleSlots[i % 11]`), and its entries are trimmable now (they were `neverTrim`); a bought gamble
   slot still restocks in place with the same base, so the page stays full.
4. **"Auto-close Wirts vendor screen when i walk away 3 tiles."** `UpdateStoreState`'s walk-away distance is three at
   Wirt's (`TOWN_PEGBOY`) and five at every other counter, as before.

# 2026-09-27 - The hint card on every vendor and artisan button (v1.12.210)

**Date:** 2026-09-27. Debug only. The user said "check dev notes and process". Two notes were open:
- **Barbarian axe (open):** still waiting on the details asked for in v1.12.208.
- **Hint cards (done):** "apply vendor buttons new tooltip design on all vendors/artisans all tabs every possible button which currently has hover tooltip."

The outcome is in `development-archive.md` under "Batch of 2026-09-27 (fifth)". The engineering follows.

## How a hover becomes a card

- **The mechanism:** a hover producer fills the panel strings, then calls `ShowPanelStringsAsHintCard()` (`cursor_tooltip.cpp`). `DrawCursorTooltip` draws the card while `InfoString` is still that text. Any later change falls back to the plain tooltip.
- **Where it was already used:** since v1.12.208, only `SetShopHoverInfoString`'s service buttons.
- **New:** `HintCardRequested()`, the same comparison, exported for the test.

## Where it is now

- **`SetWorkshopHoverInfoString` (`workshop.cpp`), Ogden's and Gillian's workshop:** Reroll, Imbue, Remove, Cleanse, Upgrade, Downgrade and the board's plates. The bench and craft cells are item slots and keep the item tooltip.
- **`SetLevskiHoverInfoString` (`levski_roar.cpp`):**
  - the Cube's recipe rows;
  - the Salvage page's item icon;
  - Recipes and the Salvage All buttons. These two were title-only, so each gains a body line.
  - Transmute still returns no text, by the user's 2026-09-12 request.
- **`SetServiceHint` (`shop_grid.cpp`):** Repair All's and Refresh Until's split sentences are joined, so the card's word-wrap lays them out.
- **Refresh Until's explainer:** it now carries what `DrawRefreshUntilHoverTooltip` (`stores.cpp`) used to say. That function draws into the HUD info box, and it is now off whenever `IsShopGridScreen(stextflag)`. The Magic tab has been a grid for a long time, so its row-geometry hit test named a row that is not on screen. The new `ShopRefreshUntilLookingFor()` (`stores.h`) exposes the file-local target parse to the card.

## Test

- **`OracoolAuditV188.EveryArtisanButtonHoverIsAHintCard`** sweeps these windows every 2px, and any point that answers the hover must be a card:
  - both workshops;
  - the Cube's first page, which has no hover text by design and is only checked for plain tooltips;
  - the Cube's Recipes page, opened through the side tab's press and release;
  - Griswold's Salvage page.
- **Its first run failed** on the Cube, which answered nothing: the first page has no hover text by design, and the recipes are on the second page. The test now turns to that page.

## Also this turn

- **Vanilla Sound Stand-ins page:** it offers five vanilla candidates plus "None of the above" for every place (Version 2).
- **Bard:** removed from the page at the user's word (Version 3, 505 places). The Bard is hidden in the game.

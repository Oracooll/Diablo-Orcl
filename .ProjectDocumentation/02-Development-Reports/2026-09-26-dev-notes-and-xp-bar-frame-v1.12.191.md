# Dev notes (8), and the XP bar's frame

2026-09-26 — v1.12.191

## What was asked

Eight dev notes, each archived in `development-archive.md` with its outcome, plus one request typed in chat: "put a grey frame around the exp bar + verticals on each 10%".

## What changed

- **Ctrl+click on a stat's plus spends 5 points** (`control.cpp` `ReleaseChrBtns`, `diablo.cpp`). It is capped by the points you have and by the stat's cap. Shift+click still spends everything.
- **Mendicant Shrine** (`objects.cpp`) converts half of the backpack's and the stash's gold together, adding the two in 64 bits and clamping the half to `int`. `TakePlrsMoney` already takes from the backpack first, then the stash.
- **Stash right-click gates** (`qol/stash.cpp` `UseStashItem`):
  - A signet at 20/20 is refused, as it already was from the backpack.
  - Guardian Keystones and Sealed Maps now open from the stash, and are consumed only once they open. Before, the stash path handed them to `UseItem`, which has no case for either, and then removed them: they were lost for nothing.
- **Runeword book** (`oracool/runeword_book.cpp`): the filter tooltips are drawn last, sized to their text, on a darker plate.
- **The item's kind word** (`items.cpp` `GetItemTypeNoun`) comes from the base before the ItemType:
  - spear, pike, lute, quiver, canticle, focus;
  - the Necromancer's wands, scythes and heads.

  The Pike stays `ItemType::Axe`, which gives the hero's polearm animation and the axe rules. Only its word is fixed, so a Pike unique no longer reads "unique axe" under its pike icon.
- **Tooltip card** (`oracool/cursor_tooltip.cpp`):
  - The name is drawn, then every pixel it painted is brightened in its own colour (`DrawStringLifted`): 30%, or 55% for the rare yellow.
  - The plate, frame, rule, footer edge and socket bullets wear the item's quality colour. The new `ItemQualityRimColor` in `inv.h` is the backing's own colour.
  - In the subtitle, the tier is drawn in the colour its own "Tier:" line had before the card.
- **XP bar** (`qol/xpbar.cpp`):
  - A grey 1 px frame (`PAL16_GRAY+7`) sits a pixel outside the bar, with its corners cut. The bar's ends are square now, because the frame does the rounding.
  - Nine darker grey marks (`PAL16_GRAY+10`) sit at 10%–90%, drawn over both the fill and the groove.

## Verified

- The v1.12.191 Debug build is clean. Three test programs were blocked by Smart App Control and relinked.
- The full suite passes, 847 of 847.
- The `OracoolPreview` render shows the card in each item's colour, with the tier colours kept.
- Nothing here has been seen in the game yet.

# Levski's Cube gets two tabs — v1.12.131

**Date:** 2026-09-21
**Version:** v1.12.131 (v1.12.129 and v1.12.130 failed to build; see **Two failures** below)
**Branch:** renderer-32bit

## The request

> Tab Cube - `Levskis Cube 340x720.png`
> Use Griswold Refresh button as Transmute button here, placed under the cube, dead center, 4px away.
> Tab Recipes - `Levskis Cube Recipe Panel 340x720.png`
> Put the list with recipes within the Frame. Lay a transparent dark layer under the recipes.

The word "tab" had no mechanism behind it: the Cube window has never had a tab column, and its
recipes sat in a bezel on the same page as the grid. Asked, and the user chose **two tabs like the
vendors'**.

## Measured first, both pages

| | |
|---|---|
| Cube page, grid frame | x 118..224, y 406..541 |
| the well inside it | rules at x 156/185/214 and y 444/473/502 |
| → grid | **3×4 at (128,416)**, 28px cells on a 29px pitch |
| Recipe page, frame band | y 289..628, opening **x 29..310, y 306..618** |

The grid's pitch is the Roar's own, so **nothing in the transmute logic changed** — the painting
simply draws the wells the code used to draw itself.

## The tabs

Drawn with the **shop's own column**, exported for the purpose:

```cpp
Rectangle GetSideTabRect(int index);
void DrawSideTab(const Surface &out, int index, string_view label, bool active, bool pressed);
```

Not a copy. The Cube docks in exactly `GetShopPanelRect()`, so a column of its own would be the same
furniture a few pixels off — the difference the eye catches when flipping between a vendor and the
Cube. Same art, same sink, same `titlemov` on entry, same press/release rule (the page turns on the
mouse-up, and only inside the tab that was pressed).

The window reopens on the Cube tab: its grid is emptied on close, so a remembered Recipes page would
open on a list with nothing to come back to.

## Transmute is Griswold's Refresh plate

The same two files his shop draws — `ui\shop_button_frame.png` and `ui\shop_glyph_refresh.png` — not
copies. Position derived, not typed:

```cpp
// centre of the painted grid frame, 4px below its foot
{ (118 + 224 + 1) / 2 - 34 / 2, 541 + 1 + 4 }   // = (154, 546)
```

So a recut canvas moves the button with it.

## The Recipes page

Fifteen rows at the list's own 20px pitch, centred in the 312px opening. The dark layer the user
asked for is **two passes** of the half-transparent blend over the opening and nowhere else — the
frame stays as painted, the same rule every canvas this day follows. One pass left the recipe names
competing with the stone floor behind them.

Two structural pieces made it safe:

- **`CurrentListLines()`** — the list's length is a number now, not the `ListLines` constant. The
  bezel skins fit eight rows beside a grid; this page fits fifteen. A page whose draw and whose
  scroll clamp disagreed about its own length would lose the last recipes to a scroll that would not
  go far enough.
- **`PageHasGrid()`** — asked by `CellAt`, which is the single chokepoint every hover and every click
  on a cell passes through. One test there takes the grid out of the hit map; the draw's own gate
  takes it off the screen. The grid's CONTENTS survive the switch, because `GridItems` is window
  state and not page state.

## Two failures, both mine

**v1.12.129** — `#include "utils/string_view.hpp"`. The real path is `utils/stdcompat/string_view.hpp`.
I wrote the include from memory instead of copying it from a header that already had it
(`text_render.hpp`, `ornate_border.h`). Cost: one full build.

**v1.12.130** — `GetSideTabRect` and `DrawSideTab` were inserted immediately before
`DrawShopTabColumn`, which is **inside shop_grid.cpp's anonymous namespace**. Declaring them in the
header and defining them with internal linkage links cleanly in that TU and fails at the link step
with two unresolved externals. Moved below the `} // namespace` beside `DrawShopTabColumnFor`, which
is where every other exported definition in that file lives.

The lesson in both is the same shape: "insert before X" put the code in the wrong scope, and
"the include is probably called Y" guessed at a path. Neither is caught by reading the diff — only
by the compiler and the linker, one build apiece.

## Files

- `Packaging/resources/oracool_assets/ui/cube_page_canvas.png` — new, 402882 bytes.
- `Packaging/resources/oracool_assets/ui/cube_recipes_canvas.png` — new, 413407 bytes.
- `Source/oracool/shop_grid.h` / `.cpp` — `GetSideTabRect`, `DrawSideTab`, and the tab loop's body
  lifted into the latter.
- `Source/oracool/levski_roar.cpp` — the two geometries, `CubeTab`, the tab draw/press/release,
  `CurrentListLines`, `PageHasGrid`, the Griswold Transmute plate and the dark layer.

**Assets added — `tools\build_oracool_mpq.cmd` run, 603 files.**

## Worth a look in play

- The tabs sit in the vendors' column, to the RIGHT of the window. Both pages are 340×720 and docked,
  so the column is where Griswold's is.
- Both canvases must be installed for the tabs to appear: a tab leading to a missing page is worse
  than no tabs, so the pair is all-or-nothing and a build short of either keeps the old workshop page.

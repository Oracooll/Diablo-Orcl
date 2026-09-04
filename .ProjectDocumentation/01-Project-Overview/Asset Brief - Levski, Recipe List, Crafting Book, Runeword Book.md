# Asset Brief - Levski's Roar, Recipe List, Crafting Book, Runeword Book

**Date:** 2026-09-05 · Four ChatGPT prompts, one per window, written against the geometry the code draws today so the deliveries slot in through the existing cutters. The HUD they must match is GPT's own `02-raised-stone-slot-design.png` (delivered-packs/diablo-bottom-hud-v1), in the game since v1.9.217.

Every prompt opens with the same STYLE and FORMAT blocks so the four read as one family.

## Shared blocks

```text
STYLE - match the bottom HUD already in the game (the reference image attached): cold grey carved
stone, aged dark gold and blackened iron trim, thin gold pinstripe rims around every opening (the
same rim the HUD's belt cells wear), gothic tracery, a restrained gargoyle or lion motif at most
twice per piece. Dark, worn, low saturation. No glow, no neon, no clean vector edges, no modern
fantasy-UI gloss. Openings that will hold items or text are flat near-black stone with nothing
painted inside them.

FORMAT - one PNG per deliverable, TRUE 32-bit alpha (transparent background, not a painted
checkerboard - the last delivery painted one and it had to be keyed out). Front-on, orthographic,
no perspective, no drop shadow outside the silhouette. Given at 3x the on-screen size stated below;
keep detail readable at one third: bold shapes, nothing thinner than 3 px at full size. No text
anywhere unless the prompt says so - the game draws its own labels and titles.
```

## 1. Levski's Roar (the transmute window)

On screen 426x557. Layout is the second painting's, which the code is already cut for (`tools/CutLevskiRoarSkin.ps1`, header `levski_roar_skin.h`).

```text
Design the window for "Levski's Roar", a transmutation altar in a Diablo 1 style action RPG.
[STYLE block] [FORMAT block]

CANVAS: 1278 x 1671 (3x of 426 x 557). The whole canvas is the window; a carved stone frame with a
gold pinstripe inner edge runs round all four sides, about 36 px wide at full size.

LAYOUT (full-size px, top-left origin):
- HEADER, y 0-330: a carved lion's head centred at the top, a title plate below it 950 x 130 at
  (164, 150) - a flat dark stone plate with a gold rim, EMPTY (the game draws the title).
- ITEM GRID, x 114-786, y 498-1338: a well of 8 columns x 10 rows, each cell 84 x 84 with a 3 px
  gold pinstripe between cells. The cells' floors are flat near-black stone. The well has its own
  gold rim.
- SALVAGE COLUMN, x 861-1164, y 561-1329: a column header plate 303 x 80 at (861, 480), EMPTY,
  then seven identical button plates 303 x 93 stacked with 15 px gaps starting y 561. Each is a
  raised stone plate in a gold rim with a small dark gem set at its left end - seven gems in
  seven colours top to bottom: white, blue, gold, orange, green, red, purple. No lettering.
- TRANSMUTE BUTTON, x 144-705, y 1416-1530: a wide raised plate, its stone tinted dark blood red.
- RECIPES BUTTON, x 771-1170, y 1413-1527: a raised plate with a small closed-book carving at its
  left end.
- CLOSE, x 1104-1182, y 138-222: a small square plate with a carved X.

STATE SHEETS: for each of the ten plates (close, transmute, recipes, seven salvage) also deliver a
sheet with the plate in three states side by side, left to right - default, hover (rim lit),
pressed (plate sunk, darker) - each state cropped to the plate's own rect, on transparent ground,
at the same 3x scale. Name them close, transmute, recipes, salvage-1 .. salvage-7.

Deliver: levski-roar.png (the window) and the ten state sheets.
```

## 2. Recipe List (the page beside Levski's Roar)

On screen: a scroll page docked to the left of the Levski window, 300 wide and the window's height (557), capped to the screen and scrolled inside. Today it is drawn as a flat panel with text rows.

```text
Design a recipe scroll that hangs beside the Levski's Roar altar window, same family.
[STYLE block] [FORMAT block]

CANVAS: 900 x 1671 (3x of 300 x 557). A stone tablet with a gold pinstripe inner edge, its frame
about 30 px at full size, and a slightly lighter, flatter stone interior than the altar's - this is
a reading surface. The interior is EMPTY: the game writes eighteen recipes into it as text rows
about 40 px tall each at full size, so no rules, no lines, no ornament inside the reading area
(x 36-864, y 120-1600). A small carved header band 900 x 90 across the top holds no text. Along the
right edge inside the frame leave a plain 24 px vertical channel for a scrollbar the game draws.

Deliver: levski-recipes.png. Also deliver a 3x3 nine-slice version of the same tablet as separate
files - four corners 120 x 120, four edges 120 long - so the game can stretch it to any height.
```

## 3. Crafting Book

On screen 944x616: header 28, subtitle 20, then recipe rows 56 tall, 12 px padding all round. Opened from the burger menu.

```text
Design a "Crafting Book" window for a Diablo 1 style action RPG - the page a crafter's recipes are
listed on. [STYLE block] [FORMAT block]

CANVAS: 2832 x 1848 (3x of 944 x 616). An open ledger of stone and iron rather than paper: a carved
frame with a gold pinstripe inner edge, about 36 px at full size, around one continuous flat dark
interior (NOT two facing pages - the list runs the full width).

LAYOUT (full-size px):
- HEADER BAND, y 0-84: a carved band with a centred title plate 600 x 60, EMPTY.
- SUBTITLE STRIP, y 84-144: plain, slightly lighter stone, EMPTY - the game writes one line here.
- ROWS, from y 156: the list area x 36-2796, y 156-1800 is flat near-black stone with NO rows
  painted - the game draws 56 px rows and their dividers itself. Leave it empty.
- Along the right edge inside the frame, a plain 24 px vertical channel for the game's scrollbar.
- CLOSE, a small square plate with a carved X at the top right corner, 78 x 84.

Also deliver one ROW PLATE as its own file: 2760 x 168 (3x of 920 x 56), a low raised stone bar
with a gold pinstripe rim, in two states side by side on one sheet - default and hover (rim lit).
The game tiles it under each craftable recipe.

Deliver: crafting-book.png, crafting-row-states.png, and the close plate's three-state sheet.
```

## 4. Runeword Book

On screen 944x616: title 22, a row of ten slot-filter keys 20 tall, a row of rune icons 32 tall, then five columns of entries 184 wide, each entry a title line, a host line, the recipe's rune icons and stat lines.

```text
Design a "Runeword Book" window for a Diablo 1 style action RPG - the book that lists every
runeword, filtered by equipment slot and by rune. [STYLE block] [FORMAT block]

CANVAS: 2832 x 1848 (3x of 944 x 616). A heavy tome bound in iron and dark stone with a gold
pinstripe inner edge, frame about 30 px at full size, one continuous flat dark interior.

LAYOUT (full-size px):
- TITLE BAND, y 0-66: carved, with a centred title plate 520 x 50, EMPTY.
- FILTER ROW, y 90-150: ten identical KEY PLATES in a row across the width, each 84 x 60, small
  raised stone tabs with a gold rim, EMPTY (the game writes Weapon, Shield, Armor... on them).
  Deliver the key plate separately as a three-state sheet: default, selected (rim and face lit
  gold), hover.
- RUNE ROW, y 168-264: a plain recessed strip the full width, flat near-black, where the game
  draws rune icons 28 px square. Nothing painted in it.
- ENTRY AREA, x 30-2802, y 300-1800: flat near-black stone, EMPTY. The game lays five columns of
  entries here. Do NOT paint columns or dividers.
- A small square toggle plate 54 x 54 at the right end of the filter row, with a carved eye - the
  "possible runewords only" switch - as a two-state sheet: off and on (lit).
- Along the right edge inside the frame, a plain 24 px vertical channel for the game's scrollbar.
- CLOSE, a small square plate with a carved X at the top right corner, 78 x 84.

Deliver: runeword-book.png, filter-key-states.png, possible-toggle-states.png, and the close
plate's three-state sheet.
```

## What will happen with the deliveries

- Levski: the existing cutter measures the painting and generates the header; state sheets slot into the hover/pressed mechanism already in `levski_roar.cpp`.
- Recipe List, Crafting Book, Runeword Book: each is drawn today as a flat panel plus text. Each needs a `DrawLoosePng` background under its existing drawing and its rects re-anchored to the painting's interior - a unit of work per window, on the pattern of the fourth and fifth HUD plates. The nine-slice for the recipe scroll is what lets it keep its dynamic height.

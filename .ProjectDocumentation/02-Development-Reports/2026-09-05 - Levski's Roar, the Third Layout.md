# Levski's Roar, the Third Layout (v1.9.253)

**Date:** 2026-09-05 · **Request:** "take levskis roar.png from oracool.mpq and take the icon sets chatgpt prepared and combine them into a working levski's UI. we are aiming at the following concept - 3 row of 3 icons (9 total) to the right of the grid to cover all necessary buttons."

## The pieces

- **The painting** is the user's own `Oracool.MPQ\levskis roar.png` (261x241, with the `.pdn` beside it): a 3x4 grid on the left at a 29px pitch (26px interiors, 3px rules) and an empty stone field on the right. Left in the drop-zone root as the live source, since it is the user's working file.
- **The icons** are GPT's `oracool-levski-icon-controls-v3` pack (filed under delivered-packs with `-compact-v2`, its predecessor): nine 32x32 plates including their bezel, DEFAULT / HOVER / CLICK each. Seven split gems for the salvage tiers, circular arrows for Transmute, a book for Recipes. The pack's salvage order is the game's `SalvageTier` order exactly.

## What the cutter does now

`tools/CutLevskiRoarSkin.ps1` scales the painting uniformly by 28/29 (252x233) so the painted cells become the game's 28px item cell, with the grid origin one painted pixel up-left of the first interior so the cell straddles the rules evenly. The nine controls sit in a 3x3 block at a 35px pitch, centred in the field and on the grid's height. It copies the 27 state files to `ui\levski_<stem>_{default,hover,pressed}.png` and generates `levski_roar_skin.h`:

| Item | Window px |
|---|---|
| Grid origin, cell | (34, 89), 28 |
| Block columns | x 130, 165, 200 |
| Block rows | y 94, 129, 164 |
| Close | (231, 32) 18x18 |

Block reading, row-major: Whites, Magic, Rare; Uniques, Primal, Set; Ethereal, Transmute, Recipes.

## What the window does now

- The painting carries no plates for the controls, so the game draws the DEFAULT frame at rest and swaps in HOVER or PRESSED. The press flash and the idle half-shade (nothing to salvage, no recipe ready) are unchanged.
- The close button is the game's red X, drawn at the skin's rect through the new `DrawWindowCloseButtonAt`, at the frame's top-right corner like every other window.
- Hovering a control puts its meaning in the info panel: "Salvage all Whites in backpack", "Transmute", "Recipes". The salvage line names the backpack because that is what the button acts on.
- The grid is 3x4 again (12 cells), so the capacity rules from 2026-08-19 apply as written: one armour, or twelve runes.

## Verification

Debug build clean; 624/625 with the standing `Drlg_l1` failure. A 2x mock with the game's cell rects overlaid shows the 28px cells landing on the painted interiors and the block clear of the field's edges. In the game: open the stand in town, hover each icon for its line, click a tier with nothing to salvage and see the shade, put a 2x3 armour in and see it fill six cells.

## v1.9.255: the SALVAGE title, rows spread

"Add a nice SALVAGE title above the icon. Gold, with text shadow. Appropriate font size. Also spread the rows vertically as you see fit." A 22px band at the top of the field carries SALVAGE in the 24px gold font with the hero sheet's text shadow, centred over the block; the three rows below it sit at a 36px pitch (was 35), which is what the 135px field affords with a 4px margin at the bottom. Rows now at y 102, 138, 174. The tooltips were already there since v1.9.253: the hover line goes through the cursor-following tooltip panel.

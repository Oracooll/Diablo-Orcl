---
version: v1.12.118
date: 2026-09-21
area: UI / inventory, towner dialogs
tests: 832/832
---

# The inventory gets its own framed canvas, and the last two long names go

## The asks

> take this canvas and apply it to the Inventory screen - "New Inventory Canvas.png"
> And still open for your call: Wirt's and Farnham's dialog headings are the only long NPC names left on screen -
> fix it.

## The canvas measures to the grid

Opening at **x 29..310, y 422..618** - the inventory's 10x7 grid of 280x196 at (30,422), with the same one pixel
of black around it the 10x16 canvas had - and the ornate frame painted immediately outside.

Measuring it needed a different threshold from last time: this opening is dark TEXTURED stone (lum 11-12 with
cell lines at 30), not the flat black of the grid canvas, so a `lum <= 12` cut caught only fragments. And the
horizontal scan had to move off the centre row, which on this canvas lies in the upper stone band rather than in
the opening.

## A THIRD canvas file

`ui\panel_bg_inventory.png`, beside `panel_bg.png` and `panel_bg_grid.png`. It cannot reuse either:

| Canvas | Grid it frames |
| --- | --- |
| `panel_bg.png` | none - six windows with no grid at all |
| `panel_bg_grid.png` | 10x16 at (30,170) - the shop shelves and the stash |
| `panel_bg_inventory.png` | 10x7 at (30,422) - the inventory |

The inventory's grid is a different size AND a different place, so one file cannot carry both frames.

It is tested BEFORE the shared panel, which carries no frame, and the older `HasInventoryPanelArt` rung stays
below both. The code-drawn bezel is skipped where the painted one is up; the fill and the 1px cell rules are
still the code's, as on the stash.

## The last long names

`stores.cpp` printed "Wirt the Peg-legged boy" and "Farnham the Drunk" as their dialog headings - the only two
vendors that still show a heading, and so the only long names left on screen after v1.12.115 shortened the hover
names. Both now read just the name.

## A trap worth recording

`inv.cpp` is **CRLF** and `stores.cpp` is **LF**. The first patch at inv.cpp matched nothing and reported
"0 of 2" rather than failing - a silent no-op. The repo is legitimately mixed, so a byte count comes before the
edit, every time.

## Build

Debug, clean. 832/832. `ui\panel_bg_inventory.png` packed at 224220 bytes.

## Not verified

Not seen on screen. Worth checking that the painted frame lands exactly on the grid, and that the canvas's own
faint cell lines inside the opening do not fight the 1px rules the code draws over them.

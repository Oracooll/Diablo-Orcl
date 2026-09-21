---
version: v1.12.120
date: 2026-09-21
area: UI / Griswold's shop
tests: 832/832
---

# Griswold's grid tabs get the forge with a frame in it

## The ask

> i want you to apply this canvas - "001.Griswold Canvas 340x720 Grid Tabs Only.png"
> Can you guess where?

## Where, and why the guess is checkable

His **grid tabs**, and not his Salvage page. The filename says so, but the art says so independently: measured on
it, the ornate band runs **x 26..313, y 159..628**, wrapping the 10x16 grid's 280x448 at (30,170) with the same
3px-and-a-black-pixel relationship the other framed canvases have. It is a frame for a grid, and the Salvage page
has no grid.

So `ui\griswold_canvas_grid.png` is a fourth canvas, and the split inside his own shop is now:

| Screen | Canvas |
| --- | --- |
| His nine grid tabs | `griswold_canvas_grid.png` - the forge WITH the frame |
| His Salvage page | `griswold_canvas.png` - the forge without one |

`IsRedesignedShopScreen` already excludes SmithTransmute, so `redesigned` means "a grid tab of his" and needed no
change. The framed cut is used when present and the frameless one is the fallback, so a build without the new
file draws exactly what it drew before.

The code bezel is skipped where the painted frame is up, as on the other three canvases.

## A conflict found, and NOT silently fixed

His six service buttons sit at **y 128..161** (y=128, 34 tall). This canvas's frame begins at **y 159**. They
overlap by **three pixels**.

Left alone deliberately and put to the user, because the obvious fix is not the one that worked last time. The
inventory's tabs are procedurally drawn plates, so cropping four pixels off them cost nothing; these buttons are
the user's painted 34x34 `002.Button.png`, and cropping would cut through its bottom border. Moving the row up
departs from the 128 his own guide canvas specified. Redrawing the canvas's frame is a third answer that needs no
code at all - the frame's position is read from nothing.

That is the user's call, not a judgement to make on their art.

## Build

Debug, clean. 832/832. `ui\griswold_canvas_grid.png` packed at 363703 bytes.

## Not verified

Not seen on screen. The three-pixel overlap above is the known defect; the rest to check is that the painted
frame lands exactly on the grid.

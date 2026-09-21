---
version: v1.12.119
date: 2026-09-21
area: UI / inventory
tests: 832/832
---

# The tab row stops short of the frame, and the frame is not tinted

## The asks

> you need to crop the bottom 4px of the tabs row in the inv screen, because they are overlapping with my inv
> grid frame.
> Also - the tint layer we have going on on every canvas - in the inv screen cut out 288x217px area out of it
> because it is tinting my inv grid frame and i dont want it tinted.

## Both numbers were confirmed against the art

**The overlap is exactly four pixels.** `TabRowY` is 388 and a tab is 28 tall, so the row reached y 415; the
canvas's frame begins at y 412. 412..415 is four rows, which is what the user counted.

**The frame is exactly 288 x 217.** Measured on the canvas: the ornate band runs x 26..313 and y 412..628. The
user's figure, arrived at from the art rather than copied, which is worth having done - it is how a rect given
without a position gets one.

## The crop comes off the HEIGHT, not the position

`TabRowY = GridOrigin.y - TabSize.height - TabRowGridGap` - the row's seat is derived from the grid's own top.
Lifting the row would have re-opened the same gap above it, so `GetTabRect` returns `TabSize.height -
TabBottomCrop` instead and the row stays where it sits.

The row's own invariant still holds, and it has a `static_assert` for it: an unselected tab is 388 + 24 = 412,
and the active tab grows three pixels upward with its bottom pinned, 385 + 27 = 412. All ten stay on one line.

**The HIT area shrank with the drawing**, because `GetTabRect` is both. A click four pixels below a tab no longer
selects it, which is better than a tab clickable on top of the frame - flagged to the user as the one place the
ask could have meant either.

## The dim cannot be undone, so the hole is never filled

`DrawSidePanelDim` is one half-transparent blend over the canvas's opening, and a blend cannot be reversed once
applied. So `keepClear` is not a second pass that lifts the tint - it splits the dim into the four bands AROUND
the hole, each clamped to the opening, any that comes out empty drawing nothing.

Optional and defaulted to `nullptr`, so the eight other windows that wear the dim are untouched.

## Build

Debug, clean. 832/832. `InvTest.EveryTabPositionOpensAStoragePage` clicks tab centres, which moved up two pixels
and are still well inside.

## Not verified

Not seen on screen. Two to check: that the tabs now sit cleanly above the frame with no gap opened instead, and
that the untinted rect matches the frame exactly - a band of bright stone around a gold frame would be as wrong
as the tint was.

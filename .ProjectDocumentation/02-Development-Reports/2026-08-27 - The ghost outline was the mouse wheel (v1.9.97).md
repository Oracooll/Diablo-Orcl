# The ghost outline was the mouse wheel (v1.9.97)

**Date:** 2026-08-27
**Version:** 1.9.97
**Trigger:** user report - "the ghost double outline of mobs. it still appears once in a while,
something must be causing it, but it triggers after a few minutes of gameplay for unknown reason."

## What was actually happening

Monster outlines are drawn twice on purpose. The first pass runs inside the tile sweep, where the
outline can be painted over by a wall drawn later; the second pass is a deferred queue
(`HiddenMonsterOutlineQueue`), drained once the whole scene is composited so the outline lands on
top of whatever occluded the body. Redrawing pixels that are already visible is a harmless no-op, so
every outline is queued unconditionally.

The drain lived in `DrawView`, **after `DrawGame` returned**. And the last thing `DrawGame` does is:

```cpp
if (zoomFactor > 1.0f) {
    ZoomScale(fullOut.subregionY(0, gnViewportHeight), zoomFactor);
}
```

So at any zoom above 1.0x the first-pass outline was scaled up with the rest of the scene, while the
second pass then painted the same sprite again at its **pre-zoom position and pre-zoom size**: a
smaller outline sitting inside and offset from the real one. A ghost double outline.

## Why it "triggered after a few minutes for unknown reason"

Because of the mouse wheel. A plain wheel notch is one 0.1x dungeon-zoom step
(`AdjustDungeonZoom(1)`, `Source/diablo.cpp`), with no modifier required - Ctrl is the automap, Alt
the mini-map, Shift the belt. One accidental scroll during play takes the view from 1.0x to 1.1x and
leaves it there. A 10% zoom change is easy not to notice; a doubled outline on every monster is not.
That is the whole "unknown reason": the bug was never intermittent, it was latched, and the latch
was a wheel notch.

## The fix

The drain moved from `DrawView` into `DrawGame`, immediately after `DrawTileContent` and **before**
`ZoomScale`. It keeps the property the second pass exists for - it still runs after the entire scene
is composited, so it still lands on top of walls - while going through the same scaling as
everything else it is drawn over.

Correcting the position alone, the way `qol/itemlabels.cpp:152` corrects its own deferred queue with
`position *= *sgOptions.Oracool.dungeonZoomLevel`, would not have worked here. A label is text drawn
at a point; this is a **sprite**, and `ClxDrawOutlineSkipColorZero` has no scale parameter. Drawing
it inside the scaled region is the only way it comes out the right size.

Two things improve as a side effect: the outlines are now clipped to the render region exactly as
the monster bodies are, and they can no longer paint over the mini-map or the full automap, both of
which `DrawView` draws after `DrawGame` and which the old drain sat on top of.

## Verification

Build clean, suite **570/572** - the two standing baseline failures
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`) and nothing else.

No unit test: the queue is file-static in `scrollrt.cpp` and the drain needs a live render surface,
a populated `dMonster` and a `MyPlayer`. This one is verified on screen, not in ctest.

**To confirm in game:** scroll the wheel up a few notches to zoom the dungeon view in, then hover a
monster or let one come inside the Range Highlight radius. Before this change that is precisely when
the second outline appeared; it should now be a single outline at any zoom. Middle-click toggles
between 1.0x and 2.0x if you want the extreme quickly.

Not pushed: GitHub Actions minutes are exhausted until roughly 2026-09-01.

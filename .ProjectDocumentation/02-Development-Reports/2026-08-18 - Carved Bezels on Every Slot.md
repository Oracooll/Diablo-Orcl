# Carved Bezels on Every Slot

**Version:** 1.7.75
**Date:** 2026-08-18
**Request:** "we now do C and D, then E and then B" — unit C of the 2026-08-18 MPQ drop-zone sweep.

Every item slot and both item grids now wear the delivered ashen-limestone bezel instead of the
procedural 3px bevel this fork drew for them.

## Six configurations, six exact homes

The thing worth recording about this pack is that **nothing had to be adapted**. All six delivered
sizes land on shapes this fork already had, to the pixel:

| Bezel | Outer | Frames |
|---|---|---|
| 1×1 | 40×40 | the amulet and the two rings |
| 2×1 | 68×40 | the equipment belt slot |
| 2×2 | 68×68 | helm, shoulders, gloves, bracers, legs, boots |
| 2×3 | 68×96 | chest, weapon, shield |
| 10×7 | 292×208 | the backpack grid (`GridSizeInCells`) |
| 10×16 | 292×460 | the stash grid (`StashGridColumns` × `StashGridRows`) |

The rule underneath is `outer = cells × 28 + 12` — a six-pixel frame on every side of a grid whose
pitch is `CellPx`. A bezel is therefore blitted six pixels up and left of the rect it frames, and
its (6,6) pixel lands on that rect's top-left corner. **Cell geometry, hit testing and item
placement are untouched.** That a pack authored elsewhere arrived carrying this fork's own 10×7 and
10×16 is the strongest evidence yet that it was measured against the real build rather than guessed.

## Frame only — and why that was the whole decision

The delivered PNGs are opaque all the way through, interior included: they are complete recess
plates, not outlines. One whole-rect blit would have been the faithful reading of the art, and it
would have quietly deleted two things this panel already does.

- The **class silhouette** is drawn behind the paperdoll (`inv.cpp` draws it *before* the slots) and
  is visible only because the slots' fill is half-transparent. Opaque slots would have blanked out
  most of the figure.
- The **item-quality backings** behind occupied cells would have gone the same way.

So `DrawGridBezel` blits **four bands** — top and bottom at full width, the sides filling in between
them so corners belong to the horizontal bands and no pixel is drawn twice — and never touches the
interior. The existing `DrawThemedFill` recess stays exactly as it was. This is also what the pack's
own integration guide actually asks for: *"replace only the outside group frame/background
treatment."* The interior colour is still in the asset, one blit away, if a solid stone recess is
ever wanted.

## The three pixels that had to be found

The bezel is 6 where the bevel was 3, and two grids in this fork sit against a hard limit: their
frames must clear the orbs that overlap the panels.

The clearance is reserved in the **layout** while the choice of frame is made at **draw time**, so
the two must not be allowed to disagree — reserve 3, draw 6, and the inventory grid's bottom edge
goes under the mana orb with nothing reporting it. Hence one name, `oracool::GridFrameWidth`, which
`GridOrigin`, `TabRowGridGap` and both clearance asserts now measure against, with a static_assert
that the fallback bevel can never grow thicker than the room reserved for it.

- **Inventory:** the grid rose 3px (`GridOrigin.y` 425 → 422) and the tab row rose with it, since
  the gap that reveals the grid's top border is defined as the frame's own width.
- **Stash:** absorbed the extra three pixels **without losing a row**. The pair of asserts around
  `StashGridBottom` prove it both ways — the frame still clears the health orb, and a seventeenth
  row still would not fit. Stash capacity is save-visible, so this was the one place a few pixels
  could have turned an art swap into a data change.

## Where it lives

`oracool/grid_bezel.h` declares the inset and the two entry points; the implementation is in
`hud_art.cpp`, where the asset loading, palette quantisation and blitting machinery already lives
and is file-local. The alternative — declaring it in `hud_art.h` — would have meant
`inventory_layout.h` including it for one constant, dragging `player.h` into a header that is
otherwise pure geometry.

No tint on quantisation: the bezels arrived already quantised against `town.pal` (their stone reads
as exact palette entries — 30,30,30 and 61,61,61 off the grey ramp), so tinting would move art that
is already sitting on the colours it was authored for.

Both call sites keep the old bevel as a fallback behind `HasGridBezel`, so a missing or unreadable
PNG costs a slot its stone rather than its outline.

## Verification

Debug build clean at 1.7.75; suite **445 of 447**, the two failures being the standing baseline
pair. `oracool.mpq` carries all six `grid_bezel_*.png`.

**To see it in game:** open the inventory — every equipment slot and the backpack grid should be
framed in carved limestone rather than the old gold bevel, and each slot should now be *slightly
roomier*, since the bezel sits outside the cells where the old bevel ate three pixels from every
inside edge. Then open the stash for the 10×16. Two things worth checking deliberately: that the
class silhouette still reads behind the paperdoll (the reason the interior is not blitted), and that
the tab row still sits cleanly on the grid's top edge after both moved up three pixels.

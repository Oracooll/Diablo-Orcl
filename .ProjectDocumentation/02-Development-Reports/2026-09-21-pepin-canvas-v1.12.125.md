# Pepin gets his doorway — v1.12.125

**Date:** 2026-09-21
**Version:** v1.12.125
**Branch:** renderer-32bit

## The request

> take Pepin's new canvas and apply it - `Resources\Pepin Shop UI\Pepin Canvas 340x720.png`
> Apply same redactions to canvas as Stash Canvas if relevant.

## Measured first: it is a painting swap, not a geometry change

Before writing a line, the canvas was scanned for its ornate band the way every canvas in this
family has been — a longest-contiguous-bright-run scan across rows inside the grid opening and down
columns through it:

```
col x=60 : bright at y 159..168 and y 619..628
row y=450: bright at x 11..28 and x 311..331
```

Which puts the band at **x 26..313, y 159..628** — the same 288×470 the stash canvas, the shared
grid canvas and Griswold's framed forge all carry, around the same 280×448 grid at (30,170).

So nothing moves for this canvas. That is worth stating rather than assuming: the whole reason a new
painting can be dropped in as one line of code is that the art keeps coming back with its frame in
the same place, and the one time it does not, this measurement is what will say so.

## The redactions, and which were "relevant"

The stash's canvas took six changes over v1.12.123–124. Four apply here:

| Stash change | Pepin |
|---|---|
| Remove the title | **Applied** — `PEPIN` no longer drawn over the portrait |
| Remove the tint | **Applied** — drawn with a bare `DrawLoosePng`, which never dims |
| Grid a bit transparent | **Applied** — one pass of fill instead of two |
| Remove the cast shadow | n/a — the shop never drew one |
| Remove the nav arrows' shadow | **already true** — the shop's `<` `>` carry no `Shadowed` flag |
| Move SORT under the gold | n/a — that is the stash's own button |

Plus the one the stash got when its canvas arrived: **no second bezel**. The frame is painted into
the art, so `DrawGridBezel` is skipped, exactly as it is for Griswold's framed tabs.

### The tint is removed by which function draws it

`DrawSidePanelGridArt` lays the shared canvas dim over whatever it draws; `DrawLoosePng` does not.
Pepin's branch calls the second, so the dim never happens — the same mechanism as Griswold's forge,
rather than a flag switched off somewhere. Worth knowing if a future canvas is routed through the
shared helper by accident: it will arrive tinted, and nothing will look broken enough to notice.

### The grid fill went to one pass for EVERY painted shop page

```cpp
const bool paintedFrame = griswoldFramed || pepinFramed || (!redesigned && HasSidePanelGridArt());
DrawThemedFill(out, grid, paintedFrame ? 1 : 2);
```

Not only Pepin's. These pages are tabs of one shop in the user's own words (2026-09-21: *"all tabs
for all vendors are to be considered Stores"*), and a grid that changes density as you move between
vendors reads as a bug rather than as a difference. Griswold, Adria and Wirt therefore lighten too —
a change beyond the literal request, flagged here and in the closing message.

The procedural path keeps its two passes: with no painting, that fill is not covering art, it *is*
the grid's face, and one pass there gives a paler grid rather than a transparent one.

## One asset, not two

Griswold needs two cuts — the framed forge for his grid tabs and a frameless one for Salvage, which
has no grid. Pepin has exactly one tab (`TalkID::HealerBuy`), so there is no frameless page and one
file does it.

## Gated on the art

`IsPepinPaintedScreen` asks `HasShopArt(PepinCanvasAsset)`, matching `IsRedesignedShopScreen`. A
build short of the asset keeps the limestone layout — otherwise the page would lose its title and
its bezel and get nothing back, which is a worse window than the one it replaced.

## Files

- `Packaging/resources/oracool_assets/ui/pepin_canvas.png` — new, 373385 bytes, 340×720.
- `Source/oracool/shop_grid.cpp` — `PepinCanvasAsset`, `IsPepinPaintedScreen`, the draw branch, the
  title suppression, the bezel skip and the fill-pass rule.

**Asset added, so `tools\build_oracool_mpq.cmd` must run** — a normal build does not repack
oracool.mpq.

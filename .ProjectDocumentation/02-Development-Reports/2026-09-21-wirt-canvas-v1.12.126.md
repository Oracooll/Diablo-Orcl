# Wirt gets his alley, and the portraits become a table — v1.12.126

**Date:** 2026-09-21
**Version:** v1.12.126
**Branch:** renderer-32bit

## The request

> the the same now with Wirt - `Resources\Wirt The Peg-Legged Boy Shop UI\Wirt 340x720.png`

"The same" being [[2026-09-21-pepin-canvas-v1.12.125]]: apply the canvas, with the stash's
redactions.

## Measured first, again

```
col x=60 : bright at y 159..168 and 619..628
row y=450: bright at x 11..28 and 311..331
```

Band at **x 26..313, y 159..628** — the third canvas in a row to land exactly on it, after Pepin's
and the stash's. Another painting swap; nothing moves.

Three files sat in the folder (`Wirt 340x720`, `Wirt shop background`, `Wirt seated shop
background`). Only the one the user named was measured or installed.

## Both tabs

Wirt has two — `TalkID::BoyBuy` and `TalkID::BoyGamble`, his Shop and Gamble pages — and both show a
shelf, so both take the canvas. Still one file: unlike Griswold, who needs a frameless cut for
Salvage, Wirt has no page without a grid.

## The third one earned a table

Pepin went in as a named bool (`IsPepinPaintedScreen`) and a branch. Doing that again for Wirt would
have been two bools, two branches and two more places for the next vendor to be forgotten, so the
pattern was lifted into a lookup that answers with the painting itself:

```cpp
const char *PortraitCanvasFor(TalkID id)
{
	switch (id) {
	case TalkID::HealerBuy:   return PepinCanvasAsset;
	case TalkID::BoyBuy:
	case TalkID::BoyGamble:   return WirtCanvasAsset;
	default:                  return nullptr;
	}
}

const char *PaintedPageCanvas(TalkID id)   // nullptr unless the file is actually installed
```

`DrawShopGrid` asks once:

```cpp
const char *const portrait = PaintedPageCanvas(stextflag);
const bool portraitFramed = portrait != nullptr;
```

The pointer says *which painting to blit*; the bool drives all four redactions (no title, no dim, no
second bezel, one pass of fill). **Adria is now one line in that switch** when her canvas arrives.

This is a refactor done at the third instance, not the second — the second is where a pattern is
guessed at, the third is where it is known.

## The redactions

Unchanged from Pepin's, and they come free with `portraitFramed`: no title over the portrait, no dim
(the branch calls `DrawLoosePng`, which never dims, rather than `DrawSidePanelGridArt`, which
always does), no second bezel inside the painted one, one pass of grid fill instead of two.

## A near-miss worth recording

Renaming `pepinFramed` with a perl substitution produced `!portraitCanvas != nullptr` at two sites —
which parses as `(!portraitCanvas) != nullptr` and is not what it looks like. Caught by reading the
grep output rather than by the compiler, which would have taken it at both sites without complaint
under some conversions. A bare-name substitution is unsafe wherever the name sits under a `!`.

## Files

- `Packaging/resources/oracool_assets/ui/wirt_canvas.png` — new, 361060 bytes, 340×720.
- `Source/oracool/shop_grid.cpp` — `WirtCanvasAsset`, `PortraitCanvasFor`, `PaintedPageCanvas`,
  `IsPepinPaintedScreen` retired into them, and the draw switched to the pointer.

**Asset added, so `tools\build_oracool_mpq.cmd` must run.**

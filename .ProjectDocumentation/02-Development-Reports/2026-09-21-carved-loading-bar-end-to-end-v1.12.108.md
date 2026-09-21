---
version: v1.12.108
date: 2026-09-21
area: UI / loading screen
tests: 832/832
---

# The loading bar is the carved bar, laid end to end

## The ask

> can you assemble end-to-end loading bar during loading times using these files, replacing the current
> gradient loading bar: `Resources\00-original-game-art\Loading Bar Assets\`

and, on looking at the art:

> i think the green needs to be cut-out.

## The art

Two files, `prog_bg.png` (the empty carved track) and `prog_fil.png` (the gold fill), 232 x 38 each. Neither is
232 x 38 of picture:

* **Row 0 is fully transparent** - one blank line of export padding.
* **Columns 228-231 are pure `#00FF00`** in every row. Green is this project's marker colour precisely because
  the game's palette holds none of it, so it can never be mistaken for art.

Cut away, the real bar is **228 x 37**, and the two trimmed files ship as `ui\loadbar_track.png` and
`ui\loadbar_fill.png`. Zero green pixels survive the trim, which the trimming pass counted and reported.

Reading across the art, each end carries a six-column cap: three columns of gold bevel (`#5B5134`, `#39311D`,
`#0F0500`) and three of inner shadow. Between them is 216 columns of texture - dark and noisy on the track, gold
and noisy on the fill.

## Why the art is loaded as a true-colour surface

This is the part that decides where the code lives. **The loading screen runs on the CUTSCENE's palette, not the
game's.** That is what `BarColor[3] = { 138, 43, 254 }` in this same file has always been for: one palette index
per cutscene, because index 138 is a different colour depending on which painting is up.

So the bar could not go through `DrawLoosePng`, the `ui\` PNG cache every window in the mod uses, because that
path quantises against the active palette - the bar would come out in whatever colours the current cutscene
happens to carry, and would change from one load screen to the next. Instead the two files load as plain
`SDL_Surface`s through `LoadPNG` and blit straight to the back buffer, bypassing the palette entirely, exactly
as the cutscene painting itself already does two functions above.

## Laying 228 pixels across 1280

`SliceButtonAxis` (oracool/shop_grid.h) already answers this question for every sliced control in the mod, so
the bar asks it too: **both carved ends whole, the middle repeated between them, at 1:1 and never stretched.**
A stretched bevel stops reading as carved, which is the one thing the art is for.

`BlitBarRun(out, art, limit)` walks those spans and draws only the first `limit` pixels. One function serves
both sprites:

| Sprite | `limit` | Result |
| --- | --- | --- |
| track | the full width | the empty bar, end to end, both caps drawn |
| fill | the progress | the gold revealed over it, cut hard mid-repeat at the leading edge |

Because the spans ascend by destination, the loop can stop at the first span past the limit, and the fill never
draws the right-hand cap until the bar actually reaches it.

## What changed on screen

* The bar is now the art's own **37 pixels tall**, not 15. The gradient's 15 was a number; this is a carving
  with three rows of bevel at each edge, and squeezing it to 15 would throw away the reason for using it.
* The **empty track is now visible** for the whole load. The gradient drew nothing where it had not reached.
* Still full screen width, still flush to the floor.

## What was kept

The gradient is the fallback, not deleted. It draws when either file is missing or the screen is the legacy
8-bit path, which cannot take a true-colour blit at all. `BarGradientColorAt` and the item-quality ladder behind
it are untouched.

## Build

Debug, clean. 832/832. `oracool.mpq` repacked by the normal build - the asset glob is `CONFIGURE_DEPENDS` and
the pack step depends on the asset files, so both PNGs went in without a manual repack:

```
[53/56] Packing oracool.mpq
  ui\loadbar_fill.png    3523 bytes
  ui\loadbar_track.png   1998 bytes
```

## Not verified

Nothing here has been seen on screen. The bar draws over the cutscene painting, and for anything drawn over a
picture a screenshot is the only verification - constants and tests pass while it is visibly wrong. Two things
to look at on the next load screen:

1. **Seams.** The middle repeats every 216 pixels. On the dark track that should be invisible; on the gold fill
   it may not be.
2. **Height.** 37 pixels is over twice the gradient's 15.

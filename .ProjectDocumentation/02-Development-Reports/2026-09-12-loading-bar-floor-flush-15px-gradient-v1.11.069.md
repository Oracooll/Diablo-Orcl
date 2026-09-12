# The loading bar: on the floor, 15px, red to green

**Version:** 1.11.069
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "i want to change the loading bar during loading screens. move it flush to the floor. make it 15px
> tall. make its color gradient, starting from dark red and finishing at bright gree, transitioning
> through whatever colors you decide."

## Flush to the floor

The bar was placed by `BarPos`, a per-screen table authored at 640x480 - y 37 on most screens, 421 on
one - and scaled with the painting. It is now measured from the **surface's** bottom:

```cpp
const int barTop = out.h() - ProgressHeight;
```

The cutscene is fit to height, so the painting's floor and the screen's floor normally coincide;
measuring from the surface keeps the bar on the floor anyway when the painting is cropped or
letterboxed. `BarPos`'s **y is no longer read at all** - only its x, which still positions the bar
horizontally under the painting's 4:3 core.

## 15 real pixels

`ProgressHeight` 22 -> 15, and it is no longer scaled from 480. The old height was multiplied by
`scaledHeight / 480`, so at 720p the 22 became 33; "15px" means fifteen on the screen the player is
looking at, so the scaling is gone from the height. The bar's **length** still scales, because that
is what tracks the painting's width.

## The gradient

Six stops, permille along the track, linearly interpolated in integer maths:

| at | colour | |
|---|---|---|
| 0 | 139,0,0 | dark red |
| 250 | 198,48,16 | red, warming |
| 500 | 226,124,8 | orange |
| 700 | 232,204,24 | yellow |
| 860 | 150,206,40 | yellow-green |
| 1000 | 48,224,72 | bright green |

The ends are the user's; the four between were the choice left open. They are not evenly spread: the
yellow is held back to 700 and the yellow-green to 860, because an even spread spends most of the
ramp in orange and the bar then reads as stuck through the middle of a load.

**The gradient spans the whole track, not the drawn part.** It is revealed as the bar grows, so the
colour at the leading edge is how far along the load is - a quarter done ends in red, half in orange,
three quarters in yellow. Normalising the ramp to the filled width instead would end every partial
bar on bright green, and the colour would say nothing.

Interpolation is at 1/256 rather than percent so adjacent columns do not step visibly on a wide
screen, and the fill is one 1px column at a time - 500-800 columns per frame during a load, which is
nothing next to the painting behind it.

## The 8-bit path keeps one colour

`FillRectRgb`'s indexed fallback takes a single palette index, and a per-column gradient there would
need a nearest-palette match per column. That path is the legacy 8-bit screen; the 32-bit one is what
ships, so it keeps the gold index it had.

## Verification

Debug and Release build clean; **718/718** tests pass. RTM updated.

The ramp was rendered outside the game from the same integer arithmetic the C++ uses, at four fill
levels, to check the colours and confirm partial bars end mid-ramp. Where it sits against the real
paintings is the user's to judge - it is flush to the surface's bottom edge by construction.

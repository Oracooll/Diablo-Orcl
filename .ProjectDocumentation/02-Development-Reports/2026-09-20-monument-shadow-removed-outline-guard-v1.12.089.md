# The Rift Monument's shadow removed; the hover outline no longer asserts on a wide sprite (v1.12.089)

**Date:** 2026-09-20 · **Version:** v1.12.089 · **Tests:** 831/831

## The error

Hovering the Rift Monument in town raised `assertion failed (clx_render.cpp:296) width < MaxOutlineSpriteWidth`.
The selectable-object hover outline (`GetOutline` in `Source/engine/render/clx_render.cpp`) keeps three row buffers
of `MaxOutlineSpriteWidth = 253` booleans and stores every x as a `uint8_t`; v1.12.088 widened the monument's one
frame to 372 pixels to hold its east-north-east cast shadow (`OracoolStonegateAnimWidth = 372`), and the first hover
walked past the buffers. In a Debug build that is the assertion box; in Release it would have been a stack overrun.

## The fix (the user's call: "remove the shadows of the rift monument asset. it will fix the problem.")

- `tools/ScalePainting.ps1` re-run WITHOUT `-ShadowLength/-ShadowRise/-ShadowBaseline`: the repainted blue-slate
  master at 128 wide with the rocks' slate cast (`-Brightness 0.7 -TintRgb "57,65,95" -TintStrength 1.0`) - a
  128x147 frame, 8936 opaque pixels, 244 colours; `tools/build_stonegate_cel.cmd` packed `objects/orclgate.cel`
  (9546 bytes, one frame). The recipe header records the shadow's removal and why.
- `Source/objdat.h`: `OracoolStonegateAnimWidth = 128`.
- `Source/engine/render/clx_render.cpp`: the assertion is now a guard - a sprite whose width or height reaches
  `MaxOutlineSpriteWidth` gets no outline and the game goes on. The next wide town object will hover unoutlined
  instead of crashing.

The inactive arch over the town portal shares the frame, so it lost the shadow too.

## Also this version: the Levski's Cube page renamed

The plan artifact is now **Levski's Cube and Artisans** (https://claude.ai/artifact/LSxpjNZdSu3skd6nJ5HmAM, version
3): a new section IX gives each artisan's book as built at v1.12.056 (Griswold's Forge 5, 6, 10-16 and the salvage
plates; Ogden's Table 0, 1, 3, 4, 8, 17; Gillian's Hearth 2, 7, 9, 18; the Cube 19-25) and the Diablo III abilities
the Roadmap's three artisan cards wanted of them - Griswold's material economy and Plans, Ogden's rings and amulets
from Designs, Gillian's Enchant, Transmogrify and dyes - tracked as phases A1-A6 beside the Cube's C1-C9 (the page's
`phases` collection takes `a1`..`a6`). Section I's D8 row now states the split that was chosen.

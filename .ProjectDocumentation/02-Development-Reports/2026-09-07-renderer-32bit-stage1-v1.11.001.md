# The 32-bit compositing renderer, stage 1: a 32-bit back buffer, the picture unchanged (v1.11.001)

**Date:** 2026-09-07. **Branch:** renderer-32bit. **Plan:** 07-Backlog/2026-09-07-Renderer-32bit-Plan.md.

## What changed

The screen surface is XRGB8888. Every drawing kernel is templated on the destination pixel type and resolves a palette index through `PaletteRGB[256]` as it writes; offscreen `OwnedSurface`s stay 8-bit and keep writing indices, so sprite work, the half-size item icons, the hardware cursor and the fourteen pixel golden tests are untouched and unchanged.

- `engine/surface.hpp/.cpp`: `bytesPerPixel`, `isIndexed`, typed `at<Pixel>`, `pixelPitch`, format-aware `SetPixel`, `BlitFrom` resolving 8-bit sources into a 32-bit destination; `OwnedSurface::Rgb` for tests.
- `engine/palette.h/.cpp`: `PaletteRGB`, rebuilt by `palette_update`; the SDL palette object is still kept current for the hardware cursor and the 8-bit surfaces that bind it.
- `engine/render/blit_impl.hpp`: the six blitters in two bodies each; a 32-bit blend is the exact per-channel average (`AverageRgb`) where the 8-bit one is the nearest-index lookup. The one visible difference, and an improvement.
- `engine/render/clx_render.cpp`: the three RLE walkers typed; one dispatch in the four draw entry points.
- `engine/render/dun_render.cpp`: all fifty-odd tile kernels typed by a script; the two line kernels' explicit specialisations became `if constexpr`; the black tile fills through the palette; RenderTile and the black tile dispatch once.
- `engine/render/primitive_render.cpp`: format-aware fills, lines, half-transparent rects and pixels; the aligned-32 black-blend trick kept for 8-bit only; a 32-bit black blend halves the pixel.
- `engine/dx.cpp`: the 32-bit buffer; `Blit` applies the present-time transforms.
- Fades and the screenshot flash are present-time transforms now (`FadeLevel`, `PresentRedFlash`): vanilla faded by repainting the palette under a frame that was not redrawn, which colours in a buffer cannot follow. `PaletteRGB` is built from the unfaded palette while a fade runs and `Blit` scales the frame on its way out. Colour cycling (water, lava) needs nothing: the world is redrawn every frame and the table follows the cycle.
- `capture.cpp` writes an RGB screenshot; the cursor's save buffer is four bytes a pixel; the zoom scaler is generic over the pixel size; the pause tint, the shop's row copy and the three read-modify-write blend sites (hud_art, aura_ground, levski_roar) are format-aware.

## Tests

The suite as it stood, unchanged, plus: a sprite, a TRN, a blend, the primitives, an 8-to-32 blit and the average itself on a 32-bit surface; the fade level's range. 694/694, the shuffle lane included.

## Not yet verified in play

This is the first build a human has to look at. The Debug exe at C:\Diablo Orcl\x64-Debug is v1.11.001. Look at: town; a Cathedral floor arriving through a fade-in; a Caves floor with water; the inventory over the world; Levski's grid; an aura's ground ring; a screenshot (F12) and its red flash; a paused game. The RTM folder stays on 1.10.017 until asked.

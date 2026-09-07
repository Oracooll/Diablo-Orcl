# The 32-bit compositing renderer (v1.11): plan

**Date:** 2026-09-07. **Branch:** renderer-32bit, off oracool-v1-main at cd0521a. **Backup:** tag v1.10.017-pre-renderer on GitHub, the last 8-bit build. **Versions:** 1.11.001 upward.

## Why

Every pixel in the game is a byte indexing a 256-entry palette, and text, monsters, items and HUD art may only use the 128 entries that are the same in every level palette. That budget is spent: a new hue means evicting something painted with it (the green ramp took the orange minis). The upgrade moves the index-to-colour lookup from the end of the frame to the moment each pixel is drawn, so the screen holds colours, not indices. Then a sprite may bring its own 256 colours, and a text colour is a value.

## The three stages

**Stage 1 - a 32-bit back buffer, the picture unchanged.** The screen surface becomes XRGB8888. Every drawing kernel resolves an index through `PaletteRGB[256]` (rebuilt whenever the palette changes) as it writes. Offscreen `OwnedSurface`s stay 8-bit, so sprite work (import, scaling, the half-size item icons, the hardware cursor) and the fourteen pixel golden tests are untouched; every kernel is templated on the destination pixel type and dispatches on the surface's bytes per pixel. Blends become exact RGB averages instead of the nearest-index approximation - the one visible difference, and an improvement. Palette EFFECTS that relied on repainting the palette under a still frame (fades, the screenshot flash) become present-time transforms; colour cycling (water, lava) keeps working because the world is redrawn every frame and `PaletteRGB` follows the cycle.

**Stage 2 - per-asset palettes.** A sprite may carry its own 256-entry table; the kernels resolve through it instead of the level's. Lighting darkens colours arithmetically for such sprites. Original art keeps the level palette and is byte-identical. New PNG art is quantised to its own 256 colours at build time by the importer, with reserved entries (transparent, shadow) and family-shared palettes for sets that must match.

**Stage 3 - RGB text.** A text colour is an RGB value in the draw flags' colour field; the .trn machinery stays for the original fonts only. The menu/in-play twin colours become unnecessary.

## Stage 1: the change set

- `engine/surface.hpp/.cpp`: `bytesPerPixel()`, typed `at<Pixel>()`, `pixelPitch()`, format-aware `SetPixel`, `BlitFrom` resolving 8-bit sources into a 32-bit destination.
- `engine/palette.h/.cpp`: `PaletteRGB`, rebuilt in `palette_update` from the unfaded palette; `FadeLevel` and the red flash as present-time state.
- `engine/dx.cpp`: the back buffer in XRGB8888; `Blit` applies the fade/flash when active.
- `engine/render/blit_impl.hpp`: the six blitters templated on the pixel type; RGB averages for the 32-bit blends.
- `engine/render/clx_render.cpp`, `dun_render.cpp`, `primitive_render.cpp`, `text_render.cpp`: kernels templated, entry points dispatch on the format; the aligned-32 black-blend trick stays 8-bit only.
- `engine/render/scrollrt.cpp`: cursor save/restore and the zoom scaler in bytes-per-pixel terms.
- `capture.cpp`: RGB screenshots. `control.cpp` (the pause tint), `stores.cpp` (a row copy), `oracool/hud_art.cpp`, `aura_ground.cpp`, `levski_roar.cpp` (read-modify-write blends): format-aware.
- `engine/backbuffer_state.hpp`: the cursor's save buffer four times larger.

**Tests that must stay green:** the whole suite, the fourteen golden tests unchanged (8-bit offscreen), plus new ones: a 32-bit surface receives the palette's colour for an index through every kernel family; a blend is the exact average; the fade transform at 0 is black and at 256 is identity.

**Verification in play (the user):** a fresh screenshot of town, a Cathedral floor with a fade-in, a Caves floor with water, the inventory over the world, Levski's grid, a half-transparent aura ring, a shrine's red flash; each compared against the 1.10.017 build by eye.

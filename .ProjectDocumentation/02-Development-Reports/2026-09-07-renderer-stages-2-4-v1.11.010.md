# Renderer stages 2, 3 and 4 - v1.11.007 to v1.11.010

**Date:** 2026-09-07. **Branch:** renderer-32bit (local, not pushed). **Plan:** [[Plan - 32-bit Colour Renderer]].

## Stage 2 - the fork's own art in true colour (v1.11.007-008)

The plan said "per-asset palettes". The code said something simpler: none of the fork's PNG art was ever palette CLX. It was loaded as RGBA at runtime and squeezed into the level or menu palette on the way in (`hud_art.cpp` QuantizeAsset, `aura_ground.cpp`, `ui_backgrounds.cpp`). With a 32-bit screen the squeeze is skipped.

- `primitive_render`: `BlitArgb`, `BlitArgbScaled`, `CompositeArgbOver`, `PackArgb`. Source-over with straight alpha; an indexed target is refused so callers keep their palette path.
- `hud_art.cpp`: `ArtAsset::argb` built beside the 8-bit layers in QuantizeAsset (tint and luminance done in RGB, `TintedRgb`); a `Layer` enum and `BlitLayer` / `BlitLayerHalf` / `BlitLayerScaled` pick the path per draw. 24 call sites rerouted by regex; the orbs' frame and sphere layers derive from the stored circle.
- `aura_ground.cpp`: `AuraArt::rgb`, composited with real alpha at the 75% ceiling the two-blend path had.
- `ui_backgrounds.cpp`: the paintings resampled to ARGB and shown through a new `UiImageRgb` item (`ui_item.h`, `diabloui.cpp`).
- Test: `TrueColourArtCompositesWithItsOwnAlpha`.

Open: in-world CLX sprites under lighting keep the level palette. Only matters once the mod ships its own monster or item art.

## Stage 3 - text colours as values (v1.11.009)

- `blit_impl.hpp` `BlitWithRgbMap`; `clx_render` `ClxDrawRgbMap` / `RenderClxSpriteWithRgbMap`.
- `text_render.cpp`: one `RgbBake` table per colour, baked from the .trn through `logical_palette` and keyed on the new `PaletteRgbGeneration` (bumped in `RebuildPaletteRgb`), so a palette load or gamma change rebuilds it and a fade does not (fades are present-time). `DrawFont` uses the table on a 32-bit target, the .trn otherwise. `DefineTextColorRgb` / `ClearTextColorRgb` for a colour from one hex, shaded across the band the way the gold ramp shades.
- Exactness: with a file, the table is `PaletteRGB[trn[i]]` - the same pixels as before.
- The legend became a registry: `tools/BuildFontColourLegend.pl` rewritten; 23 colours in use with ID, hex, field index, callers (grepped from Source), use; the pool of ramps and passes dropped. Test: `ATextColourCanBeAValueWithNoFile`.

## Stage 4 - cleanup (v1.11.010)

- The 21 Orcl .trn files converted to `RgbDefinedColors` entries: sixteen band values plus out-of-band extras (`green1` remapped the yellow minis, `orange1` the green minis, `dialogyellow` one red), each baked from the file through the town palette (the menu palette for the two menu colours). `Packaging/resources/oracool_assets/fonts` is empty and gone from the MPQ. `tools/MakeYellowFontTrn.ps1` retired.
- The legend generator reads the definitions out of `text_render.cpp` (`// was fonts\...` comments) so the samples stay exact.
- NOT done, by evidence: the golden tests draw HUD art into indexed surfaces (`DrawTownPortalIcon`, `DrawBeltSlotPlate`), so the 8-bit fallback stays.

## Numbers

| | |
|---|---|
| Suite | 696/696 |
| Colours in use | 23 |
| .trn files shipped by the fork | 0 (was 21) |

## Verification still owed

The user has seen stage 1 in play. Stages 2-4 await his look; the RTM folder is at 1.11.010. Nothing should look different except softer alpha edges on HUD art and a calmer aura ring.

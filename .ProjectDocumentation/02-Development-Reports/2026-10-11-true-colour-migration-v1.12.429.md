# v1.12.429 - the last 256-colour paths: root cause of the vanishing cursor, and Orcl art migrated to true colour (2026-10-11)

User: "Check what other game assets still use the old 256 color palettes and migrate them to the new true-colour table." A read-only survey listed every path still going through a 256-colour palette; this build fixes the stale-palette class and migrates the quantised Orcl art.

## Root cause of the vanishing cursor (and a screenshot hazard)
`LoadPalette` rebuilt PaletteRGB but never system_palette, so a palette loaded with no fade after it left the display palette on the PREVIOUS colours. The Poisoned Water Supply's purification does exactly that (StartPWaterPurify loads l3pwater.pal mid-level), as can a front-end UiLoadDefaultPalette. The hardware cursor was drawn through that display palette: an invisible black hand. Fixed three ways:
- `LoadPalette` now re-derives system_palette from the new colours at the current fade and pushes it (palette_update).
- The hardware cursor draws from PaletteRGB (v1.12.428) and rebuilds when PaletteRgbGeneration moves (`RefreshHardwareCursorColours`, each DrawCursor).
- The screenshot flash is a present-time transform only: RedPalette no longer tints system_palette, and the restore that could write a stale palette back into PaletteRGB is gone.

## Orcl art migrated to the true-colour path (32-bit screen; 8-bit paths kept for the golden tests)
| Asset | Before | Now |
| --- | --- | --- |
| Belt glyphs (portal, burger, run/walk), inventory tab chest | quantised index, only the white swapped | the art's own ARGB; white scaled to the tint, the tab's gold/grey as PaletteRGB values |
| Every HUD/panel PNG | requantised on every front-end <-> level switch | ARGB built once (tinted ones on a 128-255 palette change only); 8-bit layers lazy, built only for an indexed target |
| Aura rings | index map rebuilt, rgb dropped on palette moves | index map lazy; rgb kept |
| Front-end hero preview (incl. the dyes) | nearest UI-palette index | town.pal RGB + the sheet's own colours, ClxDrawRgbMap, scaled as uint32 |
| Item drop/flip PNG tumbles (gems, runes, charms...) | quantised to town.pal 128-255 | coloured rows + SpriteColours, drawn through DrawSpriteWithColours at the tile's light; thrown item sprites too |
| Esc-menu logo | nearest orig_palette TRN | its own palette as a uint32 table |
| Divine shield shine | brightened palette quantised to a TRN | Tint::Divine (the same brighten, as colour values) |
| Half-size shop item icons | 8-bit downscale through the blend table (level-specific indices) | ARGB downscale from PaletteRGB-resolved pixels |
| Stat-field fallback frame | decoded once through logical_palette | decoded through PaletteRGB, re-decoded on a generation change |
| Front-end paintings | true-colour cache keyed on the palette | keyed on size only |

Also: colour caches built from LightTables (LitPaletteTable, FrozenRgbTable, ConvertedRgbTable, SpriteColours tables) are keyed on a new `LightTableVersion` (bumped by MakeLightTable) as well as PaletteRgbGeneration - they no longer keep the previous level's light tables. Dead code removed: GetGoldTrn, BlitStripCellScaled. The hero portraits (ui_art\hero*.png) turned out to have no draw site (retired for the animated preview).

Already intended and unchanged: every original 8-bit Blizzard CEL/CL2 is resolved per draw through PaletteRGB / LightTables / TRNs.

Left: touch-screen potions (not a Windows feature), Smacker movies (own palette, not stale), the 1-31 colour-cycling freeze in generation-keyed tables (low risk).

Tests: test/oracool_true_colour_migration_test.cpp (light-table version invalidates the lit tables; Tint::Divine values). Build Debug, ctest 993/993.

To check in play: the Poisoned Water trip (the cursor), belt glyph and tab chest colours, item drops with PNG tumbles (colour, light, hover outline, label), the Esc-menu logo, the shop's half-size icons, the front-end hero preview dyes, a screenshot (no colour change after the flash).

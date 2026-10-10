# v1.12.419 - coloured and smooth lighting, and the stutter fixes (2026-10-10)

Two ideas taken from DamnationX (a DevilutionX build with HD art, WebGPU dynamic lighting and off-main-thread loading), the only two the user wanted: coloured dynamic lighting, and "prepare heavy art off the main thread, keep the cheap art visible until it's ready, never stall a frame". The user's answers: colour **and** smooth falloff, the hero's own light neutral, all four stutter fixes. Grid ray-traced shadows were discussed and deferred until these two are audited and played.

Built Debug, ctest 971/971 (14 new tests).

## Lighting - oracool/colour_light.cpp

The engine's light is untouched: dLight (one level per tile, gameplay and visibility) is worked out exactly as before. Two passes are added over the world only, inside DrawGame, before the zoom and the HUD:

- **Smooth floors** (option Smooth Lighting, on): floor tiles below level 15 are drawn at full brightness (DrawFloor uses LightTables[0]), then `ApplySmoothFloorLight` darkens each pixel by the brightness interpolated bilinearly between tile centres. The brightness of a level is measured from the level's own palette and light tables, so a tile centre comes out as the per-tile draw made it. Floor-pass objects are drawn after the pass (`DrawFloorObjects`) so they keep their own light. Off in the Nest and the Crypt, whose lava is full bright at every level.
- **Colour** (option Coloured Lighting, on): `RebuildLightColourField` runs at the end of every full ProcessLightList. Every light gets a colour from its owner:
  - missiles: an Orcl hue tint first, then holy bolt gold, portals blue/red/gold/violet, Blood Star red, then by damage type (fire orange, lightning blue, magic lavender, acid green, cold ice blue);
  - monsters: Diablo red, berserk blood red, otherwise the hue its TRN recolour moves it to, otherwise its champion affix colour;
  - flames (torches, candles, braziers, burning crosses, flame traps): warm, at 0.55 of a spell's strength;
  - anything else (the hero's light) neutral.
  A per-tile field holds the weighted colour; its strength is the coloured share of the tile's light (alone = full hue, beside the hero's white light = about half). `ApplyLightColour` shifts every world pixel's hue by the field under it (luminance kept), after DrawTileContent and before the monster outlines.
- Both passes skip town, 8-bit surfaces (the golden tests) and screen areas no coloured light reaches. The file is compiled with `#pragma optimize("gt", on)`, honoured by MSVC under /Od /RTC1 (checked: vectorised), so the passes are fast in the Debug build.
- Tests: `test/oracool_colour_light_test.cpp` - level brightness runs full to black, a fire light is orange and the hero's stays white, white light dilutes a shared tile, the smooth floor matches each tile at its centre and blends between, town/indexed/Nest gates. `DISABLED_ExportLightingPreview` writes three PNGs (today, smooth, smooth + coloured) to ORCL_FX_OUT; checked by eye this session.

## Stutter fixes

- **Frame-time line** (oracool/frame_timing): under the FPS counter when Show FPS is on - "worst N ms | avg N ms | >25 ms: N" over the last second, timed present to present in dx.cpp RenderPresent.
- **Monster sizes prebuilt off the main thread** (oracool/monster_scale): a worker scales Runt/Giant/Colossal sheets nearest-first from copies of the source bytes; a generation counter drops results from a previous level; PumpMonsterScale installs them each tick and swaps monsters already showing the unscaled stand-in. PrebuildMonsterSizes queues every sized type at level load. Corpses stay synchronous (a body keeps its sprites for good). Headless/test runs and a failed thread fall back to the old synchronous build.
- **PNG missile sheets kept across levels** (misdat): FreeMissileGFX parks found PNG rows in a cache and LoadGFX takes them back - the sheets quantise against the fixed town.pal, so nothing is level-dependent. Cleared in FreeGame.
- **Sounds, summons, companions preloaded** (oracool/level_preload): at level load the hero's skill cue sounds (class skills with points) load, the aura loop is cached, summon body types (waiting minions, Raise Skeleton/Skeletal Mage bodies) are added while leaving a free type slot, and waiting companions' dungeon sheets are prepared so their return does not load mid-play.
- Tests: `test/oracool_frame_smoothing_test.cpp` (the timing ring), `PngMissileSheetsSurviveALevelReload`, `MinionPreloadNeverTakesTheLastTypeSlots`.

## Known, to watch in play
- Sized monsters can show at 100% for a few ticks after arriving on a floor, then switch.
- A pixel is tinted by the floor tile under it, so a wall takes the colour of the tiles behind it.
- Where several coloured lights overlap the hero's light the blend can look muddy; ColourStrength (0.75) is the knob.

Next: the audit loop on this batch until no major issues, then the user's play pass. Shadows after that.

# 2026-10-01 - Levski's Cube: animated, opening and closing, with a live grid (v1.12.273)

**Date:** 2026-10-01. Debug only. The user asked for five things, from ChatGPT's green-screen stills ("not ideal, but i think you can make it work"):
1. an animated idle state;
2. an opening sequence;
3. an opened idle state;
4. the window's pink grid shown live, shrunk, on the sprite's grid;
5. a closing sequence when walking away.

Mid-way the user added: "we can reuse opening sequence for closing by reversing it."

## The sheet (tools/LevskiCubeSheet.cs, tools/build_levski_cube_sheet.cmd)

**Problems in the stills:**
- Each state was drawn at its own scale and offset.
- The opened frames are cropped at the sides and the top.
- Some frames carry slivers of their neighbours and white bars.

**What the tool does to each frame:**
- **Keys the green:** a pixel is background when green is at least 90 over its other channels. Fringe pixels are despilled, so their green never exceeds the other two channels.
- **Cleans:** 8-connected pieces under 12px wide, under 150 pixels, or three times taller than wide are dropped, as are white smears.
- **Normalises:** the base width is the widest row in the body's lower 40 rows. The slab is an isometric diamond, and its bottom rows taper to a point; measuring those blew the opened frames up about 2x on the first try. The frame is scaled so that width is 90px (the painting's footprint), centred, with its foot on the bottom row.

**Output:** one row of 26 frames of 128x192:
- closed idle 12;
- opening 8;
- opened idle 6, since stills 02 and 04 are cut off halfway down the panel.

The closing stills are not used. `levski_cube_frames.inc` carries the counts.

## In the game

- **Loading:** `LoadPngObjectSheetColoured` (sprite_import) loads `objects\<name>.png` as one row with its own colours. The town palette has no violet ramp, so a CEL would have lost the glow.
- **Applying:** `ApplyLevskiCubeSheet` runs after the painting CEL in `ApplyLevskiRoarGraphics`. Without the PNG, the painting stays.
- **Drawing:** `DrawObject` draws the Cube through `LevskiCubeColoursFor`, then `DrawLevskiCubeLiveGrid`.
- **State machine** (`ProcessLevskiCubeAnimation`):
  - closed idle loops at 4 ticks a frame;
  - the Cube's own window opening starts the opening, at 2 ticks a frame;
  - opened idle loops at 3 ticks a frame;
  - the window closing (X, Esc, or walking away) runs the opening backwards to the closed loop;
  - the lid can turn round midway.
- **Live grid:**
  - **Panel:** over the opened frames, a whole 3x4 panel of 11px cells is drawn at frame (48, 27)-(81, 71): a lavender glaze at 150/256 with pale rules. It hides the stills' cropped top.
  - **Items:** each of `GridItems`' items is shrunk nearest-pixel into its footprint, aspect kept.
  - **Measuring:** the panel was measured by hand on 4x zooms of the built sheet. Auto-detection (lavender runs, pale rules) kept latching onto the orb's glow and beams.
- **Archive:** oracool.mpq repacked (401 files).

## Test

- Debug build and ctest: 892/892 passed. The Debug `diablo.ini` md5 is unchanged.
- An offline composite of the six opened frames with the panel and a sample item checked the placement.
- Nothing seen in play yet.

## v1.12.274 - the Cube never moved in play

The user: "cube asset seems the same. no animations what so ever", then "the town was a bit special level", and "no opening, closing animations are played either".

- **Cause:** `game_logic` (diablo.cpp) runs `ProcessObjects` only outside town. The town branch has its own list (the stash chest's lid, the companions), and v1.12.273 had put `ProcessLevskiCubeAnimation` inside `ProcessObjects`. So in town the Cube never ticked. It stood on closed frame 1, which looks all but identical to the old painting.
  - The test built town and called the tick directly, so it passed.
  - The sheet itself was fine. A new test loads it from the archives (26 frames).
  - A second test builds town with graphics on, and the Cube wears the sheet and its closed loop advances.
- **Fix:** the tick is called from the town branch beside `ProcessTownStashChest`, and removed from `ProcessObjects`.
- **Also fixed:**
  - **Idle frames:** ChatGPT's 12 closed-idle frames differ by 0-77 pixels out of about 5,000.
  - **Colour folding:** the importer's colour fold took the sheet to 3 bits a channel, which erased even that. The tool now median-cuts the sheet to exactly 255 colours, so the game takes them as they are.
  - **Glow pulse:** the game pulses the Cube's colours on the clock. Clearly blue colours (the runes) breathe ±45% over 1.6 s. The open Cube's violet glow flickers ±12% over 0.7 s. Stone and gold hold still.
- **Robustness:** the tick puts the sheet back if anything hands the Cube its painting again.

Debug build and ctest: 894/894 passed (two new tests). The Debug `diablo.ini` md5 is unchanged across ctest.

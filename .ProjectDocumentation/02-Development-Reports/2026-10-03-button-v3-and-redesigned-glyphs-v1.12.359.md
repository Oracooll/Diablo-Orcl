# 2026-10-03 - Button v3 and the redesigned class glyphs (v1.12.359)

**Date:** 2026-10-03. Debug only; no RTM and no GitHub, at the user's word. All 915 tests pass. The Debug `diablo.ini` was unchanged by ctest. Commit eb5d425a.

The working builds v1.12.352-358 were never committed. This version holds all of their work.

## Button v3
- **The art:** the user's `Button v3.png` became `ui\button_v3.png`: 70x70, a 7 px band round a 56x56 gold face.
  - It went through three versions: a frame-only v2 at 69x70 and then 70x70 (shown at 48x48, then redrawn), Button v2 with the gold face, then v3.
- **How it is drawn:**
  - `DrawButtonBezel` stretches the band round each button with its corners 1:1.
  - The new `DrawButtonFace` scales the gold face into the button. The old glyphs draw on top.
  - On an indexed screen, or when the art is missing, the old plate draws instead.
- **Where:** every framed vendor and artisan button:
  - Griswold, Wirt and Adria;
  - Gillian's dice and Imbue icons;
  - Ogden's arrows;
  - both craft Transmutes;
  - the Cube's Transmute.
- **Spacing:** `ButtonFrameReach` = 7 replaces `GridBezelInset` in the buttons' clearances, so the 6 px gaps from v1.12.348 hold.
  - Griswold's row is now at x 35..304, y 112.
  - Griswold's Refresh until is at (120,644).
- **Size:** each button is 48x48 on screen: the 34 px face plus the band. The user saw the 48x48 version before this one.

## Reference cards
- **The test:** `OracoolPreview.DISABLED_NecromancerSheetGlyphs` draws one PNG per Abilities sheet. Each glyph is drawn 1:1 from the class strip, with no plate, on 0x00FF00, with its name.
- **Options:**
  - `ORCL_SHEETS_CLASS` picks the class.
  - `ORCL_NECRO_SHEETS_PLATES=1` draws the glyphs on the gold plates instead.
  - `ORCL_NECRO_SHEETS_OUT` sets the output folder.
- **Where they went:** `Resources\02. Oracooll Assets\<Class> Skills` for the Necromancer, the Rogue and the Monk.

## The user's redesigned glyphs
- **The delivery:** for each class, four `<Sheet> Glyphs Redesign` folders, each with 18 glyphs at 42x42: white, with a black shadow 2 px left and down, on green, plus `validation.txt`.
- **The script:** `tools\ApplyNecroGlyphRedesign.ps1 -Class Necromancer|Rogue|Monk`:
  - green becomes transparent;
  - white and black become 243/243/243 and 12/7/7, the two colours `IsGlyphFrame` recognises;
  - each glyph is centred 1:1 in its 56 px strip cell, so it sits centred on the gold plate (user: "don't upscale to 56x56. center the 42x42 glyphs onto the gold backings");
  - glyphs are joined to rows by name.
- **Results:**

  | Class | Glyphs replaced | Kept |
  |---|---|---|
  | Necromancer | 72 | 0 |
  | Rogue | 72 | 1 (his retired row) |
  | Monk | 72 | 0 |

- **Naming:** the delivery's "Bone Spires" is the table's Bone Spikes; the script maps it by an alias.
- **Monk:** Inner Sight and Spirit Ward still draw their vanilla icons in the Abilities window.
- **Caution:** re-running `tools\BuildGlyphStrips.ps1` would put the old pack glyphs back for these three classes.

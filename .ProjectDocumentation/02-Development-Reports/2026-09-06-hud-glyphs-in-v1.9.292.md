# The HUD glyph pack in: menu entries and tab numerals (v1.9.292)

**Date:** 2026-09-06
**Request:** "sweep mpq again for new items from gpt, when done."

## The sweep

`oracool-hud-glyphs-v1.zip` in the Oracool.MPQ root, unpacked under delivered-packs: the answer to the second brief. Eight 37x38 menu glyphs (character bust, sealed scroll, open rune book, cog, satchel, clasped tome, anvil, lined page) and ten 28x28 numerals, two-colour format, all 18 passing the pack's own verification. No states, as none were asked for.

## Consumed

- `tools/CutHudGlyphs.ps1` writes `ui\menu_glyphs.png` (304x38: eight 38-wide cells, the 37-wide glyph at x=0 of each, because DrawStripIcon takes square cells) and `ui\tab_glyphs.png` (280x28). MPQ repacked.
- hud_art: `MenuGlyphsArt` / `TabGlyphsArt` loaded, quantised, reset; `DrawMenuGlyph(out, cell, index)` and `DrawTabGlyph(out, cell, index, gold)`. The gold draw is a pixel loop over the quantised cell that moves the white (the grey ramp's light end) to PAL16_YELLOW+1 and leaves the shadow - the hover the user asked for, on a glyph instead of a font numeral.
- hud_menu.cpp draws the glyph over the plate and keeps the initial as the fallback; inv.cpp draws the numeral glyph, gold under the cursor, and keeps the font numeral as the fallback.
- Oracool.MPQ README row.

## Test

`OracoolAudit.EveryMenuEntryAndTabHasAGlyph` - all eight menu frames and all ten tab frames draw more than a threshold of pixels; the gold tab draw covers exactly the pixels the white one does.

Suite 634/635, the standing dungeon-generation failure only.

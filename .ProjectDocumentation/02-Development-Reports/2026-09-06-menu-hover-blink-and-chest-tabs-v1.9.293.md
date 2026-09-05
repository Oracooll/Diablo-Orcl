# The menu plates: white on hover, one blink then gold; the tabs as chests (v1.9.293)

**Date:** 2026-09-06
**Requests:** "hovering over the burger menu items to color the backing from gray to white and to blink once and hold gold on click." and "sweep mpq for the chest" (the pack answering the chest brief had landed).

## The menu plates (hud_menu.cpp)

The plate's colour is the state now: light grey at rest, WHITE under the cursor, and on a click one blink - gold for the first half of the 140 ms flash, grey for the second - then GOLD held for as long as the entry's window is showing. Game Menu, which opens nothing lasting, blinks and returns to grey. The hover shadow and the orange flash overlay are gone with their two constants.

`SkillPlateTint::White` is new: `SetSpellTransWhite()` in spell_icons.cpp is the Invalid (light grey) table lifted three shades toward the light end of the grey ramp, the mirror of `SetSpellTransDarkGrey`'s four shades down. `DrawSkillTintOutline` treats it like Unspent (no ring).

## The chest tabs

`oracool-tab-chest-glyphs-v1` (Oracool.MPQ root, unpacked under delivered-packs): two 28x28 glyphs, lid down and lid raised, one shared body from y=15 so nothing jumps, bounds x5..23. `tools/CutHudGlyphs.ps1` now cuts `ui\tab_glyphs.png` as this two-frame strip (the numerals of oracool-hud-glyphs-v1 lasted one build and stay in that pack). `DrawTabGlyph(out, cell, open, gold)`: closed chest on a closed tab, open chest on the open one, gold under the cursor. MPQ repacked; README row added.

## Editing note

Five of the six files touched are CRLF under core.autocrlf; an anchored edit that inserts LF text leaves mixed endings, which git hides but the working tree shows. The edit script now normalises on read and writes CRLF; the three files it did not touch were restored the same way. Two earlier scripts had also inserted a literal "$1" for a capture (a q{} replacement does not interpolate); those four anchors were restored from HEAD before this build.

## Test

`EveryMenuEntryAndTabHasAGlyph` now checks the two chest frames (the open one paints more) and that the gold draw covers the same pixels as the white.

Suite 634/635, the standing dungeon-generation failure only.

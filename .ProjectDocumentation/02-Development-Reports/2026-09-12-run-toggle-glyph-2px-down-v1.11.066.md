# The Walk/Run glyph sits 2px lower

**Version:** 1.11.066
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "bring the run/walk icons 2px downward."

## The change

`DrawRunToggleButton` drew the glyph centred in the belt cell, the same way the Menu and Town Portal
buttons do. It now offsets the glyph's rect by 2px on y:

```cpp
constexpr int GlyphDrop = 2;
const Rectangle glyphCell { cell.position + Displacement { 0, GlyphDrop }, cell.size };
```

Only the FIGURE moves. `DrawBeltSlotPlate` is still drawn on the true cell rect, so the plate, the
cell's hit box and the flash all stay where they were - which also means nothing about the click
target changed.

The text fallback ("R" / "W", used when the glyph strips are missing) takes the same offset, so the
two agree if the art ever fails to load.

Why it needed it and the two beside it did not: both strips carry the same 24px-tall traveller inside
a 30px cell, and the figure's own headroom is not symmetric - centring the frame put the body high in
the opening. The Menu bars and the Portal swirl are centred shapes and read correctly as they are, so
the offset is on this button rather than on `TryDrawBeltGlyph`.

## Verification

Debug and Release build clean; **718/718** tests pass. RTM updated.

The 2px is by eye on the user's side - it is a nudge against the painted cell, and only the real HUD
can settle it.

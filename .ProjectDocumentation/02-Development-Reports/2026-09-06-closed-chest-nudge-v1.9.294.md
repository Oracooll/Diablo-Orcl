# The closed chest nudged (v1.9.294)

**Date:** 2026-09-06
**Request:** "move chest icons in inactive only tabs in inv grid 3px up and 1px left. leave open chest icon alone - it is fine."

`DrawTabGlyph` (hud_art.cpp): `ClosedChestNudge { -1, -3 }` applied to the cell before either draw path (white 1:1 and the gold hover loop) when the tab is closed; the open chest draws where it did. Suite 634/635, the standing dungeon-generation failure only.

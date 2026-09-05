# The canvas dim: one pass, every window (v1.9.285)

**Date:** 2026-09-06
**Request:** "reduce it to one pass and apply to all canvases."

`DrawSidePanelDim` draws one half-transparent pass (a 50% blend with black through the palette's average table) instead of two, and `DrawSidePanelArt` calls it itself after blitting the canvas, so inventory, stash, character, quests, waypoints, spell book and the shop all wear it. The character sheet's own call went with that. The test was renamed `TheCanvasDimCoversTheOpeningAndSparesTheBezels`; its assertions hold for one pass.

Suite 631/632, the standing dungeon-generation failure only.

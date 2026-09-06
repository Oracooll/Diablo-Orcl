# The exported tileset pieces, stacked the right way up (v1.9.317)

**Date:** 2026-09-07
**Report from the user:** "they are extracted vertically flipped compared to what they look like in the game."

Checked category by category before touching anything: towners, monsters, objects, cutscenes and the UI CELs were upright. The tileset pieces were not - and not pixel-flipped either: each 32px block was upright, but the block ROWS were stacked in reverse, floors at the top of the 160px canvas and nothing in its bottom 32 rows. Measured over the first forty cathedral pieces (opaque rows 2..32 for floors, 1..128 for walls) rather than judged by eye.

`RenderTile`'s y runs the other way from `ClxDraw`'s for the tool's purposes: handing the floor pair the canvas's last row put it at the top. The tool now hands each pair its height from the bottom (`(row + 1) * 32`). The levels folder was cleared and re-exported: 5,975 pieces, floors at rows 130..159, walls rising from there; a wall piece's arch, seam and stones read as one surface.

No flip script: one would have mirrored the pixels inside every block, which were already right, and touched the other 1,100 files, which were never wrong.

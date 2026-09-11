# The shop's controls wear the vanilla button (v1.11.039)

**Date:** 2026-09-11
**Branch:** renderer-32bit (default), local commit

## What the user asked for

The user sent a screenshot of Griswold's shop:
- "these button dont look very nice. they lack consistency and dont fit too well. work on their size, posiiton, direction, color grade and try making them fit better."
- Then: "why dont you use vanilla buttons instead of these", pointing at `00-original-game-art\ui_art\but_sml`.
- Then: "we can use desaturated version of them and gold font or gold buttons and white font. i preffer the former."

## What was wrong

- **Direction:** the tab plate (`shop_tab.png`, batch 6) has its chamfered edge on the left and its flat edge on the right. It was drawn for a tab to the LEFT of its panel. The shop's tabs stand to the RIGHT, so the flat side faced the world and the chamfer faced the panel.
- **Colour grade:** the tabs were far paler (mean grey 147 at rest, 199 open) than the button plates (114) and the limestone (113).
- **Size:** controls were 26 thick, tabs 26 wide, and the gaps were 0px between service buttons and 2px everywhere else.
- **Labels:** the tabs' labels were stacks of single letters, squeezed to font 10 for "SUPPLIES".

## What changed

**The vanilla small button.** `ui_art\but_sml.pcx` is the front end's dialog button (`DiabloUI/button.cpp`). It is now read **from the player's own archive at runtime** with its own palette, and nothing is added to our mpq or the repo.
- Each palette entry resolves to a grey of its own luma × 0.9 (`VanillaGreyPercent`), so the button is desaturated at draw time and the file is never altered.
- Only the face is used: in each 112×28 frame, row 0 and columns 110-111 are padding, so the face is the 110×27 at (0,1).
- Frames 0, 1 and 2 are rest, pressed and lit. Lit is the front end's focus ring.

**Any size at 1:1.** `SliceButtonAxis` is public for its test. Per axis:
- a control longer than the face keeps both 8px ends and repeats the middle;
- a shorter one butts the face's first half to its last half.

It never scales, and the ring and bevel always survive. `OracoolShop.VanillaButtonSlicesCoverTheControlAndKeepBothEnds` pins exact coverage, in-bounds reads and both ends for every size the shop uses.

**The tabs** are the same button laid on its side. The image is transposed rather than rotated, so the light still falls from the top left.
- The open tab is lit, a hovered one is at rest, and the others are pressed in. The shelves not showing step back and the open one stands out.
- Labels read top to bottom, in gold. The line is drawn flat on a scratch 32-bit surface and copied a quarter turn clockwise. The font steps down from 12 only if a label doesn't fit.

**Geometry.** One 3px gap everywhere: between buttons on a row, between rows, and between tabs.
- Rows are 27 tall (the face's height), and tabs are 27 wide.
- Services are 92px each, centred in the 284 row.
- Rows sit at 60, 90 and 120, and the gold line ends at 136. That is exactly where the refusal toast's band above the grid begins, now pinned by a static_assert.
- The gold line sits on the pressed (recessed) face.

**Labels.** Gold throughout (`ColorWhitegold`, shadowed). The lit ring is the hover now, so a label no longer turns white. A pressed button's label drops a pixel with its face.

**Fallbacks.** If the archive lacks `but_sml`, or the target is an indexed surface, the controls fall back to the batch-6 limestone plates (a pixel short of the 27px rects), and the tabs to the stacked-letter labels. Without the plates too, they fall back to the flat fill and ornate border.

## Verification

Debug and Release built. ctest **703/703**. RTM refreshed with exe 1.11.039, and no mpq change.

A mock-up was composed off the exported frames and the panel art before building: grey ×1.0 against ×0.85, with a system font standing in for the game's. It is not the game. **Not seen in play**, and this is a screen drawn over the world, so only a screenshot settles it.

**To check at Griswold:**
- the bevel on every control, and the gold labels;
- Repair / Repair All / Recharge 3px apart;
- the tabs' labels reading downward, with the open tab lit;
- the refusal toast still clear of the gold line;
- a hover ringing the button.

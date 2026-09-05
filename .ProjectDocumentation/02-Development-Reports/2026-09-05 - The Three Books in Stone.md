# The Three Books in Stone (v1.9.270)

**Date:** 2026-09-05 · **Request:** "look for the new books templates i have added in oracool.mpq. they are bezels with transparent core. you fill-up the core with dark, transparent backing and fill in the Titles and the contents. Place the red X in the top right corner as per our rules for it."

## The frames

Two of the user's paintings, shipped 1:1: `book_frame_wide.png` (944x616, clear core at 21,24 of 902x568) for the Runeword and Crafting books, and `book_frame_tall.png` (420x620, core at 21,25 of 378x570) for Levski's recipe book. `oracool/book_frame.{h,cpp}` owns both: `DrawBookFrame` darkens the core with the item tooltip's two half-passes, lays the bezel over it, and falls back to the theme border if the painting is missing; `BookFrameCore` hands the core rect to whoever lays out inside it.

## The three windows

- **Runeword book**: frame, red X at the top-right, the title is GPT's engraved limestone RUNEWORD BOOK plate centred in the title band (text if the plate is missing). Padding 10 to 30, so the filter keys, rune row and list clear the 21-24px bezel.
- **Crafting book**: the same frame; its X is drawn centrally by the frame as a left-panel content, as before. Padding 12 to 30.
- **Recipe book**: was sized to its text (220-420 wide, up to 620 tall) on an opaque ground; it is the tall frame's 420x620 now, and its title, rows, selection band, clips and clicks all measure from the frame's core (`RecipeBookInner`) rather than the page rect. Scrolling is unchanged.

## Verification

Debug build clean; 625/626 with the standing `Drlg_l1` failure. In the game: the Runeword book (W), the Crafting book from the burger menu, and Levski's Recipes; the world should read dimly through each core, and no text should touch the bezel.

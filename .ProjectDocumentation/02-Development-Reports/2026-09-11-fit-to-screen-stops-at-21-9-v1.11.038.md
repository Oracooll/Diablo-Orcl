# Fit to Screen stops at 21:9 (v1.11.038)

**Date:** 2026-09-11
**Branch:** renderer-32bit (the default branch since today), local commit
**Request:** "when Fit to Screen graphics option is selected to expand the viewport all the way up to 21:9. no more, because it is becoming ridiculous."

## Before

Fit to Screen set the game's width to the desktop's own shape, with no upper limit. On a 32:9 monitor (5120x1440), a 1440p game ran 5120 pixels wide, which showed more of the dungeon to each side than any sane layout wants.

## Now

`FitToScreenMaxWidth(height)` in `utils/display` returns `height * 43 / 18`, which is the widest the view may be.

**Why 43:18.** "21:9" is marketing. Real panels come in two shapes:
- 3440x1440 is 43:18 (2.389);
- 2560x1080 and 5120x2160 are 64:27 (2.370).

The cap uses the looser of the two, so every 21:9 panel still fills edge to edge.

**Two places apply the cap:**
1. **`CalculatePreferredWindowSize`**, which sizes the game. Both paths are capped:
   - The integer-scaling path.
   - The fractional path, after its if/else. A stored width can already be the desktop's own (the resolution list derives it), and the `else` branch would keep it as it is.
2. **The resolution list** (`OptionEntryResolution`). Its "1440p" entries derive their width from the desktop too, so the saved width can never exceed the cap either.

**On a wider desktop.** `SDL_RenderSetLogicalSize` already letterboxes, so the 21:9 view is centred with black bars left and right. The mouse mapping (`OutputToLogical`) already subtracts the viewport offset. 16:9, 16:10 and 4:3 desktops are unaffected, and so is 21:9.

**Option text.** The Fit to Screen description now says "(up to 21:9)". The msgid changed, so that one string shows in English until the translations catch up.

## Verification

Debug and Release built. ctest **702/702**, with the new `OracoolDisplay.FitToScreenStopsAtTwentyOneByNine`: 3440 at 1440p, and 2560 and 5120 panels filled. The test covers the cap itself, not the SDL window path. RTM refreshed with exe 1.11.038.

Not seen in play. The check is on a 32:9 monitor with Fit to Screen on. Expect black bars left and right, a centred 21:9 view, and the cursor landing where it points.

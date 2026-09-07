# Loading screens in true colour, fit to height (v1.11.003)

**Date:** 2026-09-07. **Branch:** renderer-32bit.
**Request:** "00-original-game-art\gendata\ - make loading screen use these images, fit to height, respecting aspect ratio." Then: "these pngs were original art. are we allowed to pack them in my mpq file and distribute them through github? they are intelectual property of blizzard."

## No. So nothing is shipped

The first cut packed the nine extracted PNGs into oracool.mpq. That would have redistributed Blizzard's paintings, which DevilutionX's own rule (ship no original assets) and this fork have always avoided. The PNGs came out again before anything left the machine, and the local commit was rewritten so they are not in the history.

## What ships instead: a conversion at load

The cutscene is still read from the player's own diabdat.mpq, CEL and palette, exactly as before. What is new is that interfac.cpp decodes the CEL through its palette into a 32-bit XRGB8888 surface once at load (`BuildCutsceneRgb`), and the loading screen is drawn from that: scaled to the screen's height with the aspect kept, centred with black either side, blitted straight onto the 32-bit back buffer. At 960x720 that is an exact 1.5x. The same pixels the PNG would have given, and the first art in the game that never passes through a palette lookup at draw time - a taste of stage 2. The CEL draw stays for an 8-bit surface.

The progress bar rides the scaled painting: BarPos, the bar's width and its 22px height were authored for 640x480 and scale with it. It is drawn through the surface's own FillRect now - SDL_FillRect with a palette index on a 32-bit surface wrote the index as a colour, which was the blue bar in the first-look screenshot. The options menu's slider fill had the same raw fill and takes the same fix.

Suite 694/694; oracool.mpq back to 430 files. RTM refreshed.

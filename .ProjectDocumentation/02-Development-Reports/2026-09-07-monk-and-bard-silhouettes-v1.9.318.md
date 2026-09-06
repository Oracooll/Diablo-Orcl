# The Monk and Bard silhouettes (v1.9.318)

**Date:** 2026-09-07
**Request:** "look for bard silhouettes in mpq" (after "we are missing monk silhouette. make prompt for gpt").

No Bard silhouette existed anywhere in Oracool.MPQ: she was borrowing the Rogue's, and the reference sheet the four originals were cut from left with the 04-references folder. What does exist is her painted class figure, `02-source-art/class-art/class-bard-greenscreen.png` (1024x1536, green-keyed). The new `tools\MakeClassSilhouette.ps1` turns such a figure into the house silhouette: green keyed out, the figure scaled to fit 245x356 with 8px of air, luminance mapped onto the 8..105 near-black band the Monk pack was normalised to, a binary alpha edge. Her silhouette reads as the others do - lute, cloak, boots - and ships as `ui\silhouette_bard.png`.

The Monk's silhouette had meanwhile arrived in the drop zone (`oracool-monk-silhouette-v1`, to the brief written that morning): 245x356, bald warrior monk with the staff, the right style. Filed with its zip, shipped as `ui\silhouette_monk.png`.

`SilhouetteForClass` (hud_art.cpp) now returns a figure for every class; the Bard no longer maps to the Rogue's. MPQ repacked. Suite 683/683.

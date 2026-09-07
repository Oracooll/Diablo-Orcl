# The private archive: derivative art stays in the mod, leaves the repository (v1.11.004)

**Date:** 2026-09-07. **Branch:** renderer-32bit.
**Request:** "all except the front end paintings and the HUD include blizzard textures/elements in order to feel Diablo-esque. They maybe should not be distributed over the internet freely. But they need to remain part of Orcl mod." Then: the 16:9 redo of the nine cutscene paintings - "Use them in the game, but keep them with other blizzard IP. Make them fit height of screen when displayed."

## The split

Two archives. `oracool.mpq` (403 files) holds original work only and is what the repository, a release zip and the wiki carry. `oracool_private.mpq` (36 files) holds the art the user built from reworked Blizzard textures - the 340x720 canvas, the inventory panel, Levski's Roar skin and its 18 button states, the two book frames, the waypoint panel and icons, the monument and waypoint sprites, and the nine 16:9 cutscene paintings - packed from `Resources\03-private-assets\oracool_private_assets`, a folder OUTSIDE the repository that is never pushed. The engine mounts the private archive ahead of the public one when it is present (init.cpp, assets.cpp). CMake packs it when the folder exists and says so in the configure log; tools/build_oracool_mpq.cmd does the same; the release packager's fixed manifest cannot include it.

## A public build without it

Every consumer of that art already tolerated its absence (the HUD art loader warns and draws without; the book frame falls back to the theme border; Levski's skin is skipped) except the two object sprites, whose loader is fatal on a missing file: `EnsureObjectGraphicsLoaded` now falls back to a vanilla sprite (the magic circle for the waypoint, a book for the monument) with a warning. The loading screen falls back to the CEL decoded from the player's archive.

## The 16:9 paintings

`gendata\<name>.png` is tried first, from the private archive; it is converted to XRGB8888 once and scaled to the screen's height with the aspect kept, so a 1280x720 painting fills a 960x720 screen edge to edge with 160px cropped either side. The progress bar assumes the original 4:3 picture sits centred in the wider painting and offsets by that margin.

## History

The derivative files remain in the pushed history from the commits that added them (August). Purging them is a history rewrite and a force push; the user's call, at the next push.

Suite 694/694. The wiki rebuilt without the private sprites (72 inlined, was 96). RTM refreshed with the exe and both archives.

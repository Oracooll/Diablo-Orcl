# The original art exported through the engine; the Monk's ability ceiling at level 30 (v1.9.316)

**Date:** 2026-09-07

## The export

**Request:** "sweep diabdat.mpq and hellfire mpq file and extract all possible original art in png format and arrange it in ...\00-original-game-art\. Some is already extracted, so extract only what isn't extracted yet."

The archives carry no listfile, and CEL/CL2 files carry no widths - the game supplies both at every load site. So the exporter is the engine: `tools/oracool_art_export.cpp`, a CMake target (`oracool_art_export`, excluded from ALL) that links libdevilutionx, mounts the archives from the build directory, and for each category loads through the game's own loaders - the UI CELs by the widths their call sites pass, items from the drop table, objects from the object table, missiles from the missile table, monsters through `InitMonsterGFX` with each type's width, towners by their names, cutscenes with their own palettes, PCX by its own, and the tilesets piece by piece through `RenderTile` - then draws each sprite into an 8-bit surface and writes it as a palettized PNG (palette embedded, index 0 transparent). Bit-exact with the game. Anything already present is skipped, so the tool fills gaps on re-runs.

Two engine accessors were added for it: `GetNumInvItemsInSheet` (cursor.cpp) and `GetItemDropName` (items.cpp). Each monster runs in a child process (the job passed through the environment - a root under "2. Personal Files" split argv), so the one row with broken data - the unused dark mage, whose walk is a one-frame placeholder - costs itself, not the run.

Result: 7,086 files on top of the 950 already there - 5,975 dungeon pieces across eight tilesets, 300 monster sheets, 100 front-end PCX frames, 63 objects, 52 missiles, 43 items, 19 towners, 9 cutscenes, 240 cursor icons and the UI CELs. The folder's README describes the layout and the re-run command.

## The Monk's ceiling

**Request:** "monk has lvl36 active skills. i dont want that. active skills ceiling lvl is 30. move lvl 36 to lvl 30 row. only passive skill go as far as lvl 36."

The three Way masteries (Master of the Long Staff, Perfect Vessel, Enlightenment) were each Way's seventh rung at level 36. They sit in the level-30 row now, column 1 beside the Way's capstone, per the column rule; the rank-cap test that pinned the level-36 gate pins level 30, and the header's Monk note says why. The Passive Skills page keeps its level-36 tier.

Suite 683/683.

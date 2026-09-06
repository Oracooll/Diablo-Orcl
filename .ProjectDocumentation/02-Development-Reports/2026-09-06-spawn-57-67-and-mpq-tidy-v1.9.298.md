# The hero spawns at 57,67; the Oracool.MPQ root tidied (v1.9.298)

**Date:** 2026-09-06
**Requests:** "tidy up oracool.mpq" and "make hero spawn coords - 57,67."

## The spawn

Three places name the new-game town tile and must agree: `CreateTown`'s ENTRY_MAIN view (town.cpp), `SetupLocalPositions`' spawn table (multi.cpp) and the town-objects test's `NewGamePlayerSpawnTile`. All three went from 55,67 to 57,67, one tile east of the Stash Chest at 56,67. The town-objects furniture-collision test passes at the new tile, so nothing stands on it.

## The tidy

Oracool.MPQ's root holds only the README and the user's four working files (the canvas, the Levski painting with its .pdn, the two book templates) - those stay because the cutters read them from the root. Moved: the three glyph-pack zips beside their unpacked folders under delivered-packs; the sixteen builders and prompt JSONs from `ChatGPT Assets` into `02-source-art/chatgpt-build-scripts`; the white-marble panel texture into `02-source-art/textures`. Deleted: `Runeword Book.png`, a 2 KB empty layer export the README already described as such. The README's sweep note and rows were updated. The emptied `ChatGPT Assets` folder is held open by OneDrive and will go once it lets it.

Suite 634/635, the standing dungeon-generation failure only.

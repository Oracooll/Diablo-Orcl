# 2026-10-14: Orcl Act, 100 x 100 woods and the first Release play test (v1.201.025-030)

Branch orcl-act. Debug ctest 1066/1066 at every version. The Release tree is `C:\Diablo Orcl\x64-Release-OrclAct`.

## 100 x 100 woods floors (v1.201.025)
The user said: "go with 100x100 for now. put 224x224 in the roadmap". The 224 x 224 map is on the Orcl Roadmap as Idea `orcl-act-224-map`.

- **gendung.h:** `PlayAreaOrigin()`, `PlayAreaTiles()` and `PlayAreaCells()` return 16 / 80 / 40 on every floor and 6 / 100 / 50 on the woods. `MaxPlayAreaCells` is 50.
- **woods.cpp:**
  - `Lo` = 6 and `Hi` = 105; the woods' own cell grids are `WoodsCells` = 50.
  - The minimap reads its own `MapCell[50][50]` through `WoodsAutomapCell`, not `dungeon[][]`.
  - The forest past the 112 x 112 map's edge is drawn from a shifted mirror of the band, not left black: `WoodsMirrorBeyondMap`, `DrawWoodsBeyondMapFloor/Tree`, and in the plain Tristram look the mirror's own cels.
- **Engine:**
  - `AutomapView` is now [50][50]. The automap and minimap index it through the play area. Saves write and read `PlayAreaCells()`.
  - `msg.cpp LocalLevel` is resized to match.
  - Random monster, object and item placement covers the play area.
  - The debug reveal and the Secluded shrine cover the whole play area.

## Mob roster and boss (v1.201.024, audited in v1.201.026-027)
- **Fights as a boss:** the woods boss passes `FightsAsUnique`, so no Berserk, Doppelganger or full freeze, and he can't be revived.
- **Loot:** a rift guardian's loot plus a pile of 4. The King and the Butcher drop no quest uniques and give no quest credit.
- **Scaling:** done on the ladder, with Diablo treated as floor 16. His Apocalypse takes his damage percent, applied once.
- **Placement:** inside the fence, off the houses. On the shareware build he is only a kind the game has.
- **Death:** the Reaper leaves no shrunk body and dissolves with the option on.
- **Champions:** the King's raise type is never a champion's body.

## The play test (user's Release clips 19:48 and 19:57, ten dev notes)
- **"game ended abruptly" (both clips):** this was me. I stopped `DiabloOrcl.exe` by name before each Debug build and closed the Release game the user was recording. No crash was logged. The memory rule now allows closing a process only after an LNK1168, and only by its exe path.
- **"mobs just appear out of thin air" / "disappearing and reappearing depending on range":**
  - A monster's body was drawn only inside the hero's light radius, but its shadow further out.
  - `lighting.cpp` outdoor sight now marks woods tiles within 20 as Lit. Visible is unchanged, so monsters notice the hero as before.
  - The gamepad still aims only at Visible monsters.
- **"some mobs are very dark":** these were the hero-light shadows, solid black. The woods now have the sun's shadows by day (`scene_fx` sun on `IsUnderSky`).
- **"7 minutes ... i dont really notice night time":** day and night was Tristram-only.
  - `PlanTownDayNight` now runs under every sky. The GPU path combines it with the coloured light (`CombineTintPlans`).
  - The townsfolk's lamps stay in town.
  - A full day is 20 minutes.
- **"description popup ... superlarge dimensions" (LMB/RMB menus):**
  - A stale item hover turned the skill's hint text into an item card, with an empty, screen-high sprite slot. Salvation, Resist Cold and others were affected.
  - The card asked for in a draw now wins that draw, with its own text and colours.
- **"blood star ... reduce its light radius by half":** the light radius is 4. Projectiles now keep their radius as they move; a hard 8 had restored it.
- **"mix and diversify mobs":** each floor now draws six kinds from eight candidates, a different sprite family each while one is left, scaled up or down to the rung.
- **"we need to design new loading screens. prepare chatgpt prompts":**
  - Two prompts are in `Resources/ChatGPT RfA/RfA - Orcl Act loading screens.md`: `gendata/cutwoods.png` and `gendata/cutwcave.png`, 3440 x 1440.
  - The game already loads them. Until they exist it shows Tristram's and the Caves' 21:9 paintings.
- **"the grass seems to be vanilla textures":** the build ran the Mixed look (Tristram grass).
  - The user then asked for the Orcl textures, so Woods Painted Art is on by default and in both `diablo.ini` files.
  - Its ground is now cached per tile: about 60 ms to 8 ms a Debug 1080p frame, in `DISABLED_WoodsFrameBenchmark` "paintedGround".
- **Gameplay clip:** `Diablo Orcl/Trailers/Orcl Act - Woods Gameplay (draft 1).mp4` (89 s, with a cut list next to it).

## Audits
Five rounds:
1. 18 findings (no P1), all fixed in v1.201.026.
2. 2 P3s, fixed in v1.201.027.
3. 3 P2s and 3 P3s, fixed in v1.201.030.
4. A check of the round 3 fixes.

None of these changes has been seen on screen yet.

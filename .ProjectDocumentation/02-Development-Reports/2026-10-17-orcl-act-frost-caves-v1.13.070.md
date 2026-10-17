# Orcl Act: the Frost Caves, engine steps 1-2 (v1.13.070-071, 2026-10-17)

User, 2026-10-17: "start the frost caves engine work while waiting". Plan: `.ProjectDocumentation/07-Backlog/2026-10-16-orcl-act-frost-caves-plan.md`.

## v1.13.070: room for eight more levels
- **The level count:** `NUMLEVELS` goes from 25 to 40. The set levels index the same per-level tables as the dungeon floors (`glSeedTbl`, `_pLvlVisited`, `_pSLvlVisited`, the level files), and the Orcl Act's had reached 24.
- **Save headers:** saves are written under four new ones (`OSHR`, `OSHL`, `ORTL`, `OHLF`) carrying every level. The old four (`SHAR`, `SHLF`, `RETL`, `HELF`) still load with their 17 or 25.
- **Level file names:** `GetSaveNames` and the game/hero sentinels go by `NUMLEVELS`, not the save's count, so a level file keeps its name in either layout.
- **Seeds:** the seed loop now draws 40 seeds; the first 25 are unchanged.

## v1.13.071: the Frost Caves exist
- **Levels:** eight set levels, `SL_ORCL_FROST_1..4` (25-28) and `SL_ORCL_FROST_RIVER_1..4` (29-32).
- **Helpers:**
  - `IsFrostLevel` and `IsFrostRiverLevel`.
  - `IsOutdoorActLevel` now means a floor on the woods engine, and includes the frost floors.
  - `IsActCaveLevel` includes the rivers; `IsActBossLevel` includes the Throne of Winter.
  - `OrclArea::Frost`, with `AreaIndex` 2 and `OrclAreaCount` 3.
- **Underground:** the frost floors are DTYPE_WOODS, but `IsUnderSky` and the woods' outdoor sight leave them out (`OnFrostCavesFloor`), so they have no weather, no day and night, and no 20-tile sight. `CreateWoods` skips the all-round light, and crystals are real dungeon lights (`AddLight` in `BuildWoodsLevel` on every build: 7 for a cluster, 4 for a small one).
- **The generator (`GenerateFrost`):**
  - Cellular caves on the 50 x 50 cells: a seeded fill per floor (0.50 / 0.54 / 0.50 / 0.51), smoothed 5 times, the edges roughened.
  - The trodden way from the arrival to the way on, cut through the rock.
  - The way down to the river at an open spot, joined to the way by a trail.
  - Each floor's sights:
    - the Rime Gate: the war's last stand and its camp;
    - the Glacial Hollows: frozen falls, ice pillars, crystals;
    - the Frostbound Deep: the beast in the ice, the frozen dead;
    - the Throne of Winter: pillars.
  - Clutter on the open floor, then the unreachable sealed.
  - Every rock tile facing open ground is a RfA-77 wall piece, and two in five of the next row in.
  - `FrostRockDepth` shades the rock behind the walls to near black (in `DesertGroundFeature`), so the caves read as caves and not as a snowfield with rows of rocks (the first render).
- **Art:** 42 approved pieces in `levels/frost/` (the RfA-76 to 81 batches): 8 ground textures, the scatter sheet, 26 standing kinds (walls at 40-46%, landmarks at 36-42%, the rest at 29%), 3 lying decor, and the 2 ways down as soft decals (`FrostEntranceFiles`: the hole on floors 1 and 3, the stairs on 2 and 4).
- **The frozen rivers:** four one-floor caves on the Caves tileset, one under every floor. `TintFrostCavePalette` turns the level half (32-127) to ice and black stone, and the lava range (1-31) to pale frozen water.
- **Getting there:**
  - The Tristram gate is on the east edge at x 83-84, y 29-32, with a gate mark; the return tile is two in.
  - Waypoints 33-36 (`FirstFrostWaypoint`), carried in the 64-bit hero chunk.
  - Names: the Rime Gate, the Glacial Hollows, the Frostbound Deep, the Throne of Winter; the Frozen Stream, Black Ice Run, the Drowned Hall, the Still River.
  - Loading screens: `CutFrost` and `CutFrostRiver` (RfA-75's `cutfrost`, `cutfriver`).
- **Monsters:** the woods' roster and bosses until the Designer's Frost roster is saved.
- **Tests:**
  - `FrostCavesAreWalkableCaverns`: 4 floors x 5 seeds, both ends and the way down reachable, open floor 20-60%, lights on the Hollows.
  - `DISABLED_FrostFloors` previews.
  - `EveryDungeonLevelSitsOnExactlyOneAct` updated for 13 Orcl rows.
  - Full suite 1072 of 1072.

## Next
- The game-drawn frost effects: crystal glow, breath, snow sifting, cold mist, and the frozen river's shimmer.
- Tristram's painted mouth: RfA-79's `mouth_tristram`, drawn in town.
- The Throne's pieces (RfA-82, 82b).
- The Glacial Hollows' lakes, bridges and the RfA-81 pieces placed.
- The Frostbound Deep's pieces (RfA-81b).
- The interactive frost objects (RfA-83).
- The frost roster with its recolours and ice attacks (the Designer's Frost Caves roster).
- The Frost tab in the Level Designer.
- The audit loop.

## v1.13.072: the Hollows' lakes, the Deep's dead, Tristram's mouth, the crystals' glow
- **The Glacial Hollows' frozen lakes (RfA-81):**
  - Two pools of black ice (the `OasisMud` slot of the Frost ground table), each ringed with lake stones (decor).
  - One or two holes of dark water in each pool, and a fish frozen in a block of ice on the shore.
  - Two crevasse lips, which block their footprints.
  - Two crystal spires, which are lights.
- **The Frostbound Deep (RfA-81b):**
  - A barrow field of two burial mounds and a rune stone.
  - Chains with the kneeling dead, plus a lone kneeling corpse and a lone rune stone.
  - One wall piece in six holds the dead (`WallFaces`).
  - A tomb door is cut into the wall once, where open floor lies in front of it.
- **Tristram's mouth:** RfA-79's painted `mouth_tristram` stands behind the east gate (`DrawTownFrostMouth`, from scrollrt's town cell pass), at half its painted size and lit with the town.
- **The crystals' glow:** clusters, small crystals and spires each get a slow cold blue shine in the glow pass, on top of their `AddLight`.
- **A bug caught in the render:** `Unit()` reads the low 24 bits, so `Unit(h >> 12)` never passes 0.0625, and every Deep wall had become a wall of faces. The faces now roll their own hash. The `h >> 8` uses elsewhere keep 24 bits and are fine.
- **Tests:** full suite 1072 of 1072.

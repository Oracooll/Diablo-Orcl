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

## v1.13.073: the Throne of Winter's hall, and every waypoint named
- **The Throne of Winter's hall (RfA-82, 82b):**
  - **Layout:** a 13 x 13 open spot of carved paving (`ArenaCentre`), running along +x, down and to the right on screen.
  - **The pieces:**
    - the frozen throne at its head, with a frozen brazier either side;
    - a ring of six rune stones in its middle (three kinds, one cracked);
    - three pillars down each side, some broken;
    - the old king's statue among them;
    - the ice arch at its foot.
  - **The boss:** waits before the throne (`WoodsBossSpot`, hall - (1, 0)).
  - **Footprints:** the throne and the arch block theirs.
  - **The first layout** ran the hall along the screen's vertical, and each side's three pillars stood in one line and read as a single column.
- **Waypoint names:** every Diablo and Hellfire dungeon waypoint has its own name, replacing "Region Level N" (the user approved the suggested names, 2026-10-17). For example: The Sanctum Stair, The Bone Labyrinth, The Lava Run, The Throne of Terror, The Defiler's Lair, Na-Krul's Prison.
- **Tests:**
  - `FrostCavesAreWalkableCaverns`: the hall is on the Throne of Winter alone, and the floor before the throne can be reached.
  - The preview renders the hall.
  - Full suite 1072 of 1072.

## v1.13.074: the Frost Caves' monsters in their frost looks
- **The looks:** every monster on a Frost Caves floor or frozen river draws in its kind's frost look (`FrostLookFor`), on the 32-bit screen and lit as it stands. A unique's own colours go under it, unlit, as uniques glow. Ice, stone, infravision and the chill win, as they do over everything.
- **Ported:** the five looks are the Designer's preview colour maps (`FrostLookTable`): Rime (the game's frozen look), Glacier, Hoarfrost, Frostbite and Black ice.
- **The roster (`FrostKinds`):** the woods' kinds, all in Rime, until the Designer's Frost Caves roster is saved (none saved yet). A kind off the roster, such as a boss, wears Rime too.
- **RfA-83:** the interactive objects are approved and in the package (`levels/frost/objects/`); swapping them in for the vanilla objects is a later engine step.
- **Tests:** `FrostCavesMonstersWearTheirFrostLook` checks the look is used in the Frost Caves alone, that each look turns a warm colour cold (Frostbite mutes it) and that dark colours stay dark. Full suite 1073 of 1073.

## v1.13.075: the Frost Caves' monsters shoot ice
- **The swap (`FrostMissileFor`, woods.cpp):** on a Frost Caves floor or frozen river, a ranged monster's missile becomes its ice version; anywhere else nothing changes.
  - Arrows, firebolts and the flares: Ice Bolt.
  - Acid, blood stars, fireballs and magma balls: Ice Blast.
  - Diablo's Apocalypse and the storm lightning: Glacial Spike.
- **Where:** `GetMonsterMissile` wraps `GetMissileType` for `AiRanged` and `AiRangedAvoidance`; the skeleton archers' own `StartRangedAttack(Arrow)` goes through it too.
- **Why these three:** they roll the monster's own damage when a monster fires them. The Frost Arrow does not (a monster's deals 1), so the archers fire Ice Bolts.
- **Tests:** the swap in the Frost Caves, a charge staying a charge, and the woods untouched. Full suite 1073 of 1073.

## v1.13.076: the area objects (RfA-83, RfA-84)
- **The paintings (`DrawAreaObject`, from scrollrt's `DrawObject`):** on the Frost Caves' and the desert's floors and caves, the vanilla object's sprite gives way to the area's painting, at half its painted size, standing on the tile's middle. The after picture shows once the object is used (`_oSelFlag == 0`): broken, opened, spent, searched.
  - **Frost:** barrels and urns (frozen barrel), chests (ice chest), shrines (ice shrine), candle stands (torch stand), corpses and sarcophagi (corpse in the snow).
  - **Desert:** sarcophagi, shrines (sun shrine), candle stands (bronze brazier), corpses (wrapped corpse on its bier). Its barrels and chests keep the game's own.
- **The candle stand** always shows its lit picture: the game's light is on it.
- **Hover:** the game's outline colour (palette 194) is drawn round the painting.
- **Verified** in a rendered row of every object, before and after, on the frost floor.

## v1.13.077: the waypoint pads
- The frost pad (RfA-79) and the sun pad (RfA-84) replace the waypoint on those floors, at the game pad's width (75 %).
- A lit waypoint (`_oAnimFrame == 2`) gets a soft additive glow over the pad, ice-blue or gold; the paintings are unlit by brief.

## v1.13.078: the Still River's frozen captive (RfA-85, 85b)
- **The object:** a large chest of three real items, untrapped, marked by `_oVar3 = CaptiveMark`, placed on the Still River's first build at the longest walk from the landing. Named "Frozen Captive"; drawn as the robed figure in ice, shattered once opened. No quest (the user: "My act doesn't add Quests for now").
- **The art:** RfA-85's broken state had three dark shapes in its base that read as letters ("ROX"); sent back as RfA-85b. Its redo still had see-through gaps in that patch, which I filled from the ice round them.

## v1.13.079: the way on
- RfA-79's `passage_on` stands on the rock tile just past the trodden way's far end (`Tree[Lo - 1][WayOn.y]`) on floors 1-3; the fourth ends in the throne hall.

## v1.13.080: the cold
- **Sifting snow:** 90 slow pale flakes on every Frost Caves floor and river at every hour (`ApplyAfterWorld`); the sky's weather never reaches underground.
- **Mist:** the ground mist lies cold blue on the floors all day (strength 0.7), and the rivers' Caves mist turns blue.
- **Breath:** every two or three seconds a pale puff at the hero's mouth, drifting the way he faces (`TickFrostBreath`, dust particles).
- **The river shimmer** needed no code: the frost tint makes the river's cycling colours pale blue, so post_fx classes it as water and gives it the water light, not the lava's heat haze or glow.

## v1.13.081: the menu clip, the Hellfire act, the cave landings
- **The idle menu** plays Diablo's or Hellfire's intro at random, each only if the archives hold it, on its own roll (no game seed moves).
- **The Hive and the Crypt scale with the hero** (the user, 2026-10-17): each floor 17-24 fixes its rung the first time it is entered in a game - a third of the hero's level (at least 1), plus one per floor down, capped at 16 (`FixHellfireFloorRung`, called before anything is placed). `AreaLevel` uses it for loot; `ScaleHellfireFloorMonsters` rescales the floor's monsters to it after the theme rooms. This replaces the 2026-09-12 decision to keep them weaker than Hell.
- **Audit - walled-off cave landings:** while testing the captive, its walk from the landing reached only 24-155 tiles of about 2,400 in some builds. Measured over 180 builds of six caves: a third of first builds reached under four fifths of their floor. The Caves generator checks its floor joins up before it lays the lava rivers and fences, whose one-tile lines can then wall the stairs' corner off. `BuildWoodsCaveLevel` now walks the floor from the landing (`WalkFromLanding`: open tiles, diagonals where one side is open, ignoring objects) and builds again from the next seed until 80 % is reachable, up to 12 times; the accepted seed is kept, so a revisit rebuilds the same floor. Whether the hero was really boxed in during play was not confirmed; the rebuild costs nothing when he was not.
- **Tests:** the rung rule; every Orcl cave built 30 times, each landing reaching four fifths of its floor. Full suite 1073 of 1073.

## v1.13.082: the light shafts
- Two per Frost Caves floor, on open floor chosen after the unreachable pockets are sealed (placed before the seal, some first landed inside a pocket), each with a light of radius 6. The snow heap (`light_shaft.png`, RfA-79b) is drawn there as the desert's craters are once it is delivered; until then the shafts are light alone.
- **Tests:** two per floor, each walkable. Full suite 1073 of 1073.

## Designer: the Frost tab (Version 25)
- The Orcl Act Level Designer has a Frost tab beside Woods and Desert: 8 grounds, 15 walls and pillars, 32 mid props, 12 scatter details, at the game's own scales and foot points, on a dark cave wash. It saves under `settings/current` -> `frost` {ground, objects, placed, effects}, beside the roster's `frost.mobs`.
- No Frost roster has been saved yet, so every kind keeps the Rime look and the default ice attacks.

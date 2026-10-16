# Orcl Act: the Desert (v1.13.059, 2026-10-16)

User, 2026-10-16:
- "design the 100x100 levels of Desert Area. Implement whatever assets we now have into them"
- "Mob Roster needs to be per Act Area"
- "implement all the weather effects including your suggestions of sand storm"
- "Try to draw an in-game Sand Hole which takes player to Desert Caves. Invent properly tinted Desert Caves"
- "run an audit loop until all issues are fixed"

## What was built

### Levels
- **Floors:** four set levels, SL_ORCL_DESERT_1..4 (ids 18-21). Like the woods, they are DTYPE_WOODS on the woods generator, here in its desert mode (woods.cpp `Desert`).
- **Caves:** three desert caves, SL_ORCL_DESERT_CAVE_1..3 (ids 22-24), on the Caves tileset with a sandstone tint on the palette's level half (32-127).
- **Floors and what each has:**

  | Floor | Its look | What is on it |
  |---|---|---|
  | The Salt Road | sand, ripples, clay, bedrock | an oasis; a sand hole down to the Sand Warrens |
  | The Ash Dunes | more dunes | an oasis; the giant rib cage |
  | The Glass Flats | more clay and rock; petrified trees, black glass | ruins; a sand hole down to the Buried Halls (two floors) |
  | Tomb of the Sun-Eater | buried paving | ruins; the rib cage; the boss |

- **Edges:** rock walls round each floor (hoodoos, outcrops, boulders). There are no fences, cottages, river or wisps.
- **Roads:** the salt road runs from the arrival to the way on. Vanilla shrines, chests and Hellfire's clay urns stand along it.

### Drawing
The game draws these itself, into the cached ground: the **oasis pool** (dark water, the sky's reflection, glints, wet mud) and the **sand hole** (a ragged funnel lit from the top left, sand sliding in, a cracked rim). Grains trickle into the hole, drawn in the Beams pass.

### Art
Everything approved so far is in the game, under `levels/desert/`:
- RfA-65: ground and scatter;
- RfA-66: the big props;
- RfA-67: the mid props;
- RfA-68: oasis and ruins.

Each ground texture's brightness is brought close to the sand's, so the tile grid does not show at material edges. Scales are the Level Designer's Desert-tab starting values.

### Getting there
- **From Tristram:** a gate at x 72-75, y 11-12, east of the cow pen, with a green tint. The return tile is (73,14).
- **Waypoints:** 29-32, under the Orcl Act tab.
- **Loading screens:** for now Tristram's out to the desert, the Caves' into its caves.

### Monsters
- **Roster:** the user's desert roster from the Level Designer, saved 2026-10-16 11:54, 28 kinds (DesertKinds).
- **Bosses:** the woods' rule applied to it: of the kinds ticked, those the woods use as bosses, which are the Skeleton King, the Butcher and the Reaper. Each is drawn at Diablo's height, and one is rolled per game from the desert's own seed.
- **Woods roster:** restored to the game's 41 kinds and 4 bosses in the Designer, after the user unticked them all by accident.

### Weather (scene_fx `Weather::Sandstorm`)
- **The sandstorm:** a gusting tan haze and streaking grit.
- **Drifting sand:** always low over the ground, heavier in a storm.
- **Heat:** shimmer on the upper rows, and a mirage glare at the hottest hours. It obeys the Heat Haze option.
- **The sky's life:** dust devils and vulture shadows by day.
- **Dusk:** a red sun at dawn and dusk, while midday stays the town's.
- **Each sky keeps its own weather:** no rain, puddles or fireflies over the desert, and no sandstorm elsewhere.

### Designer
The **Orcl Act Level Designer** (renamed from the Woods Levels Designer) has Woods and Desert tabs, and a Mob Roster for each area.

## Audit loop: three rounds, every finding fixed

**Round 1:**
- **Wrong monsters:** GetLevelMTypes' `currlevel == 16/18/19/20/24` blocks ran on set levels. A set level's id is in `currlevel`, so the desert floors got Hive and Crypt kinds, and the woods' Underroot got Diablo's.
- **Cornerstone:** it could complete on set level 21.
- **Weather leaks:** woods lightning over the desert; woods rain ended by a short desert visit; desert heat ignoring its options; woods puddles on the sand.
- **Placement and speed:** oasis and sand hole on the road; the sand hole's maths per pixel past the map edge; road-end clearing splitting footprints.
- **Robustness:** all-or-nothing art loading.
- **Gate:** one row deep.

**Round 2:**
- **Palette:** the caves' tint covered the global palette half.
- **Traps and disarm:** they used the set level's id. Now `TrapFloor()`, the floor's own rung.
- **Melee:** monsters' minimum melee hit chance on set levels 14-16.
- **Spellbooks:** their drop level used the set level's id.
- **Na-Krul:** the lever and book checks are now guarded.

**Round 3:** the remaining trap paths (arrow blasts, Ring of Fire, Inferno, a dead caster's bolt).

**Lesson:** set level ids 11-24 collide with currlevel-keyed vanilla rules. Guard such rules with `!setlevel`, or use the floor's rung (TrapFloor, ShrineFloor).

## Tests (oracool_audit_test)
- **DesertFloorsAreWalkableAndHoldTheirSights:** four floors × five seeds.
- **DesertRosterHasFourKindsOnEveryRung:** also no Hive or Crypt kinds on a desert load, the three bosses, and the Cornerstone not on set level 21.
- **SandstormsBelongToTheDesert.**
- **OracoolPreview.DISABLED_DesertFloors:** maps and isometric views of each floor.
- **Waypoint acts:** updated for nine Orcl rows.

## Not yet seen in play
The weather, the caves' tint, the dust devils and the vultures. They are drawn by the game only, and the previews do not run them.

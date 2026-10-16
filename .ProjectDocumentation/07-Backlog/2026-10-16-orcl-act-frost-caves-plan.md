# Orcl Act: the Frost Caves (plan, 2026-10-16)

User, 2026-10-16: "plan a Frost Caves area - 4 100x100 underground levels in D2 Act 5 frost zones style. When we are done with the desert
we tackle this area. We just need to decide where on Tristram's map we should place a proper Frost Caves entrance worthy of the name.
Each level to have it's own 80x80 underground D2-Frozen-River-like floor."

Status: **planned, not started**. The desert comes first. Entrance place **decided 2026-10-16: A, the east edge** (section 6).

## 1. The shape
The third Orcl Act area, after the woods (south gate) and the desert (north-east gate). It is all underground, with eight levels:

| # | Main floor, 100 x 100 | Its side floor, 80 x 80 (the D2 Frozen River kind) |
|---|---|---|
| 1 | **The Rime Gate**: the cave mouth's first halls, icicles, snow blown in from Tristram | **The Frozen Stream** |
| 2 | **The Glacial Hollows**: wide caverns, frozen lakes, crystal | **Black Ice Run** |
| 3 | **The Frostbound Deep**: narrower, darker, the frozen dead in the walls | **The Drowned Hall** |
| 4 | **The Throne of Winter**: the area's boss, in an ice cathedral | **The Still River**: optional, the area's treasure floor |

- **Ways in and out:** each main floor leads on to the next. Each side floor is a dead end off its main floor, entered by a hole or a stair in the ice, as the desert's caves are.
- **Waypoints:** four, one per main floor (the desert's run to 32; these would take 33-36, `MaxWaypointSlots` permitting).
- The names are placeholders; rename freely.

## 2. Engine: the one real obstacle
- **The level limit:** `NUMLEVELS` is 25 and the set levels already reach id 24 (`static_assert(SL_LAST < NUMLEVELS)` in setmaps.cpp). Eight more levels need NUMLEVELS raised to at least 33. I'd raise it to 40 for headroom.
- **What depends on it:** 23 sites, among them the per-level seed and save tables and the level-visited flags. V1 always starts a new game ([[project_v1_always_new_game_no_continue]]), and we don't protect save formats ([[feedback_dont_protect_save_formats]]). So it is a mechanical change, done and tested on its own before any frost level.
- **Trap and roster rules:** ids 25+ lie past Hell/Hive/Crypt's floor numbers, so the old `currlevel` clash (the desert's lesson) does not recur. Every currlevel rule keeps its `!setlevel` / `TrapFloor()` guard all the same.

## 3. Engine: two kinds of floor

### The 100 x 100 main floors: the woods engine, underground
They use the woods' painted renderer (DTYPE_WOODS, its own ground, props and the cached Mixed floor), in a third mode, `OrclArea::Frost`:
- **Generator:** a new one for caverns, not the woods' open field. Cellular-automata caves on the 100 x 100 field, joined into one walk:
  - wide halls and narrow throats;
  - a frozen lake or two (ice you can walk on, with a sheen; black water at its holes);
  - ice bridges over crevasses;
  - the cave walls built from solid "blocker" props.
- **Light:** underground, so no sky, no day or night, and no outdoor all-round light. A cold blue-grey ambient, the hero's torch, and blue glows from ice crystals. Braziers and frozen campfires go warm.
- **Drawing:** the painted cave walls stand as tall props along every wall edge (as the desert's rock walls), with see-through near the hero as the woods' canopies have.

### The 80 x 80 side floors: the vanilla Caves generator, frozen
D1's Caves generator builds exactly 40 x 40 mega-tiles, which is **80 x 80 tiles**. So the side floors run on it as the desert's caves do:
- **Tint:** an ice tint on the palette's level half (32-127), as `TintDesertCavePalette` does.
- **The river:** the Caves' lava rivers repainted as **frozen water**: pale blue-white ice with dark cracks, and a slow shimmer in place of the lava's colour cycle. This is the Frozen River look.
- **Lava damage:** off on these floors; the river is ice.

## 4. The D2 Act 5 look (what the art briefs will ask for)
| Group | What it covers |
|---|---|
| **Ground** | packed snow, blue glacier ice, rime-crusted rock, frozen gravel, ice over dark water, a trodden path, and a scatter sheet (ice shards, frost crystals, snow clumps, frozen bones, a dropped axe) |
| **Big blockers** (cave walls and pillars) | ice-rock wall pieces, ice pillars, stalagmites and stalactite stumps, crystal clusters (blue, lit by the game), a frozen waterfall, a giant frozen beast in the ice |
| **Mid props** | frozen barbarians in ice, torn war banners, broken siege gear and catapult parts, snowed-over crates and barrels, ice-locked chests, frozen campfires, a sled, bones and skulls under frost |
| **Ways in** | Tristram's cave mouth (section 6); per floor, a way on; per side floor, its hole or stair in the ice |
| **The Throne of Winter** | an ice cathedral's pillars, a frozen throne, the boss arena's ring |

Before the first brief, a catalogue like the desert's (`Frost Asset Catalogue.md`, about 80 assets, with a D2 Act 5 mapping section) and the brief order.

## 5. Weather and life (game-drawn, each an option)
- snow sifting down through cracks and the cave mouth's first halls;
- the hero's breath fogging; a faint frost creeping in at the screen's edges, stronger on deeper floors;
- ice glints on the lakes; blue light from the crystals pulsing slowly;
- a rare cave-in of snow (dust only, no damage);
- drifting cold mist along the floors (the desert's drifting sand fog, re-coloured);
- colour_light: a cold ambient, the hero's torch warmer by contrast.
- The rain and fireflies stay off, as in the desert.

## 6. The open decision: the entrance in Tristram
The woods' gate is on the south edge (56-59, 94-95) and the desert's on the north edge, east side (72-75, 11-12). The Frost Caves want a **real cave mouth**, not a gate in the edge: a painted rock face with an ice-rimmed opening and icicles, frost spreading over the town's ground round it for a few tiles, cold mist breathing out of it, a little snow in the air near it, a pale blue glow inside. It would come in as one painted entrance (an RfA, like the sand holes) laid into the town plus game-drawn frost.

Candidate places, from the town's walk map:
- **A. The east edge, mid-north (about x 82-85, y 27-37):** open meadow reaching the edge. It balances the south (woods) and north-east (desert) gates, and is far from the Hive.
- **B. The west side, past the river (about x 10-14, y 45-60):** the wilder bank. It reads as "out of town", but the ground there is broken by trees, so the mouth's footprint needs clearing.
- **C. Behind the cathedral, the north-west (about x 12-20, y 10-14):** "under the church's hill", but it is crowded beside the cathedral and the graveyard (Hellfire's Crypt).

**Decided (user, 2026-10-16): A, the east edge.** It is open, visible from the town square's road, and keeps each area on its own side of town. The exact tiles get fixed once the mouth's painting and footprint are known.

## 7. Order of work (after the desert)
1. Raise NUMLEVELS and add the eight set levels, the `OrclArea::Frost` plumbing, the waypoints, and a test.
2. The cavern generator, its walk tests and previews; a placeholder look on the woods' art.
3. The side floors on the Caves generator: the tint, the frozen river and its shimmer.
4. The catalogue and the first RfAs: ground, then blockers.
5. The Tristram entrance (its RfA plus the frost and mist round it).
6. The roster (a Frost tab on the Designer's Mob Roster), the bosses (the woods' rule), weather, then the audit loop.

## 8. Monsters (decided 2026-10-16)
- **No new monster art.** The roster is the existing kinds, picked on a Frost tab of the Designer's Mob Roster.
- **Their frost look is drawn by the game:** ramp recolours and retints toward ice (the 32-bit `FrozenRgbTable` look is the starting point), and game-drawn touches such as a frost breath, rime glints and a cold trail.

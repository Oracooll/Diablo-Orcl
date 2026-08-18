# alvl, mlvl, ilvl

**Version:** 1.8.0
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

The three numbers Diablo II keeps separate, introduced here as three numbers.

## 1 — a level-1 spell band

Tiers for skills and spells are now **1, 6, 12, 18, 24, 30** - the same six the class trees use. Band
1 holds Firebolt, Healing, Charged Bolt, Holy Bolt, Heal Other, Telekinesis and Mana, so a Sorcerer
who finds a book on Cathedral 1 can read it. Everything else shifted down one step accordingly
(sBookLvl 1-2 -> 1, 3-4 -> 6, 5-6 -> 12, 7-9 -> 18, 10-14 -> 24, 15+ -> 30). The Spells sheet groups
and sorts by band, so the new band sorts itself.

## 2 — the ladder

`oracool/area_level`: **alvl = floor + 24 x difficulty**, one clean run from 1 to 96.

| Difficulty | Cathedral | Catacombs | Caves | Hell | Nest | Crypt |
|---|---|---|---|---|---|---|
| Normal | 1-4 | 5-8 | 9-12 | 13-16 | 17-20 | 21-24 |
| Nightmare | 25-28 | 29-32 | 33-36 | 37-40 | 41-44 | 45-48 |
| Hell | 49-52 | 53-56 | 57-60 | **61-64** | 65-68 | 69-72 |
| Torment | 73-76 | 77-80 | 81-84 | 85-88 | 89-92 | 93-96 |

`ItemsGetCurrlevel()` now returns this. That one edit moves every non-monster source - chests, floor
spawns, shop stock, quest rewards, gold piles - onto the ladder at once, and fixes two things with
it: difficulty never counted before (a Torment chest held Cathedral loot), and Hellfire's fold
mapped Nest back to 9-12 and Crypt to 14-17, correct when those were a parallel path and wrong here
where they are floors 17-24.

## 3 — mlvl, split from Monster::level()

`ItemLevelOfMonster()` = area level, +3 for a unique, +2 for a champion. `Monster::level()` is
untouched and still drives to-hit, block, experience and missile damage from its thirteen call sites
(your call: "split now, revisit with telemetry").

## 4 — ilvl, stamped and stored

`Item::_iOracoolItemLevel`, a byte of its own in the item-extension record (format version 5). NOT
`_iCreateInfo`, whose `CF_LEVEL` is six bits and would saturate at 63 - a third of the way up Hell.

`SetupAllItems` takes the ilvl as an explicit parameter rather than deriving it, because `lvl`
carries two inherited conventions - monster drops pass the monster level, floor items pass twice the
depth - and guessing which a caller meant would put a wrong number on every chest item.

## 5 — books stop dropping out of band

`GetBookSpell` now refuses any spell whose band is deeper than the ilvl:

| Band | Book needs ilvl | Earliest |
|---|---|---|
| 1 | 1 | Normal Cathedral |
| 6 | 6 | Normal Catacombs |
| 12 | 18 | Normal Nest |
| 18 | 30 | Nightmare Catacombs |
| 24 | 42 | Nightmare Nest |
| 30 | **52** | **Hell Cathedral** |

Apocalypse is band 30. It cannot exist before Hell, from a monster, a chest or Adria.

## 6 — all three visible

- **ilvl** - "Item Level: 62" in the item description, under the quality line.
- **mlvl** - bottom-right of the monster health bar. The bottom-left is left to the resistance icons,
  which are the "specific buffs" the request reserves that corner for.
- **alvl** - "Area Level: 62" on the automap, under the difficulty line, quest lairs included.

## Files

- `Source/oracool/area_level.h` / `.cpp` (new), `Source/CMakeLists.txt`
- `Source/oracool/spell_ranks.h` / `.cpp` - band 1, and `SpellBookItemLevel`
- `Source/items.h` / `.cpp` - the ilvl field, `ItemLevelOfMonster`, `ItemsGetCurrlevel`, the book
  gate, the description line
- `Source/loadsave.cpp` - item format version 5
- `Source/qol/monhealthbar.cpp`, `Source/automap.cpp` - the two readouts
- `test/oracool_audit_test.cpp` - the rank gate retargeted at band 1

## Not done here, deliberately

The **base-item qlvl regrouping** (your point 6) is the same edit as the four base tiers (your point
7), so both are in the proposal awaiting approval rather than done twice. Until then bases keep their
existing `iMinMLvl` ladder of 1-51, which the new ilvl values read correctly - the gate works, the
spacing is just not yet re-authored.

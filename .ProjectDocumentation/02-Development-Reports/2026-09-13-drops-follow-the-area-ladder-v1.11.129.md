# Drops follow the area ladder — the 96-place loot audit

2026-09-13 — v1.11.129

## Why

> "rare items still seem very rare and i am now in hell/hell. i thought they will be more common. make an artifact with
> drop chances of all item tiers. i wanna review it."
>
> "make sure drops reflect our vision we described in the wiki - progressive difficulty with more rewarding and better
> loot. double check and audit code behind it throughout all 96 levels."

## The vision, as the wiki states it

- **Mechanics.** mlvl is "the area level, +3 for a unique, +2 for a champion". ilvl is "the mlvl of whatever dropped
  it, or the alvl of the chest, floor or shop".
- **Areas.** The area level "decides what a place can drop and how good the rolls may be". Hell/Hell (alvl 48) is
  where everything is droppable, and Torment exists "for better rolls and deeper tiers".
- **Base tiers.** Bands open at ilvl 13, 25 and 37. Hell vendors stock at 38–48 and reach Torment tier.

## How it was measured

`SimulateMonsterDropOdds` (items.cpp, exported for tests) runs SpawnItem's own sequence a chosen number of times and
tallies the outcome by tier:

1. `RndItemForMonsterLevel` (or RndUItem's pool for a unique monster);
2. `SetupAllItems`.

`OracoolAudit.DISABLED_DropOddsReport` runs it for all 24 floors on each of the four difficulties, with the monster
types that actually spawn on each floor:

- **before:** the item rolled and stamped at `monster.data().level`;
- **after:** rolled and stamped at the loot level;
- a champion and a unique monster, after only.

It is a measurement, so it is disabled and run by hand:

```
oracool_audit_test.exe --gtest_also_run_disabled_tests --gtest_filter=*DropOddsReport*
```

The results are published as the **Orcl Drop Ladder** artifact.

## Findings

### Fixed

1. **Monster drops ignored the area ladder.** `SpawnItem` chose the base from `ItemLevelOfMonster`, but rolled
   quality and stamped the ilvl at `monster.data().level`, the monster type's authored level. Difficulty and depth
   therefore changed which bases could drop but not how good they were.
   - Hell/Hell paid rares at Normal's Hell-floor rate, measured at 0.57% a kill on both.
   - The Nest and Crypt paid their authored 22–30 instead of the Caves' and Hell's rungs.
   - The comment on `Monster::level()` claimed its difficulty offset "drives item level/quality". It did not; the
     comment is corrected.

   The drop is now rolled and stamped at `ItemLevelOfMonster`, which is also what `ReforgeOracoolItem` was written
   to expect ("mLevel IS the ilvl").
2. **Worn-tier gear stopped at ilvl 30.** `TrySpawnOracoolSetItem` clamped its level to 30 "to keep
   IsDungeonItemValid satisfied on the loopback". In single-player that check never runs, because `IsPItemValid`
   returns before it, and the hook is single-player only. Now the loot level.
3. **Griswold's Rare shelf stopped at ilvl 30.** `CreateRareVendorItem` left the ilvl to fall back to its clamped
   roll level, so it could never offer a Torment-tier base. It now stamps `VendorItemLevel(lvl)`, the vendor level
   lifted by the difficulty block, as the Basic and Premium shelves do. The 1..30 roll clamp (affix depth) stays.

### Verified as designed

- Chests, floor spawns, weapon racks, quest rewards and books: roll `2 × alvl`, stamp `alvl`.
- Gold: amount from the area level and difficulty.
- Named set pieces, socketables and signets: gated by the loot level, with rates from the area's treasure class,
  scaled +15/+30/+50% by difficulty.
- Base tiers and quality bands open at ilvl 13, 25 and 37.
- The Basic and Premium vendor shelves stamp the lifted vendor level.
- The Nest and Crypt sit on the Caves' and Hell's rungs.

### Worth knowing, not changed

- The rare, buffed-unique and primal bands top out at ilvl 37, so Torment does not raise those chances. It raises
  how often gear gets a quality roll (GetItemBLevel), the affix depth and the base tier, which matches "Torment
  exists for better rolls".
- Uniques: vanilla's 2% window (16% for a unique monster), the 1-in-10 cut from v1.11.125, and once per game each.

## Results

Rare drops per ordinary kill, measured over 24,000 kills per place with each floor's own monster types and the
user's diablo.ini (Rare 6, Buffed 3, Primal 1):

| Floor 16 | Before | After | One rare every (after) |
|---|---|---|---|
| Normal (alvl 16) | 0.67% | 0.21% | ~470 kills |
| Nightmare (alvl 32) | 0.75% | 0.76% | ~130 kills |
| Hell (alvl 48) | 0.71% | 1.45% | ~70 kills |
| Torment (alvl 64) | 0.67% | 1.71% | ~60 kills |

**Before, every difficulty paid the same.** The rate rose only with the monster types' authored levels.

**After, it climbs with the ladder:**

- **Floor 1:** 0.05% on Normal, 0.28% on Nightmare, 0.68% on Hell, 1.45% on Torment.
- **Buffed uniques and primals** climb the same way: Hell floor 16 buffed 0.04% → 0.14%, Torment floor 8 primal
  0% → 0.03%.
- **The Nest and Crypt** now pay the Caves' and Hell's rungs. On Hell floor 20 that is 0.28% → 1.35%.

**Normal's deeper floors pay less than before.** Their monsters were authored at 20–30, above the area level
(13–16) those floors sit on. That is the ladder working as the wiki describes it: the generosity sits in the harder
difficulties, not in Normal's last floors. It is still worth the user's eye.

The sample counts are small at the top tiers (a handful of primals and uniques per place), so those two columns are
indicative rather than precise. The full table and chart are in the **Orcl Drop Ladder** artifact.

## Tests

- The existing suite, for regressions.
- `DISABLED_DropOddsReport`, the measurement above.

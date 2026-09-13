# No affix above the item's level

2026-09-13 — v1.11.130

## Why

The user saw Weird (+81–100% to hit) and Strange (+101–150%) on low-level items and asked whether they were
invented. They are vanilla rows (item levels 35 and 60) that vanilla drops almost never reached. Asked whether
guaranteed affix picks should be capped at the item's level:

> "Why? I never made such instructions. all items including oracool invented ones should abide the ilvl-affix level
> corelation."

## What was wrong

An item may roll affixes whose level lies between half its roll level and its roll level. Two things broke that.

### 1. The guaranteed picks lifted both limits

`GetTieredItemAffixes` draws a Rare's 2, a Buffed Unique's 4 and a Primal's 6 guaranteed affixes with the level
window switched off. `DrawUnifiedAffix`'s `ignoreLevelLimits` dropped the floor **and** the ceiling, and so did the
older `GetItemPowerPrefixAndSuffix`. It was switched on by:

- every guaranteed pick;
- a Primal's perfect roll (all six picks);
- a Magic Find rare upgrade;
- the forced-tier paths: Griswold's Rare shelf, Adria's rare staves, Retier, and the debug `give*` commands;
- Griswold's Premium INI option.

The intent recorded in the comments was to keep a small pool at low level from starving a count guarantee. Lifting
the ceiling to do that was an implementation choice, never an instruction, and it put level-60 affixes (Strange,
Merciless, Godly, Hydra's, "of the whale", "of slaughter", "of thunder") on floor-1 items. On a Primal they rolled at
maximum.

### 2. Chest and Wirt items rolled above their displayed level

Chests, barrels, weapon racks and floor items roll at 2 × the area level but stamp the area level as their ilvl.
Wirt rolls between his level and twice it. So even a plain magic item from a chest could carry affixes up to twice
the item level its tooltip shows.

## Fix

`DrawUnifiedAffix`, the one draw every quality and source uses (vanilla rows and OracoolPoolRows alike), now
enforces:

- **The ceiling** = the roll's `maxlvl`, lowered to the item's stamped `_iOracoolItemLevel` when it has one. No row
  above it is ever eligible, whatever the caller asks.
- **The floor** = `min(minlvl, ceiling / 2)`, so a lowered ceiling does not leave an empty window.
- **`ignoreLevelLimits`** now relaxes **only the floor**, for the count guarantees and the Premium option.
- **The Oracool pool rows' fallback** ("the gentlest band") is taken only if that band is within the ceiling.
- **`GetItemPowerPrefixAndSuffix`** likewise always keeps `maxlvl`.

The Premium option's description and INI comment now say what it does: below the shelf's usual window, never above
the item's level.

### Fresh generation only, for the ilvl lowering

The first build applied the ilvl lowering everywhere, and four tests failed: `PackTest.UnPackItem_diablo`,
`_hellfire`, the shuffled pack run, and `Writehero.pfile_write_hero` (its hero's Strength came back 79 instead of 97).

All of them rebuild items from stored seeds through `RecreateItem`. A changed window changes both the eligible pool
and the random draw inside it, so the same seed produced a different item. That is the multiplayer pack and the
hero-select preview, never a single-player load, which reads the full record.

The lowering is therefore skipped while `ReplayingStoredItemSeed` is set, which `RecreateItem` sets. A rebuild
reaches exactly the window the item was generated with. The maxlvl ceiling and the floor-only `ignoreLevelLimits`
hold on both paths.

## Effect

- A floor-1 Rare can no longer roll Strange.
- A level-60 affix appears only on an item of level 60 or more. Since v1.11.129 that means:
  - monster drops on Torment floors 12–16 and the Crypt;
  - chests from Torment floor 12 down.

  Chests used to reach that from Nightmare floor 14, via their doubled roll level.
- Chest and Wirt items roll a narrower, lower window than before: at most their shown item level.
- Items already found keep the affixes they rolled.

## Tests

- **New:** `PrimalItemTest.NoAffixOnATieredItemIsAboveTheItemsLevel`. It creates a Primal and a Rare stamped at
  ilvl 13 with a 30..60 roll window (a chest's shape), 200 trials each. Every stored affix's value must be allowed
  by a row at or below level 13, and the Rare must still reach its 2-affix guarantee.

  Its first run flagged armour penetration rolling 6 and 12 against row parameters of 1–3. Those were legitimate:
  outside Hellfire `SaveItemPower` widens armour penetration to `1 << param1 .. 3 << param2` before rolling, so
  Piercing (level 1) rolls up to 6 and Puncturing (level 9) up to 12. The test now applies the same widening.
- **Changed:** `GetPrimalItemAffixes_AlwaysProducesExactlyThreePrefixesAndThreeSuffixesInNarrowLevelWindow` now uses
  window 13..13 instead of 1..1. At level 1 a weapon has exactly one beneficial prefix row (Bronze), so its three
  prefixes could only have come from above the item's level. 13 is the lowest level a Primal can roll at all.
- The jewellery count-guarantee tests for Rare and Buffed Unique still hold at 1..1.

## For the user to look at

New low-level rares, buffed uniques and primals carry only affixes suited to their level. Weird, Strange, Merciless,
Godly and the like turn up only on deep items (level 35 or 60 and above).

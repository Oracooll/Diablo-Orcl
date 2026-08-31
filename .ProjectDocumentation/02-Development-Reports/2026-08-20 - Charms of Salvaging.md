# Charms of Salvaging

**Version:** 1.8.75
**Date:** 2026-08-20

> "add in the game and in adria/griswold stores charms 1slot charms of salvaging. 7 types for each
> type of salvagable items. when i put them in inv everytime i pick an item it automatically turns
> into salvaged material instead of a regular item. save inv slots and time."

Seven new 1-slot charms, one per salvage tier. With one in your backpack, every drop of that tier is
converted to its material **at pickup**, before it ever occupies a cell.

## What shipped

| Charm | Converts | Yield | Drops from | Vendor price |
|---|---|---|---|---|
| Charm of Salvaging: Whites | plain gear | 1 White Scale | qlvl 1 | 4,000 |
| Charm of Salvaging: Magic | magic gear | 1 Magic Powder | qlvl 4 | 8,000 |
| Charm of Salvaging: Rare | rares | 2 Rare Fibres | qlvl 8 | 16,000 |
| Charm of Salvaging: Uniques | uniques | 3 Unique Encrustments | qlvl 13 | 26,000 |
| Charm of Salvaging: Primal | primals | 4 Primal Vines | qlvl 20 | 40,000 |
| Charm of Salvaging: Set | set items | 3 Set Engravings | qlvl 15 | 30,000 |
| Charm of Salvaging: Ethereal | ethereals | 2 Ethereal Imbueities | qlvl 17 | 34,000 |

The yields are `SalvageYield`'s existing `TierYield` table, unchanged - a charm gives exactly what
Levski's button would have given, so the charm buys convenience, not value.

## The three rules that make this safe

**1. The active cap applies.** `IsOracoolCharmIdx` now includes salvage charms, so they flow through
the same `ForEachActiveCharm` walk as the stat charms: **the first three charms in reading order are
live and everything after them is dead weight.** Arming a fourth tier means retiring one. Without
that cap, a backpack full of charms would silently eat every drop in the game.

**2. A full pack declines rather than destroys.** The materials are placed *before* the item is
consumed. If nothing fits, the conversion is refused outright and the item is picked up normally.

**3. It is never silent.** You never see the item, so the event log names it:
`Salvaged on pickup: Godly Plate of the Whale -> 4 Primal Vines`. That matters most for the tiers you
might regret - a Primal charm eats a Primal you would have kept, and the log is the only trace.

## Where the code went

- `tools/GenSalvageMaterials.ps1` - the tier table gained `Button`, `CharmValue` and `CharmMinLvl`
  columns and a `New-Charm` drawing routine. One walk still emits ids, table rows, ICURS ids, frame
  sizes and icon specs for **both** families. The charm frames are collected in **separate baskets
  and appended after all material frames** - a charm spec interleaved between two material specs
  would have shifted every id after it, because a CEL stores frames in file order and nothing else
  ties a frame to an id. That exact bug was written once and caught by inspection before shipping.
- `Source/itemdat.{h,cpp}` - three new `.inc` includes, `IDI_LAST` and `ICURS_ORACOOL_LAST` moved to
  the last charm, and the new `IsOracoolSalvageCharmIdx`.
- `Source/oracool/salvage.{h,cpp}` - `TierCharm` table, `SalvageCharmFor`, `SalvageTierOfCharm`,
  `SalvageCharmEffectLine`, and `TrySalvageOnPickup`.
- `Source/inv.cpp` - the hook, in **both** pickup paths. `AutoGetItem` is the QoL pickup-radius path;
  `InvGetItem` is what runs when the item is on the exact tile you are standing on. A hook in only
  one of them works most of the time, which is the worst way for it to fail.
- `Source/items.cpp` - `StockSalvageCharms` for Griswold and Adria; the charm **drop** walk widened
  from `IDI_ORACOOL_CHARM_GREED` to `IDI_LAST`; `givecharms` widened the same way.
- `Source/oracool/charms.cpp` - the item popup line.
- `Source/oracool/crafting.cpp` - recipe 2 ("two charms of any kind") now **excludes** salvage
  charms. It produces a random stat charm, and letting a 40,000 gold Primal charm be consumed for a
  Charm of Vigor is a trap, not a recipe.

## Two boundary bugs the change surfaced

Both were old bounds that would have quietly excluded the new family:

- the charm **drop** walk stopped at `IDI_ORACOOL_CHARM_GREED`, so every charm appended after GREED
  would have been unobtainable as loot;
- `DebugSpawnCharms` had the same bound, so `givecharms` would have "worked" while testing nothing.

Both now run to `IDI_LAST` and filter with `IsOracoolCharmIdx`, which cannot be outgrown by
appending.

## Vendors

Both Griswold and Adria carry all seven, rather than splitting them - a utility item you have to
remember the vendor for is a worse utility item. They are **ordinary restocking stock**, not pinned
like Adria's potions: buying one removes it, and the next restock brings it back. Pinning would have
meant widening the witch's hardcoded three-slot pinned block, which the random Hellfire book slots
sit immediately behind. Each vendor's random stock count is capped seven slots below capacity so the
charm slots are always available.

## Verification

- Icon CEL: **455 -> 462 frames**, materials first then charms. The frame count is the check that
  catches the ordering bug.
- Build: clean.
- Tests: **487/489**, the two standing baseline failures
  (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).
  `Writehero.pfile_write_hero` passes - nothing here touches the save format.
- MPQ repacked (395 files).

## To test in game

1. `givecharms` - should now yield the seven Charms of Salvaging alongside the stat charms.
2. Put one in the backpack among fewer than three charms; kill something of that tier; the drop
   should never appear as an item, and the log should name what it became.
3. Put four charms in and confirm the fourth's popup says it is inactive, and that its tier is *not*
   converted.
4. Fill the backpack, then walk over a matching drop - it should be picked up as an item, not eaten.
5. Griswold and Adria should both list all seven.

## Not done

The charm art is a bevelled tablet in the tier colour - deliberately not an orb, so a charm and its
material are distinguishable at a glance, but still placeholder like the material orbs.

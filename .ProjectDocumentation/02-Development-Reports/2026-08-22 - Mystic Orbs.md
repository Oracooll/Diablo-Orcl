# Mystic Orbs

**Version:** v1.9.18 -> v1.9.19
**Date:** 2026-08-22
**Phase:** D2MXL-to-ORCL Phase 1 (see `07-Backlog/Plan - D2MXL to ORCL.md`)

## What shipped

Eight consumables that add one **fixed small stat** to an item, permanently, capped at **six per
item**. Drop one onto a backpack item and it is absorbed.

| Orb | Grants | From |
|---|---|---|
| Might / Grace / Insight / Vigour | +3 to a stat | qlvl 4 |
| Orb of Fury | +2 damage | qlvl 10 |
| Orb of Warding | +5 all resistances | qlvl 12 |
| Orb of Fortune | +5% magic find | qlvl 16 |
| Orb of Avarice | +8% gold find | qlvl 16 |

The four attribute orbs open early because a new character can use them at once; find and
resistance wait for the depth where those are a concern.

## The three properties that make it a mechanism

1. **The cap is per ITEM, not per orb type.** Six into one weapon finishes it and the seventh has to
   go somewhere else. That is what makes an orb a decision rather than an accumulator, and it is why
   `_iOracoolOrbCount` counts orbs rather than tracking which kinds were used. The test builds an
   item out of *mixed* orbs specifically to prove that mixing buys no extra room — that is one `++`
   away from being wrong and nothing else would notice.
2. **The value is fixed, never rolled.** `param1 == param2` on every row, asserted, because
   `SaveItemPower` rolls a range between them and a rolled orb is an affix wearing another name.
3. **It is permanent.** Levski's Roar already sells gambling — the whole reroll ladder — and orbs
   are deliberately the opposite of it.

## Values kept small on purpose

An orb is worth roughly a third of an affix. Six should be a real upgrade to a good base and never a
substitute for finding a better item, because the moment orbing beats finding, the drop tables stop
mattering and every zone's treasure class becomes decoration.

## What it cost, and what it bought alongside

**`OracoolItemFormatVersion` 8 -> 9.** This is the first per-item value in the fork that is not
derived from something: the base tier, the ethereal roll and every affix come out of the item's own
seed, and a monster variant or a boss trait comes out of the monster's. A count of orbs is a
**player decision**, and a decision has nowhere to be recomputed from.

Since the bump was being paid anyway, it also carries **`IPL_MAGICFIND`** and `Item::_iPLMagicFind`
— the exact gap `IPL_GOLDFIND` filled at v1.9.5, one axis over. `ItemBonusTotals` has had a
`magicFind` figure since Phase 1 and the drop tail consumes it, but the **only** contributor was a
charm: there was no field for `SaveItemPower` to write into, so no item, affix, set rung or unique
could grant magic find at all. Orb of Fortune needed one that could.

`IsOracoolAffixTypeValid`'s bound moved with it, as it did last time. Forgetting that is how a new
power loads back as `IPL_INVALID` on every existing item, silently, and only after a round trip.

## Riding seams instead of building new ones

- **Applying** rides `inv.cpp`'s gem paste path. That already resolves "an unequippable item dropped
  onto a backpack item", so orbs inherit the target resolution, the backpack-only rule (orbing worn
  gear means carrying it first — the same beat of friction socketing has), and **no new window**.
- **The stat** goes through `ApplyItemPower`, the public door onto `SaveItemPower` that the fifteen
  item sets already use. That is what guarantees an orb's +3 Strength is the identical +3 Strength
  an affix grants, in the same field with the same sign.
- **Dropping** is a fifth family in the socketable draw rather than a hook of its own, so which zone
  favours orbs is a table entry beside the other four. Orbs take a slice from every zone rather than
  owning one — an orb is useful everywhere and to everyone, so a zone that was *the* orb zone would
  be the only zone anyone farmed.

## The bug this uncovered, which was four versions old

Adding two fields to the item record made the loadsave round-trip test **hang**. The cause was not
the new fields:

`OracoolItemExtensionSaveSize` is a hand-maintained sum that **sizes the save buffer** — it is not
documentation, it is what `SaveHeroItems` passes to its `SaveHelper`. Version 8 appended
`_iPLGoldFind`'s four bytes at v1.9.5 and **this sum was never updated**. Every hero save written
since has gone into a buffer four bytes per item too small.

It did not fail then because `SaveHelper::WriteBytes` **silently returned** when the buffer was full:

```cpp
if (!IsValid(len))
    return;   // the tail of the record is simply not written
```

Four bytes of slack absorbed it. Nine did not, and the loader then read past the end of a short file
and spun.

Two fixes, and the second matters more than the first:

1. The sum is now spelled one term per line with the version that added each, because a sum of bare
   numbers is a sum nobody re-derives when they add a field.
2. **`SaveHelper` no longer truncates silently.** A dropped write sets a flag and the destructor
   `app_fatal`s naming the file. An overrun is never intentional — every caller declares a size it
   computed from what it is about to write — and a save that quietly lost bytes is worse than one
   that refused, because the player keeps playing and the damage surfaces as a corrupt character
   much later.

## And one of my own, of the same shape

`OracoolAudit.TreasureClassesGiveEveryZoneSomethingOfItsOwn` counted family shares into `int
counts[4]`. Orbs made a fifth family, `FamilyForRoll` returned index 4, and the loop wrote one past
the end of a stack array — so the test did not fail, it **hung**. Sized from the enum now, with a
bounds assertion inside the loop.

That is the same trap as the socketable drop's `candidates[MaxRuneLadder]`, found one file over a
week ago: an array sized for one family and written by several.

Retuning also had a knock-on. Orbs take a slice of every zone, which diluted every majority, and the
Nest's charm share fell to 34% — just under the 35% the test uses to separate a treasure class from
a tilt. The **table** was retuned rather than the threshold, because that threshold is the definition
of the feature and a table that has to argue it down is a table with no identity.

## Two fixture traps worth recording

Both made a test look like it had proved something when it had not:

- **`_iStatFlag`** is "the wearer meets this item's requirements" and is set by `CalcPlrInv`, not by
  `InitializeItem`. `ItemBonusTotals::AddItem` returns on its first line without it — so the first
  run reported a magic find of zero and read as a failure of the orb rather than of the fixture.
- The save round trip is **not reachable** from a unit test: `SaveItem`/`LoadItemData` are file-local
  to loadsave.cpp and `PackItem` is the network path, which carries no extension record. That is
  covered by `Writehero.pfile_write_hero`, whose golden output moves exactly once at this bump.

## What to look at in game

The paste interaction and the description lines are not verifiable from tests.

- Kill things until an orb drops; its description should name what it grants and say to drop it onto
  a backpack item.
- Do that, and check the target gains the stat and gains a `Mystic Orbs: 1 / 6` line.
- Fill one to six and confirm the seventh is refused.

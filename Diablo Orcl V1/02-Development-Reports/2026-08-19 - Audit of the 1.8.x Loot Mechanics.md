# Audit of the 1.8.x Loot Mechanics

**Version:** 1.8.4
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

A re-audit of everything introduced across 1.8.0 - 1.8.3: the area ladder, mlvl, ilvl, the book band
gate, the four base tiers, banded qlvl, depth-weighted quality, and the replay seam.

Six findings. Two fixed here, one is a correction to a claim I made in the 1.8.0 report, and three
are decisions that need the user.

## F1 - CORRECTION: shops were never on the ladder

The 1.8.0 report said `ItemsGetCurrlevel()` moved "chests, floor spawns, shop stock and quest
rewards" onto the area ladder. **Shop stock was wrong.** No store path calls that function. Vendor
level is computed in `SetupTownStores`:

```cpp
l = clamp(deepestFloorVisited + 2, 6, 16);
SpawnSmith(l); SpawnWitch(l); SpawnHealer(l); ...
```

So vendor stock ignores difficulty entirely - Adria in Torment offers the same 6-16 range as Adria in
Normal - and always did. Nothing broke; the claim was simply false.

## F2 - shop items carry no ilvl and no base tier

Every store path (`SpawnSmith`, `SpawnWitch`, `SpawnHealer`, `SpawnBoy`, `SpawnOnePremium`) builds
items with `GetItemAttrs` + `GetItemBonus` directly, never through `SetupAllItems` - which is where
ilvl is stamped and the base tier applied. Consequence: nothing bought from a vendor shows an Item
Level line or a Tier line, and every purchasable base is Normal tier forever.

Not a bug in the new code - it is machinery the stores never routed through. Fixing it means giving
the store paths an ilvl (from F1's vendor level, or from a difficulty-aware one) and calling into the
tier application. **Needs a decision: should vendors sell tiered gear at all?**

## F3 - the book gate is approximate in shops

`GetBookSpell` prefers the item's stamped ilvl and falls back to `lvl * 2` when there is none. Shop
books have none (F2), so their effective ilvl is twice the vendor level - at most 32.

The reported bug stays fixed: Apocalypse needs ilvl 52 and cannot appear. But band-18 books (qlvl 30)
become buyable in Normal once the vendor level reaches 15. They still cannot be READ until character
level 18, so the ladder holds; the purchase just runs ahead of it.

## F4 - FIXED: a lost short-circuit

The three quality rolls read:

```cpp
GenerateRnd(1000) < QualityChancePerMille(...)
```

Both operands are evaluated, so the draw happened even when the configured chance was 0 - where the
old code's `*option > 0 &&` short-circuited before it. Setting a tier chance to 0 in the INI
therefore advanced the random stream where it previously did not. Now the chance is computed first
and the draw is guarded by `> 0`.

## F5 - FIXED: an area level for a place with no drops

The automap printed "Area Level: 1" (or 25/49/73) in **town**. The ladder does answer for town, but
town has no monsters, chests or drops, so the number was about nothing. Suppressed there.

## F6 - VERIFIED BENIGN: the CF_LEVEL clamp

`_iCreateInfo` is clamped to 63 because `CF_LEVEL` is six bits and the ladder reaches 96. That makes
a recreated item's level wrong above 63 - but single-player never keeps a recreated item: both
`pfile_read_player_from_save` and the character-select preview run `UnPackPlayer` and then overwrite
every item with `LoadHeroItems`, which reads the full saved stats. The clamped recreate is transient.
Multiplayer would notice; V1 is single-player.

Related and also benign: a Torment-tier item's value (x30) can exceed `MaxVendorValue`, which the
multiplayer wire check `IsShopPriceValid` would reject - but per F2 vendors never generate tiered
items, so nothing reaches that path today.

## Files

- `Source/items.cpp` - the three guarded rolls
- `Source/automap.cpp` - no area line in town

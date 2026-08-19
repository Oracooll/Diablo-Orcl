# Tiered Vendor Stock

**Version:** 1.8.5
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

Closes F2 from the audit: vendors now stamp an ilvl and, on a chance, stock gear above Normal tier.

## Vendor item level

Vendor level has always been `clamp(deepest floor visited + 2, 6, 16)` and has never known which
difficulty the game is on. `oracool::VendorItemLevel()` lifts it by the difficulty block:

| Difficulty | Vendor ilvl | Tier band |
|---|---|---|
| Normal | 6-16 | Normal |
| Nightmare | 30-40 | Nightmare |
| Hell | 54-64 | Hell |
| Torment | 78-88 | Torment |

One tier band per difficulty, matching the dungeon ladder.

## The chance

`ApplyVendorTier` stamps the ilvl on every stocked item, then rolls whether that item is tiered at
all - **35% by default**, INI-tunable as `Vendor Tiered Stock Chance` (0-100). If it is, the tier
comes from the same weighting the dungeon uses at that ilvl.

A chance rather than the dungeon's flat weighting because a shop is a repeatable source: if every
Torment shelf were Torment-tier, the dungeon would stop being where gear comes from.

The roll hashes the item's seed with a different salt than `TierForItem`, so "is it tiered" and
"which tier" are independent questions rather than the same bits read twice.

## Where it is applied

The five fresh-stock paths: `SpawnSmith`, `SpawnWitch`, `SpawnHealer`, `SpawnBoy`, `SpawnOnePremium`.
Placed INSIDE each vendor's retry loop where one exists, so a tiered item that prices itself past the
vendor cap is simply rolled again - the cap keeps its meaning instead of being quietly exceeded.

NOT applied in the five `Recreate*` twins. Those rebuild an item from a stored seed for the
multiplayer pack, which has no room to carry a tier; single-player never keeps a recreated item,
since the save holds every stat. Same rule the dungeon path follows, and the pack tests hold it.

## One more name bug, found on the way

`ApplyBaseTier` was still prepending the tier word to the item name - the 1.8.1 removal had not
actually applied. On a vendor item it produced **"Lightning Jagged Maul"**: the tier word wedged
between the affix and the noun, because vendors build the affix name before the tier runs. Removed
properly this time, with the reasoning recorded so it does not come back a third time.

## Files

- `Source/oracool/item_tiers.h` / `.cpp` - `VendorItemLevel`, `ApplyVendorTier`, the name fix
- `Source/items.cpp` - five spawn paths
- `Source/options.h` / `.cpp` - `Vendor Tiered Stock Chance`

## To look at in game

Buy from Griswold in Normal, then in Nightmare: about a third of the shelf should carry a blue Tier
line, and every item should show an Item Level. Adria's books are unaffected - they are pinned stock,
and books take no tier.

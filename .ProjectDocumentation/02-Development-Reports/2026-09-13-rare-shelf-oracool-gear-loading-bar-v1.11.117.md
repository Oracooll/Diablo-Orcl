# Oracool gear on Griswold's Rare tab; the loading bar in the item-quality colours

2026-09-13 — v1.11.117

## 1. The Rare tab never offered Oracool gear

> "something is wrong with RARE store in Griswold. None of oracool items make it there. check what items
> are allowed to appear there and fix it - make all item types that appear in basic and magic stores also
> make it to the rare store."

### What each tab draws from

| Tab | Vanilla bases | Oracool gear |
|---|---|---|
| Basic | `SpawnSmith` → `RndSmithItem` (droppable pool, `SmithItemOk`) | `StockOracoolVendorItems` — a third of the shelf |
| Magic | `SpawnOnePremium` → `RndPremiumItem` (droppable pool, `PremiumItemOk`) | `StockOracoolMagicItems` — a fifth |
| Rare | `CreateRareVendorItem` → `RndPremiumItem` | **none** |

### Cause

Every vendor rolls its vanilla bases through `GetItemIndexForDroppableItem`, and that pool excludes every
Oracool item on purpose: it is the save format (a town item's index is re-derived by replaying its seed
through it). The Basic and Magic tabs therefore reach the fork's gear through a second pass over
`OracoolGearBasesFor`. The Rare shelf (v1.9, rebuilt 2026-09-12 onto the premium pool) never got that
pass — so its base could only ever be vanilla. The premium pool is a superset of Basic's vanilla bases, so
Oracool gear was the only missing part of "everything Basic and Magic sell".

### Fix

`CreateRareVendorItem` now aims a third of its rolls at an Oracool gear base (`RndOracoolGearBase`, a thin
wrapper over `OracoolGearBasesFor`, the pool both other tabs already share) before forcing the Rare tier.

- **A third**, the share Basic stocks.
- **Depth-gated by the character's level** (the larger of it and the vendor level), as the Magic tab gates
  its Oracool block by `premiumlevel` — the vendor level alone is capped at 16, which would hide the deep
  tiers from a level-40 character.
- **Replay safety does not constrain this pick**: the curated shelves are never saved (built once per game
  in memory), and a bought item keeps its full record like any other.
- Single-player only, like every Oracool stocking pass.

### Test

`OracoolRareShelf.TheRarePoolReachesTheKindsTheOldCeilingLockedOut` now also requires Oracool gear to make
up more than a tenth of 4000 sampled rare rolls, besides the existing kind-breadth checks.

## 2. The loading bar in the item-quality colours

> "make the loading bar gradient between basic,magic,rare,unique, set, primal items colours."

`interfac.cpp` `BarGradient`: six evenly spaced stops, in the user's order — basic white, magic blue, rare
yellow (YL-3, 0xFEFB24), unique gold, set green (0x8CBE8C), primal (BE-2, 0xE8CACA). It was dark red to
bright green. Rare, set and primal are the exact top shades of their font bands; white, blue and gold come
from vanilla .trn files with no RGB, so those three stops are the nearest on-screen values.

The gradient still spans the whole track and is revealed as the bar grows, so the colour at the leading
edge is how far the load has got.

## For the user to look at

- Griswold's Rare tab (new game, or a fresh shelf): shoulders, bracers, gloves, belts, greaves, boots and the
  other Oracool bases among the rares.
- Any level transition: the bar runs white → blue → yellow → gold → green → primal.

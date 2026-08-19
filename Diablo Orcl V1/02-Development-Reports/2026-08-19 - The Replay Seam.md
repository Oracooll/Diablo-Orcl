# The Replay Seam

**Version:** 1.8.3
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

Closes the gap 1.8.2 left open: the banded qlvl ladder now governs **every** drop pool - chests,
floor spawns, vendors and monsters alike.

## The problem it solves

The shared droppable pool is part of the save format. `UnPackItem` rebuilds a dungeon item's index by
replaying its seed through the pool walk, and `RecreateTownItem` replays the five vendor pools the
same way. Band the filter unconditionally and the same seed rebuilds a different item - which is why
twenty net-pack validation tests failed in 1.8.2 when I first tried, and why that version shipped
with chests and vendors left on the authored ladder.

## The seam

One file-local flag in items.cpp, `ReplayingStoredItemSeed`, set by an RAII guard at `RecreateItem`'s
entry - which covers the dungeon path, all five vendor paths through `RecreateTownItem`, and anything
either calls. Every pool filter now asks one function:

```cpp
int PoolQlvl(const ItemData &item)
{
    return ReplayingStoredItemSeed ? item.iMinMLvl : oracool::BandedQlvl(item.iMinMLvl);
}
```

Authored values during a replay, banded values during fresh generation. Old seeds rebuild exactly as
they did; new drops use the ladder.

A flag rather than a parameter because the filters are lambdas handed to a shared walk, four layers
below the two functions that know which case this is. Routing every gate through one accessor is what
stops the two answers drifting apart by a call site being forgotten.

## What changed in play

Chests, floor spawns and shop stock now open their bases on the same 1-60 ladder as monster drops -
so a Hell chest offers Hell-grade bases, and a Normal one cannot. That was the last piece of the
alvl/mlvl/ilvl work.

## Files

- `Source/items.cpp` - the flag, the guard, `PoolQlvl`, and five filters routed through it
- `Source/oracool/item_tiers.h` - the seam documented where the banding is defined

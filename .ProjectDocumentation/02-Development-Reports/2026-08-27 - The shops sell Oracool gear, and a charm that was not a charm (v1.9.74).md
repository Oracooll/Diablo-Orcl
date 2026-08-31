# The shops sell Oracool gear, and a charm that was not a charm (v1.9.74)

**Date:** 2026-08-27
**Version:** 1.9.73 → 1.9.74
**Tests:** 564, of which 562 pass — the two standing baseline failures, unchanged.

---

## Why the shops were all vanilla

> "add to the list - all shops to also offer all of the new items we have introduced. i now only see
> vanilla items."

Structural, not an oversight. Every vendor rolls its stock through `GetItemIndexForDroppableItem`,
and that walk **refuses every Oracool item on purpose**:

> the set items are droppable but NOT through this pool — because this pool is part of the save
> format. `UnPackItem` recreates a dungeon item's INDEX by replaying its seed through this exact
> walk, so growing the list re-routes every seeded recreation: the first attempt put them here and
> `pack_test` watched a Jade Great Helm come back as Jade Leggings.

So the obvious fix — add them to the pool — would silently re-identify **every item already bought
and saved**. It was not done.

`StockOracoolVendorItems` is a hook beside the pool, the same shape `TrySpawnOracoolSetItem` uses for
drops. Griswold's Basic shelf now gives about a third of its slots to Oracool gear, depth-gated by
the same banded qlvl ladder the drop hook reads, so a low-level Griswold offers leather and a
high-level one offers spectral — by data, not by a table.

## The stamp, and the bug it uncovered

There is one difference from the drop hook, and it turned out to matter more than the feature.

`CF_TOWN` is the **only** route in `RecreateItem` that ignores the packed index and re-derives it
from the seed. So a vendor-sold Oracool item wearing `CF_SMITH` would be replaced on the next load.
The new stock is stamped as a rolled dungeon item instead, which keeps the packed index and replays
the affixes.

Checking that led straight to a bug that was **already live**:

**`StockSalvageCharms` stamped `lvl | CF_SMITH`.** A Charm of Salvaging bought from Griswold was
therefore re-derived from its seed on the next load — through a pool that excludes charms — and came
back as some vanilla item. Bought, paid for, saved, and quietly replaced.

Now stamped `0`, which sends `RecreateItem` down its `icreateinfo == 0` branch and rebuilds from the
index that was actually stored. A salvage charm has no affixes and no level scaling; its identity is
the whole item.

### Demonstrated, not argued

`PackTest.OracoolItemsSurviveAPackRoundTrip` packs an Oracool item and unpacks it. With the fix it
round-trips. With `CF_SMITH` restored:

```
Which is: 139        Which is: 316
Which is: "Spiked Club"        Which is: "Charm of Vigor"
```

A Charm of Vigor comes back as a Spiked Club. That is the bug, exactly, in one line of test output.

**Anyone who bought a salvage charm from Griswold in an earlier build has already lost it** — it
became something else on the next load, and there is nothing here that can recover it. This stops it
happening again.

---

## Still queued — five items

The Rare tab; the Set shop; the hammer-cursor Repair rework; Refresh on Basic/Rare/Supplies; and
docking every limestone window to the bottom of the screen.

Adria and Pepin do **not** yet stock Oracool goods — this instalment is Griswold's Basic shelf only,
because the vendors each roll their stock differently and the stamp question had to be settled first.
Extending it is now a small change per vendor rather than a design one.

# Banded qlvl and Depth-Weighted Quality

**Version:** 1.8.2
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

The two tuning tables that the 1.8.0 plumbing was built for.

## 1 — banded qlvl

`oracool::BandedQlvl()` maps the authored `iMinMLvl` ladder (1-51, written against monster levels that
ignored difficulty) onto the area ladder's 1-60, in seventeen groups:

| authored | banded | | authored | banded |
|---|---|---|---|---|
| 1-3 | 1 | | 27-30 | 37 |
| 4-6 | 5 | | 31-33 | 41 |
| 7-9 | 9 | | 34-36 | 45 |
| 10-12 | 13 | | 37-39 | 49 |
| 13-15 | 17 | | 40-42 | 52 |
| 16-18 | 21 | | 43-45 | 55 |
| 19-21 | 25 | | 46-48 | 57 |
| 22-24 | 29 | | 49-51 | **60** |
| 25-27 | 33 | | 99 | 99 (sentinel) |

Groups, not a rescale: bases land ON band edges, so a depth unlocks a shelf rather than one sword.
The last base opens at 60 - Hell/Hell, which was the design target.

## 2 — quality weighted by depth

The INI keeps the master knob; the band shapes it. At default settings (rare 20, buffed unique 10,
primal 5):

| ilvl band | Rare | Buffed Unique | Primal |
|---|---|---|---|
| 1-24 | 2.0% | 1.0% | 0% |
| 25-48 | 6.0% | 2.5% | 0.25% |
| 49-72 | 14.0% | 5.0% | 0.75% |
| 73-96 | 22.0% | 8.0% | 2.0% |

Per mille rather than percent, because the early primal chance is a fraction of one percent and would
round away to never. Primal starts at zero on purpose: a perfect item found at level 3 flattens the
ladder in front of the character who found it.

Basic-vs-magic needs no table - vanilla's `GetItemBLevel` already rolls against the level, so deeper
items come out magic more often on their own.

## The constraint I hit, and where it leaves us

The banding is applied to the **monster drop pool**, the **set and gem spawns**, and **Adria's book
stock**. It is NOT applied to `RndUItem`, `RndAllItems`, `RndTypeItems` or `RndVendorItem` - the
chest, floor and vendor pools.

Those four filter the shared droppable pool, and that pool is **replayed during item recreation**:
`RecreateItem` and `UnPackItem` re-run the same filtered pick from a stored seed to rebuild an item.
Changing the filter means the same seed produces a different item, and twenty net-pack validation
tests failed at once when I tried it - correctly. That is a real invariant, not a fixture being
precious.

So chests, floor spawns and shops still use the authored 1-51 ladder. In play that means their
availability curve is vanilla's while monsters follow the new bands. Closing it needs a seam through
`GetItemIndexForDroppableItem` so recreation asks for authored values while fresh generation asks for
banded ones - a day's careful work with the pack tests as the guard, not a bigger table.

## Files

- `Source/oracool/item_tiers.h` / `.cpp` - `BandedQlvl`, `QualityChancePerMille`
- `Source/items.cpp` - five gates rebanded, three quality rolls reweighted

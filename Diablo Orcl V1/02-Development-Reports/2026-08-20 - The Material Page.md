---
date: 2026-08-20
version: 1.8.70
tags: [stash, sockets, runes, gems]
---

# The Material Page

SORT now pulls every rune and gem out of the ordinary sort and lays them out on a page of their own.

## The layout

**Runes across the top**, El to Zod, left to right: ladder position `p` goes to column `p % 10`,
row `p / 10`. Three full rows of ten and a fourth of three - exactly the 33 the ladder holds, which
is asserted rather than assumed.

**Gems from the bottom up**, one column per type, quality counting down from the floor of the grid:
row = `StashGridRows - GemQualityCount + quality`. Perfect lands on row 15, the last one, and
Chipped four rows above it. Seven columns, five rows.

The two blocks cannot collide - a `static_assert` says so, rather than the reader having to add
4 and 11 in their head.

## Why fixed placement needed its own function

`AutoPlaceItemInStash` finds the first hole that fits, which is precisely what a fixed layout must
not do. `PlaceMaterialAt` writes the cell directly. Every material is 1x1, so there is no footprint
to solve.

## The page is chosen, not assumed

"The first one unoccupied by items" is taken literally: the non-materials are re-packed **first**,
by the same first-fit scan as before, and only then is the first wholly-empty page searched for. So
the material page falls out of how much other stuff you own rather than being pinned to page 1.

If every page is occupied, the materials go through the ordinary scan instead. Losing them to a
tidy layout is not a trade worth making.

## One box per kind, and what happens when that is not enough

The layout works because runes and gems **already stack** - `isStackableConsumable` has covered both
since 2026-08-16, capped at 99. So 33 rune boxes and 35 gem boxes hold any realistic quantity.

A kind that overflows past 99 into a second box goes in the band **between** the two blocks, filled
left to right. It does not shove the grid: a 34th rune box is not worth losing the alignment over. A
per-cell `taken[][]` flag is what detects the second stack - without it the duplicate would overwrite
the first and a Zod would simply vanish.

## Verification

487 tests, the two standing baseline failures only.

`OracoolAudit.SortMovesRunesAndGemsToTheirOwnPageInFixedPositions` deposits runes and gems out of
order with an ordinary item among them, sorts, and checks El at the top-left, Zod at the end of the
fourth row, a Perfect Ruby on the grid's last row, a Chipped Amethyst four rows above it, and that
no material was left behind on page 0.

Every expected cell is **derived the way the code derives it** - from the ladder position and from
(type, quality) - not transcribed from what one run produced. A test that only recorded the output
would agree with the layout drifting.

Worth a look in game anyway, since no test renders the page: hit SORT with a mixed stash and see
whether the two blocks read the way they were meant to.

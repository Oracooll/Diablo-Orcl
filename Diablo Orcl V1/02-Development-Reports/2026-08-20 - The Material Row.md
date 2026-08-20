---
date: 2026-08-20
version: 1.8.73
tags: [stash, salvage, sockets]
---

# The Material Row

The last piece of the salvage request: SORT now gathers the seven materials onto their own row of
the material page, between the runes and the gems.

## Row 6, and why not a computed midpoint

The band between the blocks is rows 4-10. Row 6 leaves rows 4-5 free **directly under the runes**,
which is where rune overflow lands - so the commonest overflow case never has to step over the
material row to find space. A computed midpoint would have put the row at 7 and left the same amount
of room in a less useful place.

Both bounds are `static_assert`ed rather than trusted: the row must sit between the two blocks, and
the seven must fit across one row of ten.

## Left to right is white to dark grey, without a second table

The column is `IDidx - IDI_ORACOOL_SALVAGE_WHITE_SCALES` - the item's own index, offset. That works
because `GenSalvageMaterials.ps1` emits the seven in the order the user listed **both** the colours
and the salvage buttons, so enum order already IS white to dark grey.

The alternative was an explicit ordering array in the sort, which is a second copy of a sequence
that already exists and can therefore drift from it. There is now exactly one place that decides
what order the materials go in, and it is the generator.

## Verification

489 tests, the two standing baseline failures only.

`OracoolAudit.SortGivesSalvageMaterialsTheirOwnRow` deposits White Scales and Ethereal Imbueities
along with a rune, a gem and an ordinary item, sorts, and asserts the two materials share one row,
that white is leftmost and dark grey rightmost, and that the row sits below the runes and above the
gems. Positions are derived the way the code derives them, so the test cannot agree with the order
drifting.

Worth a look in game with a real haul, since nothing in the suite renders the page.

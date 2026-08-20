# The store crash, and four audit rounds

**Versions:** 1.8.94 - 1.8.96
**Date:** 2026-08-20

The user reported a crash while scrolling and buying in Griswold's consumables, with screenshots
showing rows named "Gold", "Club" and "Ring of Truth" - none of which is in that stock.

## What the garbage rows proved

Those items live in the statics **adjacent to** `witchitem`. So the rows were an array overrun
displaying whatever follows the array. That is the same defect as the crash, seen on the occasions
it does not fault: the fixed-array path shows neighbouring memory, the vector path faults.

## The three defects behind it

**1. The walk stalled on an empty slot.** `ScrollWitchBuy` advanced `idx` only when it DREW a row:

```cpp
if (!item.isEmpty()) { ...draw...; idx++; }
```

One empty slot and every remaining row re-read the same entry, drew nothing, and everything past it
became unreachable however far you scrolled. The witch path is where it bites, because it walks the
raw `witchitem` array rather than the filtered vector, and that array grows holes the moment
anything is bought from it.

**2. `ItemMiscIdIdx` had no end test at all.** It walked `AllItemsList` to the first match and ran
straight off the table for any misc id no droppable row carries. Vanilla got away with a short fixed
table where every id had a match; this fork has appended ~250 rows and edits `iRnd` - Town Portal is
set never to drop - so neither half of that excuse survives.

**3. `GetTranslatedItemName` indexed `AllItemsList[IDidx]` unchecked.** Every name in the game comes
through it, so a stale index turned a bad item into an out-of-bounds **read on the render path** - a
crash from merely looking at a list, which is the worst kind because it destroys the evidence.

## The audit rounds

Then a pass as a third party, hunting the **classes** those defects belong to rather than re-reading
the fix.

### Round 1 - the stalled-index shape, elsewhere

`ScrollSmithBuy` and `ScrollHealerBuy` are copies of the same function and carried **both** defects:
index advancing only on a drawn row, and an unbounded array read. Griswold's basic list and Pepin's
list are now bounded and advance unconditionally.

`ScrollSmithSell`, `ScrollSmithPremiumBuy` and `ScrollSmithUniqueBuy` were checked and are correct -
they advance outside the conditional or in the loop header.

### Round 2 - the unbounded-walk shape, elsewhere

`SpawnUnique` walked `AllItemsList` for the base carrying a unique's `UIItemId`, no end test. **This
one has a live way to miss**: the fork added 143 expansion uniques and a large share are still
waiting on base items to exist, so spawning one of those read off the end and built an item from
whatever followed.

Bounded, and the lookup **moved above `AllocateItem`** so failing it cannot leak the slot or leave a
cleared item on a tile `dItem` already points at. It logs and declines rather than substituting
another base: an item under a name that does not describe it is worse than a drop that visibly did
not appear.

### Round 3 - things checked and found already correct

- The scrollbar's division by `storenumh - 1` is guarded by `if (storenumh > 1)`.
- `CheckPlrSpell` is the **only** `MyPlayer`-derived default argument in the codebase, and its
  remaining default-arg callers are the right-button and controller paths, where the readied spell
  IS what they mean. (Its one wrong caller was fixed in v1.8.93.)
- `OperateShrineEnchanted`'s `do/while` over known spells is guarded by `cnt > 1`, so it terminates.

### Round 4 - the picker against its own opener

Clicking the well that owns an open picker closed it here and let the well's handler open it
straight back up on the same click. The popup could never be shut by its own button, and every
attempt silently reset the scroll. The owning well now toggles and consumes the click; the OTHER
well still falls through, so switching buttons stays one click.

## Verification

Build clean at every step. **489/491**, the two standing baseline failures. No asset touched, so the
396-file MPQ still stands.

## Still unverified

The consumables crash itself is fixed by reasoning, not by reproduction - it needs a play session on
1.8.96 to confirm. If garbage rows survive, the remaining suspect is the mapping in `WitchBuyEnter`
rather than the draw.

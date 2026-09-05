# The Stash Stacks (v1.9.278)

**Date:** 2026-09-05 · **Request:** "too many items in the stash dont seem to stack. analize." → "do it and make sure they will stack when i hit sort." → "find appropriate spot on consumables dedicated stash tab to put them there on sorting."

## What was wrong

Not item level: `canStackWith` compares the misc id and the spell for potions and scrolls and the base index for materials, nothing else. The stash simply never merged:

1. **Paste** treated an occupied cell as a swap - the held potion went in, the stack came out on the cursor.
2. **Auto-place** (ctrl-click deposit) scanned for the first free cells and took one every time; its own comment said "no merge on deposit".
3. **SORT** merged only runes, gems, jewels and salvage on their page; potions and scrolls went through the plain sort split.

## What changed (qol/stash.cpp)

- Paste merges first: a stackable dropped on a compatible stack joins it up to 99 (the backpack's rule), with the remainder staying on the cursor; only then does the swap apply.
- Auto-place merges first: a stackable joins compatible stacks with room on ANY page, and only what they cannot absorb goes to the free-cell scan.
- SORT merges every stackable kind through one `MergeStacks` before it packs anything, then lays the consumables out on their own page: the next empty page after the materials', potions and elixirs in the belt's own order by misc id, then scrolls by spell, one cell per stack from the top-left. Overflow falls to first-fit.

## Tests

`StashAutoPlaceReachesEveryRowOfThePage` placed a hundred potions and expected a hundred cells; under the new rule that is one stack of 99 and a single, so it places rings now, which is what it was testing anyway. New `StashMergesStackablesOnDepositAndSort` pins the 99+1 merge on deposit and the potions leaving page 0 for their own page on SORT. 626/627 with the standing `Drlg_l1` failure.

## v1.9.279: on the materials page, and the Hellfire runes

"i hit sort but potions and scrolls moved to a brand new tab. i wanted you to find a unallocated slot on the runes/gems dedicated tab and we keep all of them there. also - why aren't you stacking the hellfire runes?"

- SORT now puts the consumables into the FREE cells of the materials page - the rune, salvage, jewel and gem blocks leave rows and columns unallotted, and the consumables take those in row order, potions first, then scrolls, then the runes and oils. Only when there is no materials page do they take the next empty one. The materials page index is remembered from the materials pass rather than looked up again, so the two cannot land on different pages.
- Hellfire's trap runes (Rune of Fire / Lightning / Nova / Stone) and the oils are stackable consumables now: `isStackableConsumable` admits the two misc-id ranges, the kind test keeps different runes and different oils apart, and a use already goes through the decrement-or-remove path a scroll uses. `IsStackableConsumable_ExcludesRunesAndEquipment` became `_IncludesRunesExcludesEquipment`.

626/627 with the standing `Drlg_l1` failure.

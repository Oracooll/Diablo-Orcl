---
title: 2026-08-12 - Worn Items Destroyed by the Diablo Save Remap
date: 2026-08-12
tags: [dev-report]
summary: All six new-type items vanished on New Game. The save-format id remap classed everything above id 160 as "Hellfire exclusive, does not exist in Diablo" and wrote each one out as the empty-slot marker - the items were destroyed at save time, not on load. Both remap directions now pass the range through, with a round-trip test.
---

# Worn Items Destroyed by the Diablo Save Remap

User report, with a before/after screenshot pair: fully dressed thirteen-slot paperdoll, then New Game - and only the vanilla seven remain. All six new-type items gone.

## The mechanism

V1's New Game reloads the character through the compact hero pack: `PackItem` on save, `UnPackItem` on load. In a Diablo (non-Hellfire) game, `PackItem` first translates every item id into the *Diablo save numbering* via `RemapItemIdxToDiablo` - and that function contains:

```cpp
if ((i >= 83 && i <= 86) || i == 92 || i >= 161) {
    return -1; // Hellfire exclusive items
}
```

The six worn types live at ids 168-173, squarely inside the `i >= 161` band. So each one mapped to -1, which `PackItem` writes as `0xFFFF` - **the empty-slot marker**. The items were destroyed at save time; New Game merely displayed the result. The vanilla seven survived because their ids sit below the band. `SaveItem` (the full in-game save path) runs the same remap, so it had the same hole.

This was the one piece of the id-appending plan that assumed instead of checked: appending past `IDI_ARENAPOT` kept every *existing* index stable, but landed the new ids inside a range the save format had already assigned a meaning to.

## Why identity mapping is safe

The fix must claim space in the Diablo save numbering. That numbering is provably sparse above 166: the Diablo format compresses all 168 Hellfire ids down to 0-155 (removing 4 oils, 1 scroll, and the 7 ids above 160), with 166 special-cased for the Sorcerer's starting staff. No legitimate vanilla save can contain a value in 168-173, so the six ids map to **themselves**, with an early-out in both directions ahead of every shift. The spawn (shareware) remap pair got the same guard - its numbering tops out around 140 by the same arithmetic.

## The test

`PackItem_diablo_roundtrip_preserves_oracool_worn_items` packs and unpacks one item of each new type in Diablo mode - exactly the path the user's character took - and asserts, per item, that the packed idx is not the empty-slot marker and the round-tripped item keeps its identity. Against the pre-fix code it fails at the first assert with `packed.idx == 0xFFFF`.

The golden save hash did NOT need re-baselining: the writehero fixture's six new slots are explicitly empty, and empty slots pack identically before and after.

## The user's items

The already-lost items are gone - they were overwritten in the hero file as empty slots before this fix existed. The give*set commands regenerate a full set in seconds, which is what they are for.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.17**. Tests **349/351** - the suite grew by one, same two pre-existing failures.

Play-test: dress all thirteen slots, exit to Main Menu (forcing the save), New Game, and confirm everything is still worn. Ideally twice, since the previous session's save now contains empty slots that will load as empty - only a save made on 1.1.17 proves the round trip.

## Related

- [[2026-08-12 - Boots Crash - the Hover Chain Nobody Taught]]
- [[2026-08-12 - Six New Equipment Slots]]

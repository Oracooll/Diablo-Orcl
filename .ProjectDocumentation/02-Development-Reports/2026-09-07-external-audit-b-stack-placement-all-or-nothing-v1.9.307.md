# External audit (ChatGPT, 2026-09-06), patch B: stack placement is all-or-nothing (v1.9.307)

**Date:** 2026-09-07. INV-01, the audit's first-priority finding, verified in the code before the change.

## The defect

`AutoPlaceItemInBelt`, `AutoPlaceItemInInventory` and `AutoPlaceItemInStash` merged an incoming stack into partial stacks FIRST, with `persistItem` true, and only then looked for a slot or cell for the remainder. When there was none they returned false with the stacks already topped up, and every caller keeps the source on a false return: the merged units existed twice. A second fault in the inventory path handed the extra-tab fallback the ORIGINAL item after part of it had merged, so a successful placement over-credited by the merged amount (95 + 20 became 99 + 20 instead of 99 + 16).

## The fix

Each public auto-place call is a transaction: a commit is preceded by a full probe (the same function with `persistItem` false, which already made no writes), and nothing is written unless all of it fits. Probe and commit walk the same slots in the same order, and merging does not change which slots are empty, so they agree. The extra-tab fallback receives the remainder.

## Tests

Backpack full with one 95-stack and room in an extra tab: 99 + 16, never 99 + 20. Every destination full: false and the stack still 95. The belt: false and unchanged with all real slots taken, then 99 + 16 once a slot frees. The stash: false and unchanged when full with four units of headroom, then 99 + 16 once a cell frees.

Suite 645/646, the standing dungeon-generation failure only.

# DROP-01: every fresh drop gets the drop tail (v1.9.309)

**Date:** 2026-09-07
**Decision (the user):** "all fresh drops get the drop tail, do DROP-01."

Monster drops ran the Phase 1 tail after setup - Magic/Gold Find, the socket roll, the ethereal roll, the noteworthy-drop log - and nothing else did: chests, sarcophagi, corpses, barrels, armour stands, weapon racks, bookcases, theme-room items and the Find Item cry all went through `SetupBaseItem` and skipped it. A rack's plate could never roll sockets; a chest's gold ignored Gold Find.

`FinalizeFreshDrop(item, level)` (items.h) is the one funnel, called from the monster path at the monster's level and from `SetupBaseItem` at the level the item was generated at (2 x the dungeon's item level, the same number its setup used). Still outside it, on purpose: seed replays, network recreations, uniques (fixed), vendor stock, crafting outputs and saved items - none is a fresh drop. The tail only touches equipment and gold, so quest books and potions through the same funnel are unchanged.

Test: the funnel applies Gold Find to a gold drop; the socket and ethereal stages keep their own tests. Suite 646/647, the standing dungeon-generation failure only.

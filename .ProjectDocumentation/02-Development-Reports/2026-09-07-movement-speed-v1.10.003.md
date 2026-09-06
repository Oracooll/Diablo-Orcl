# Movement Speed: a stat on the sheet, Vigor by rank, an item affix, a slow channel (v1.10.003)

**Date:** 2026-09-07
**Requests:** "introduce Movement Speed stats in hero stats window. i dont see my movement speed when i use Vigor. Also vigor should make it clear in the description how many % it increases movement with each level. We also need to introduce Movement Speed +X% affix on items so other classes have a chance at such abilities, not just the Paladin." Then: "movement speed stats to be listed in percentage. abilities and items increase it. curses and cold spells decrease it."

## The model

This engine has one speed knob: the walk animation's frame skip (StartWalkAnimation's -2 for a walk, 2 for the run). Each skipped frame is a tick off the ten-tick stride, so the ladder is 100 / 111 / 125 / 143 / 167% above a walk, and two steps below it (91 / 83%) before the animation stutters. `MovementSpeedPercent` (class_tree.cpp) is what the sheet shows: 100 plus the worn affixes and the burning Vigor, minus any slow, floored at 10. `WalkFrameSkipFor` maps it onto the ladder by threshold (80, 90, 110, 125, 140, 160) and StartWalkAnimation takes the larger of that and the binary run sources (Run In Town, the R toggle, the dash, the Barbarian's, Bard's and Monk's run rows), so nothing that ran before runs slower.

## Vigor

+15% per rank (`VigorMoveSpeedPerRank`), through the aura totals like every other aura, so the Abilities window's "Current Skill Level / Next Level" block names it and the sheet sums it. Five ranks reach the run, which is what rank 1 used to grant outright. The description says so.

## The item affix

`IPL_MOVESPEED`, "+X% movement speed". NOT a row in the vanilla prefix/suffix tables: those are what the seed replay walks, and a row added there re-rolled every seeded item in every save - the pack fixtures caught it at once (a Helm of harmony became a Great Helm of haste). It rolls on the drop tail instead (`TryAddMovementSpeedToDrop`, beside sockets and ethereal): one wearable drop in twelve - body armour, helms, rings, amulets, normal or magic quality - gets +10..30%, the floor rising with the item's level, into the item's own affix record. The record is persisted as records already are, the field is re-derived from it on load, and the tooltip prints it; the item format did not grow and no seeded stream moved.

## The slow channel

`SlowPlayer(player, ticks, percent)`, `PlayerSlowPercent`, `TickMovementSlow`, `ClearMovementSlows`: a per-player slow that the percentage subtracts, ticked from the class tree's per-player tick, cleared at new game. Overlapping slows keep the deeper and the longer, never add. Nothing in the engine slows a player yet - no monster cold attack, no curse targets a player - so the channel waits for its first caller; the sheet turns red the moment one exists.

## Tests

The percentage from a ring and from Vigor at three ranks, the thresholds, the slow side (depth, clock, overlap), the drop-tail roll (one in twelve within 400 tries, 10..30, record and field agree, never on a unique), and Vigor's ranks 1 and 5. Suite 685/685.

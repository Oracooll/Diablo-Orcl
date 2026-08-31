---
title: 2026-08-09 - Autosave Trigger Coverage Gaps
date: 2026-08-09
tags: [dev-report]
summary: Closed three confirmed autosave gaps (ground item drop, deposit to Stash, withdraw from Stash) and extended coverage to every other Stash content mutation, matching the user's "save on any significant or insignificant event" Diablo 3-style goal.
---

# Autosave Trigger Coverage Gaps

## Context

Following [[2026-08-09 - Remove Auto Save Item Delay]], the user manually tested autosave and reported four events that didn't produce a save: item drop, item transfer to Stash, item pick from Stash, and gaining a level. The user's stated goal: "it is crucial we make it work as in diablo 3. auto save at any significant and insignificant event."

## What changed

Grepped every `oracool::ScheduleAutoSave*` call site (see [[2026-08-09 - Autosave-Only Play, Part 1]] for the original trigger set: item pickup, equipment change, level change, store purchase, experience gain, stat point spent). Three of the four reported events had **zero** hook anywhere in the code - confirmed, not assumed:

- **Item drop** - [Source/controls/plrctrls.cpp:2022](../../../Source/controls/plrctrls.cpp) `TryDropItem()` had no schedule call at all. Added `oracool::ScheduleAutoSaveForItemDrop()` right after the drop succeeds.
- **Item transfer to Stash** - [Source/inv.cpp:1963](../../../Source/inv.cpp) `TransferItemToStash()` and `TryTransferHoveredActiveTabItemToStash()` had no schedule call. Added `oracool::ScheduleAutoSaveForStashChange()` to both.
- **Item pick from Stash** - [Source/qol/stash.cpp](../../../Source/qol/stash.cpp) had no autosave hook anywhere in the file. Added `ScheduleAutoSaveForStashChange()` to every content-mutating path: `CheckStashPaste()` (deposit, both the gold and item branches), `CheckStashCut()` (manual pickup to cursor), `TransferItemToInventory()` (Ctrl+click withdrawal), `WithdrawGold()`, `UseStashItem()` (reading/drinking straight from the Stash), and `SortStash()`.

New options added to `OracoolOptions` ([Source/options.h](../../../Source/options.h), [Source/options.cpp](../../../Source/options.cpp)) so these stay independently toggleable like every other autosave trigger:

- `Auto Save on Item Drop` (default on)
- `Auto Save on Stash Change` (default on) - covers every Stash mutation listed above under one switch, since they're all "the Stash's contents changed" from a save standpoint.

Both wired through the existing `ScheduleAfterSeconds(0)` instant-save path in [Source/oracool/auto_save.cpp](../../../Source/oracool/auto_save.cpp), same as every other trigger post-[[2026-08-09 - Remove Auto Save Item Delay]].

Bumped `ORACOOL_VERSION` to `1.0.34` and rebuilt the Debug config clean.

## Why

The fourth reported gap - **gaining a level** - was investigated but not changed. `AddPlrExperience()` in `player.cpp` already calls `oracool::ScheduleAutoSaveForExperienceGain()` on every experience increase, including the tick that crosses a level threshold, and `ProcessAutoSave()` runs once per game loop iteration after that call completes, so a level-up save should already be captured with the post-level-up `_pLevel` value. No code path was found that lets a level-up happen without going through `AddPlrExperience()`. This one needs a targeted retest in isolation (see Not done below) rather than a speculative code change.

The three fixed gaps share a root cause: `ScheduleAutoSaveForItemPickup()` and `ScheduleAutoSaveForEquipmentChange()` were the only hooks added when auto-save was first built, and item drop / Stash traffic simply weren't part of that first pass - not a logic bug, a coverage gap. Auditing `qol/stash.cpp` end-to-end (rather than adding one call and assuming the rest followed the same pattern) is what surfaced `WithdrawGold`, `UseStashItem`, and `SortStash` as additional silent gaps beyond the two the user explicitly named.

## Verification

Debug build completed with no errors (111/111 targets, `DiabloOrcl.exe` linked). Not yet manually retested in-game by the user.

## Not done / deliberately left alone

- **Level-gain autosave**: left as-is since the existing hook appears correct by inspection. Worth a targeted retest after this rebuild - if a level-up genuinely still doesn't save, check the Event Log's timestamped entries against what the player was doing at that exact moment (e.g. holding an item on cursor from the killing blow's drop, which would correctly defer the save per `IsSafeToSave()`'s `pcurs == CURSOR_HAND` gate - see [[2026-08-09 - Autosave-Only Play, Part 1]]).
- Did not audit *every* possible state-mutating action in the game (e.g. quest-flag changes, waypoint unlocks) for autosave coverage - scoped this pass to the four events the user actually tested and the Stash surface those pointed to.

## Related

- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[2026-08-09 - Remove Auto Save Item Delay]]

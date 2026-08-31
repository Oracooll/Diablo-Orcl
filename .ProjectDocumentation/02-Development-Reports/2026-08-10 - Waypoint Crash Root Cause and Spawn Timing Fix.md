---
title: 2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix
date: 2026-08-10
tags: [dev-report]
summary: Found the real cause of the level-1-waypoint crash - a missing graphics registration on dungeon revisits - and fixed a second, separate timing race in the spawn-reposition logic that was consuming its one-shot flag before the actual level transition had even started.
---

# Waypoint Crash Root Cause and Spawn Timing Fix

## Context

After [[2026-08-10 - Waypoint Spawn Crash - Move Reposition Off the Load Path]] shipped, the user retested and found it hadn't actually fixed anything - if anything, worse: spawn position was inconsistent between two otherwise-identical attempts (once stuck at the new-game spawn, once briefly visible there before snapping to the town waypoint), and the game crashed both times the moment they got close to the Level 1 waypoint sigil after landing near the stairs.

That last detail - a crash triggered specifically by proximity to the sigil, unrelated to any teleport - was the key clue that the previous fix had been chasing the wrong bug entirely.

## Root cause 1: the actual crash

Traced through the object graphics pipeline. `AddWaypointSigilObject()` registers the sigil's graphic (`OFILE_MCIRL`) via `EnsureObjectGraphicsLoaded()` - but, like every other level-content placement call, only runs on a *fresh* level generation. A revisited dungeon level restores its objects through `LoadLevel()` instead, which never calls `AddWaypointSigilObject()` at all.

`LoadLevel()` does call `SyncObjectAnim()` for every restored object, which is supposed to (re)link each object's `_oAnimData` to its graphic. It does this by searching `ObjFileList` for the object's graphic ID - and since `OFILE_MCIRL` was never registered for this revisit, the search fails. `SyncObjectAnim()` logs `"Unable to find object_graphic_id..."` and returns *without setting `_oAnimData`* - it's left in whatever empty/stale state it was already in. `DrawObject()` only reads `_oAnimData` once the object is close enough to actually render (gated by `LightTableIndex`), which is exactly why the crash was invisible from a distance and immediate up close: the object was never truly broken, just spriteless, and dereferencing that empty state is what actually crashed.

**Fix**: new `oracool::EnsureWaypointGraphicsLoaded()` ([Source/objects.cpp](../../../Source/objects.cpp), declared in [Source/oracool/oracool.h](../../../Source/oracool/oracool.h)), called unconditionally in [Source/diablo.cpp](../../../Source/diablo.cpp)'s `LoadGameLevel()` whenever `currlevel == 1`, before the fresh-vs-revisit fork - so the graphic is registered every time regardless of which branch runs afterward. (Town doesn't need this separately; `AddWaypointSigilObject()` already runs unconditionally there per [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]].)

## Root cause 2: the spawn-position inconsistency

The previous report moved the spawn-reposition check to run once per game tick, reasoning that "after the level fully loads" was safer than mid-load. That reasoning had a gap: `WaypointSpawnRequested` gets set the instant the player clicks a travel-list entry, but the actual level transition doesn't happen until a *later* tick processes the queued `WM_DIABNEXTLVL` message from the event queue. A per-tick check can't distinguish "warp requested" from "warp requested but not yet underway" - it fired on whatever level the player was still standing on, consuming the one-shot flag before the real destination had even started loading. This explains both observed outcomes: sometimes the flag got burned on the wrong level and nothing corrected the eventual spawn (stuck at the default position); other times timing happened to line up and it fired correctly, just late enough to be visible as a flash-then-teleport.

**Fix**: moved the call to the one place that's both synchronous and exactly-once: [Source/interfac.cpp](../../../Source/interfac.cpp)'s `WM_DIABNEXTLVL` handler (the only `interface_mode` any waypoint warp uses), immediately after its `LoadGameLevel()` call returns.

Bumped `ORACOOL_VERSION` to `1.0.48` and rebuilt the Debug config clean.

## Why two separate bugs looked like one

The crash and the spawn-position issue are unrelated - the crash would have happened from normal walking even without any teleport involved, and the spawn-position race would have misfired even if the graphics bug didn't exist. They surfaced in the same test session because both only manifest on a dungeon *revisit*, which the user's test sequence (find Level 1 waypoint → warp to town → warp back to Level 1) exercises directly.

## Verification

Debug build completed with no errors. Not yet manually retested - this is the third attempt at the waypoint-warp feature, so please retest the full sequence again: town waypoint → Level 1 waypoint (spawn position and no crash on approach), and Level 1 → town waypoint (spawn position), consistently across repeated attempts this time.

## Related

- [[2026-08-10 - Waypoint Spawn Crash - Move Reposition Off the Load Path]]
- [[2026-08-10 - Stale Waypoint Spawn Flag Bug]]
- [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]]

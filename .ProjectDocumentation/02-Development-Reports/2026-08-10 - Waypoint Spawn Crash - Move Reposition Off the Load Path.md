---
title: 2026-08-10 - Waypoint Spawn Crash - Move Reposition Off the Load Path
date: 2026-08-10
tags: [dev-report]
summary: The waypoint-spawn reposition added in the previous fix crashed when warping to a revisited dungeon level. Moved it out of the mid-level-load code path entirely, into a safe once-per-tick check alongside the existing autosave processing.
---

# Waypoint Spawn Crash - Move Reposition Off the Load Path

## Context

Immediately after [[2026-08-10 - Stale Waypoint Spawn Flag Bug]] shipped, the user reported a crash reproducing the exact scenario that fix targeted: warping from the town waypoint back to the (already-visited) Level 1 waypoint.

## Investigation

Found a Windows Error Reporting crash record for the session (`0xc0000005` access violation, `DiabloOrcl.exe`). Attempted to resolve the fault offset against the build's PDB using `llvm-symbolizer`/`llvm-pdbutil` (bundled with the Visual Studio LLVM toolset) to get an exact function/line, but the tooling failed to resolve symbols even for a known-good address (the module's own entry point) - a tooling incompatibility with this PDB, not informative about the actual crash. No memory dump survived to reopen (Windows only keeps the temp `.mdmp` briefly; only the `Report.wer` metadata remained).

Without a stack trace, re-examined the previous fix's design rather than guessing at a specific line. The reposition it added ran `ApplyPendingWaypointSpawn()` synchronously from inside `LoadGameLevel()` itself - for the dungeon-revisit case, immediately after `LoadLevel()` restores that level's saved object/monster/dungeon state. That is *mid-load*: several subsystems the reposition touches (`Objects[]`/`ActiveObjects`, the `dPlayer` collision grid, lighting, vision) are supposed to be fully settled by the time normal gameplay resumes, but nothing guarantees they're in a stable, finished state at that exact intermediate point in a still-in-progress load - unlike the fresh-generation case (where the object was *just* placed by the same function calling the reposition), the revisit case reaches this point via a completely different code path with different ordering guarantees.

## What changed

Moved the reposition off the load path entirely:

- **`Source/objects.cpp`**: `AddWaypointSigilObject()` no longer calls `ApplyPendingWaypointSpawn()` at the end.
- **`Source/diablo.cpp`**: removed the `oracool::ApplyPendingWaypointSpawn()` call that ran immediately after `LoadLevel()` in the dungeon-revisit branch. Added a new call, `oracool::ApplyPendingWaypointSpawn();`, to the main game loop instead - right next to the existing `oracool::ProcessAutoSave();` call, which already runs once per normal game tick, well after any level load has fully completed and normal per-tick simulation has resumed.
- **`Source/oracool/oracool.h`**: updated `ApplyPendingWaypointSpawn()`'s doc comment to describe the new, safer calling convention and why the mid-load version was replaced.

The function's own logic (search the current level's active objects for the waypoint sigil, relocate the player there) is unchanged - only *when* it runs changed.

Bumped `ORACOOL_VERSION` to `1.0.47` and rebuilt the Debug config clean.

## Why

`ProcessAutoSave()` already proves this exact calling point is safe for touching full player/game state every tick, across every kind of level transition, without incident throughout this whole session's testing. Piggybacking on the same call site means the reposition inherits that same safety margin instead of relying on `LoadGameLevel()`'s internal ordering being safe for an operation it was never designed to support mid-sequence.

## Verification

Debug build completed with no errors. Not yet manually retested in-game by the user - this is the second attempt at the same underlying feature, so please retest the exact repro (town waypoint - Level 1 waypoint) along with the original three symptoms from [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]].

## Related

- [[2026-08-10 - Stale Waypoint Spawn Flag Bug]]
- [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]]

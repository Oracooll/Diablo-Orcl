---
title: 2026-08-10 - Scoped Black Flash Fix to Dungeon Only
date: 2026-08-10
tags: [dev-report]
summary: Re-applied the black-flash fix, scoped to dungeon levels only. Town has no lighting baseline established by the time the reposition runs, unlike a dungeon level; forcing the recompute there is what caused the earlier corruption.
---

# Scoped Black Flash Fix to Dungeon Only

## Context

After [[2026-08-10 - Revert Lighting Force-Recompute - Corrupted Town]] reverted the black-flash fix entirely, the user confirmed everything else works and the flash itself is minor ("no big deal") but asked for it to be fixed if a safe way could be found.

## What changed

[Source/objects.cpp](../../../Source/objects.cpp): `ApplyPendingWaypointSpawn()` calls `ProcessLightList()`/`ProcessVisionList()` again, now gated on `currlevel != 0` - dungeon levels only.

Bumped `ORACOOL_VERSION` to `1.0.52` and rebuilt the Debug config clean.

## Why

Traced why dungeon and town behaved differently under the original, unscoped fix. `diablo.cpp`'s `LoadGameLevel()` gives every dungeon level a `dLight`/`dPreLight` baseline *before* this function ever runs: a fresh generation calls `SavePreLighting()`, a revisit restores `dLight` from that same saved snapshot via `LoadLevel()`. Town's own branch does neither - it has no equivalent baseline-establishing step at all before this point in the sequence. Forcing an early lighting recompute against dungeon's already-settled state works cleanly (confirmed by the user's own report: Level 1 warps looked correct throughout, only town corrupted). Doing the same against town's unestablished state is the most likely explanation for the corruption seen previously.

Scoping the fix to dungeon levels only keeps the working half of the original attempt and drops the part that broke - town keeps the brief black flash rather than risk the corruption recurring, since a real fix for town's case would need understanding what its own lighting-initialization sequence is actually missing, not just applying the same call earlier.

## Verification

Debug build completed with no errors. Not yet manually retested - please confirm the Level 1 flash is gone again and Tristram still renders correctly (should be unaffected, since this change doesn't touch the town path at all).

## Related

- [[2026-08-10 - Revert Lighting Force-Recompute - Corrupted Town]]
- [[2026-08-10 - Waypoint Spawn Black Flash Fix]]

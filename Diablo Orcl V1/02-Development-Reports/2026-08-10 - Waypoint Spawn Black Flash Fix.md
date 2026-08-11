---
title: 2026-08-10 - Waypoint Spawn Black Flash Fix
date: 2026-08-10
tags: [dev-report]
summary: Fixed a brief black flash when warping to a waypoint - the destination level's initial lighting was computed for the default spawn point, not the sigil, and only caught up on the next natural game tick.
---

# Waypoint Spawn Black Flash Fix

## Context

Following [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]], the user confirmed the waypoint feature now works as intended and the crash is gone, with one remaining cosmetic glitch: warping to the Level 1 waypoint showed a fraction of a second of complete blackness before the dungeon rendered.

## Root cause

`LoadGameLevel()` computes the destination level's initial lighting using whatever position the player had *before* `ApplyPendingWaypointSpawn()`'s reposition runs (the default spawn point, not the waypoint sigil). `FixPlayerLocation()`'s `ChangeLightXY`/`ChangeVisionXY` calls only *flag* that lighting/vision need recomputing (`UpdateLighting`/`UpdateVision`) - the actual recompute normally happens via `ProcessLightList()` on the next game tick. That's one tick too late: the level's first rendered frame used the stale, wrong-position lighting, which was near-total darkness since the default spawn's light radius doesn't reach the waypoint sigil's tile.

## What changed

[Source/objects.cpp](../../../Source/objects.cpp): `ApplyPendingWaypointSpawn()` now calls `ProcessLightList()` and `ProcessVisionList()` immediately after `FixPlayerLocation()`, forcing the recompute before returning - closing the gap before the level's first frame is drawn.

Bumped `ORACOOL_VERSION` to `1.0.49` and rebuilt the Debug config clean.

## Verification

Debug build completed with no errors. Not yet manually retested by the user.

## Related

- [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]]

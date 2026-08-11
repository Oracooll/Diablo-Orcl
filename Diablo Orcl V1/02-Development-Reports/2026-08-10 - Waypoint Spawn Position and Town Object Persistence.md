---
title: 2026-08-10 - Waypoint Spawn Position and Town Object Persistence
date: 2026-08-10
tags: [dev-report]
summary: Warping via a waypoint now spawns the player at the destination's sigil instead of the level's default entry point, and the Stash Chest / waypoint sigil no longer vanish when returning to a previously-visited town in the same session.
---

# Waypoint Spawn Position and Town Object Persistence

## Context

Following [[2026-08-09 - Waypoint Unlock State and Real Warping]], the user tested the full loop: activate the Level 1 waypoint (worked - lit up, list entry turned white), select Tristram (worked - warped to town), then reported two problems on arrival:

1. Spawned at the new-game spawn point instead of the town waypoint's own coordinates. Their stated rule: warping to any waypoint should always spawn the player at that waypoint's tile.
2. The Stash Chest and the town waypoint sigil were both gone, as if town had regenerated without them.

## What changed

### Bug 2: town objects disappearing (root cause and fix)

Traced to `SaveLevel()`/`LoadLevel()` (loadsave.cpp) - the mid-session per-level state snapshot system used whenever a player leaves and later returns to an already-visited level. For `leveltype == DTYPE_TOWN`, both functions **skip** `Objects`/`ActiveObjects`/`dObject` entirely (vanilla town has zero Objects[]-pool props, so there was never anything to save there) - but `ActiveObjectCount` itself gets written/read *unconditionally*, outside that guard. Net effect on a return visit to town: `ActiveObjectCount` gets restored to whatever it was (e.g. 2, for our Stash Chest + waypoint sigil), while the actual object data behind it - contents of `Objects[]`, `ActiveObjects[]`, `dObject[][]` - never gets restored at all, since the write and the read both skip it for town.

The original town-branch code ([Source/diablo.cpp](../../../Source/diablo.cpp)) placed the Stash Chest and waypoint sigil under the same "only on a genuinely fresh town generation" guard the dungeon branch uses, on the theory that a return visit's reload would otherwise duplicate them. That premise doesn't hold for town - there's no reload to duplicate against, so a return visit needs these two re-placed from scratch every time, not just once. Removed the guard entirely; `InitTownObjectPool()`/`AddStashChestObject()`/`AddWaypointSigilObject()` now run unconditionally on every town entry. Multiplayer behavior is unchanged - its `gbIsMultiplayer` branch of the old guard already ran these every time.

### Bug 1: spawn position

New `oracool::ConsumeWaypointSpawnRequest()` ([Source/oracool/waypoint_menu.h](../../../Source/oracool/waypoint_menu.h)/`.cpp`): `CheckWaypointMenuClick()` sets a pending flag right before calling `StartNewLvl()`; `AddWaypointSigilObject()` ([Source/objects.cpp](../../../Source/objects.cpp)) consumes it right after placing the destination's sigil, then directly relocates the player there - clearing the old `dPlayer` collision-grid registration, setting the new tile/old position and `dPlayer` entry, and calling `FixPlayerLocation()` (the same helper `StartStand`/warp levers already use) to sync `ViewPosition`, light, and vision.

This has to be a post-placement reposition rather than simply setting `ViewPosition` earlier, because `InitPlayer()` (which reads `ViewPosition` to place the player - see `player.cpp:2591`) already runs *before* `AddWaypointSigilObject()` during level load. Town's sigil position is a fixed constant known in advance, but a dungeon level's sigil is placed at a fresh random spot every visit (`GetRndObjLoc`) that genuinely isn't known until placement - so the same mechanism has to work for both, and placement-time is the only point that reliably knows the answer for either.

Bumped `ORACOOL_VERSION` to `1.0.44` and rebuilt the Debug config clean.

## Verification

Debug build completed with no errors. Not yet manually retested in-game by the user - both fixes address the exact sequence reported (Level 1 waypoint → Tristram).

## Not done / deliberately left alone

- The player-reposition path (dPlayer clear/set + `FixPlayerLocation`) is a lower-level engine operation than most of this session's other fixes, reusing an existing pattern (`StartStand`) rather than a novel one, but it hasn't been exercised by a live test yet - worth specifically confirming no light/vision artifacts or a stray collision tile after landing.
- Still no save/load persistence for `WaypointUnlocked` - unchanged from [[2026-08-09 - Waypoint Unlock State and Real Warping]]'s note.

## Related

- [[2026-08-09 - Waypoint Unlock State and Real Warping]]
- [[2026-08-09 - Autosave-Only Play, Part 1]]

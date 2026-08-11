---
title: 2026-08-10 - All 16 Dungeon Waypoints and givewp Debug Command
date: 2026-08-10
tags: [dev-report]
summary: Extended waypoint placement from just Level 1 to all 16 dungeon levels, and added a givewp debug command that unlocks every waypoint on the current difficulty for testing.
---

# All 16 Dungeon Waypoints and givewp Debug Command

## Context

The waypoint system (unlock state, spawn positioning, town object persistence, the crash and lighting bugs) has now been ironed out on Cathedral Level 1 across [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]] and its follow-ups, matching the project's original staged plan of proving the mechanism on one real dungeon level before extending it. The user asked to extend to the remaining 15 levels and add a debug command to unlock them all at once for testing, rather than having to walk to and activate all 16 sigils by hand.

## What changed

- **`Source/objects.cpp`**: `AddWaypointSigilObject()`'s placement condition widened from `currlevel == 1` to `currlevel >= 1 && currlevel <= 16` - every dungeon level now gets a randomly-placed sigil on entry, using the same level-type-agnostic `GetRndObjLoc()` search Level 1 already used (no Cathedral-specific logic existed to begin with, so this is a pure range extension).
- **`Source/diablo.cpp`**: the graphics-registration fix from [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]] (`EnsureWaypointGraphicsLoaded()`, unconditional on every level entry to prevent the revisit crash) widened the same way, from `currlevel == 1` to `currlevel >= 1 && currlevel <= 16`.
- **`Source/debug.cpp`**: new `givewp` command - calls `oracool::UnlockWaypoint(i)` for `i` in 1-16 (Tristram/0 is always unlocked already, nothing to do there).

Stale comments referencing "only level 1" or "currlevel == 1 right now" in `objects.cpp` and `oracool/waypoint_menu.cpp` were updated to match.

Bumped `ORACOOL_VERSION` to `1.0.53` and rebuilt the Debug config clean.

## Why

17 total levels (town + 16 dungeon) matches this build's non-Hellfire level count exactly (`giNumberOfLevels = gbIsHellfire ? 25 : 17`), so no Hellfire-specific Nest/Crypt handling is needed - `WaypointNames` already had all 17 entries defined since the feature's original build-out, only placement was scoped down to one level for staged testing.

## Verification

Debug build completed with no errors. Not yet manually retested - worth checking a few different level types (Catacombs, Caves, Hell), not just more Cathedral levels, since the graphics/crash fixes were only proven against Cathedral so far.

## Related

- [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]]
- [[2026-08-10 - Scoped Black Flash Fix to Dungeon Only]]

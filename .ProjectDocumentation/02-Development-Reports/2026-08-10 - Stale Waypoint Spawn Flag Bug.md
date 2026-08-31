---
title: 2026-08-10 - Stale Waypoint Spawn Flag Bug
date: 2026-08-10
tags: [dev-report]
summary: Found and fixed the real cause of two of three newly-reported bugs - returning to a waypoint's own level via warp never consumed the pending spawn-reposition flag, leaving it stuck and firing on a later, unrelated level entry (including a save reload) instead.
---

# Stale Waypoint Spawn Flag Bug

## Context

Following [[2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table]], the user reported three problems from a single test session:

1. The Level 1 waypoint unlock didn't survive a close-and-reopen.
2. Warping from Level 1 to the town waypoint worked correctly, but warping back from town to Level 1 landed at the Cathedral's normal staircase entrance instead of the waypoint.
3. Reopening the game (after closing it) spawned the player standing on the *town* waypoint - even though they hadn't been in town, let alone warped there, right before closing.

## Investigation and root cause (bugs 2 and 3)

Traced through `LoadGameLevel()` (diablo.cpp): a dungeon level's objects only get freshly placed via `InitObjects()`/`AddWaypointSigilObject()` on a level's *first* visit (or a save reload). A *return* visit to an already-visited dungeon level within the same session instead calls `LoadLevel()`, which restores that level's previously-saved objects (including its waypoint sigil, at its original position) - correctly, this is the same mechanism dungeon objects have always used to persist within a session.

The bug: `AddWaypointSigilObject()` was also the *only* place that consumed the "warp here, then spawn at the sigil" flag set by `CheckWaypointMenuClick()` (see [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]]). Since that function never runs on a level revisit, the flag was never consumed there - it stayed set (`true`), waiting to spuriously fire on the *next* time `AddWaypointSigilObject()` ran for *any* level, town or dungeon, whether that was minutes later or an entirely different game session after a reload.

Concretely, in the user's sequence: warping from Level 1 to town consumed the flag correctly (bug the user didn't have). Warping back from town to the *already-visited* Level 1 set the flag again, but nothing consumed it - the player landed at the normal Cathedral entrance (bug 2), and the flag stayed stuck at `true`. The next time the game placed a waypoint sigil - on reload, for whichever level the save put them on - it fired against that unrelated level entry, relocating the player to that level's waypoint regardless of where they'd actually saved (bug 3, landing on the town waypoint specifically because that's what got loaded).

## What changed

- **`Source/oracool/oracool.h`**: new `ApplyPendingWaypointSpawn()` - searches the current level's active objects for its waypoint sigil (rather than taking a position, so one implementation covers both a freshly-placed sigil and a `LoadLevel()`-restored one) and, if a spawn is pending, relocates the player there.
- **`Source/objects.cpp`**: `AddWaypointSigilObject()` now calls this shared function instead of its own inline reposition logic (previously written assuming it always had a freshly-placed sigil's position on hand).
- **`Source/diablo.cpp`**: added a call to `oracool::ApplyPendingWaypointSpawn()` right after `LoadLevel()` in the dungeon already-visited branch - the one path that previously never consumed the flag at all.

Bumped `ORACOOL_VERSION` to `1.0.46` and rebuilt the Debug config clean.

## Bug 1 (unlock not surviving reload) - not conclusively root-caused

Traced the save path end to end: `oracool::ProcessAutoSave()` → `SaveGame()` → `pfile_write_hero(true)` → `SaveGameData()` → `SavePlayer()`, which does write `_pWaypointUnlocked` (confirmed present in the diff from the previous report); the load path is the mirror of that. Nothing found in this path that would explain a written-then-lost unlock. Also confirmed there's no explicit "Save on quit" - Exit Game reuses vanilla's abandon-and-close handler (see `gamemenu.cpp`'s own comment on this), so quitting relies entirely on the last autosave already having landed, exactly as intended.

Given the stale-flag bug traced above genuinely corrupted the level-transition sequence in the user's test run (the wrong-position teleport in bug 2/3), it's plausible it was an unrecognized contributing factor here too, but this isn't confirmed. Asking for a clean, isolated retest of bug 1 specifically now that the stale-flag bug is fixed: activate a waypoint, check the Event Log for a "Game saved (auto)" line appearing right after, then close and reopen.

## Verification

Debug build completed with no errors. Not yet manually retested in-game by the user.

## Related

- [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]]
- [[2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table]]

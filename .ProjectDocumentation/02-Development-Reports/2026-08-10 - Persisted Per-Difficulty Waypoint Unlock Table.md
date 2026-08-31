---
title: 2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table
date: 2026-08-10
tags: [dev-report]
summary: Waypoint unlock state now lives on the character save (Player::_pWaypointUnlocked), one table per difficulty, instead of a runtime-only array that forgot everything on reload.
---

# Persisted Per-Difficulty Waypoint Unlock Table

## Context

The user unlocked the Level 1 waypoint in the previous test session, expecting it to have autosaved (it did - `Auto Save on Waypoint Activation` fires on every activation) and to still be unlocked on reload. It wasn't. Root cause: every earlier waypoint-system report explicitly flagged this as a known gap - `WaypointUnlocked` was a process-lifetime-only array in `oracool/waypoint_menu.cpp`, never written to the save file at all, so an autosave had nothing to persist. The user's explicit ask: "every character should have a table of locked/unlocked wp for every difficulty level" - i.e. real per-character, per-difficulty persistence, matching how Diablo 2's waypoints work.

## What changed

- **`Source/player.h`**: new `bool _pWaypointUnlocked[4][17] = {};` on `Player`, indexed `[difficulty][waypoint list index]` (0 = Tristram, 1-16 = that dungeon level). Lives alongside `_pLvlVisited`/`_pSLvlVisited`, the existing precedent for this exact kind of per-character array.
- **`Source/loadsave.cpp`**: `LoadPlayer()`/`SavePlayer()` read/write this table, appended after every vanilla field (right after the existing Reset Stats extension, which already established the pattern of extending this save format for new Oracool per-character data). Not slotted into the vanilla format's remaining "Available bytes" padding - there isn't enough of it left (4 bytes, need 68). Backward compatible for free: `LoadHelper::Next` returns `0` once its read cursor runs past a save file's actual length, so a save made before this change just loads with every waypoint but Tristram locked - no version-gating needed.
- **`Source/oracool/waypoint_menu.cpp`/`.h`**: removed the old runtime-only array; `IsWaypointUnlocked()`/`UnlockWaypoint()` now read/write `MyPlayer->_pWaypointUnlocked[sgGameInitInfo.nDifficulty][index]` directly. Index 0 (Tristram) is still special-cased as always-unlocked regardless of stored state, matching the original design.

Bumped `ORACOOL_VERSION` to `1.0.45` and rebuilt the Debug config clean.

## Why

Difficulty-scoped rather than shared across difficulties, per the user's explicit request - unlocking Cathedral Level 1's waypoint on Normal shouldn't also unlock it on Nightmare/Hell/Torment, since those are meant to be separately-played runs with their own progress.

Appended at the end of the save format rather than inserted mid-sequence: `SaveHelper`/`LoadHelper` read/write sequentially, so a mid-sequence insertion would desync every field after it for any save made before this change. Appending at the very end avoids that entirely - old saves just run out of data early (for this one array) and get the sane all-locked-except-Tristram default.

## Verification

Debug build completed with no errors (167 targets, `DiabloOrcl.exe` linked). Not yet manually retested - the user's existing test character predates this field, so it will load with the Level 1 waypoint locked again once; unlocking it from here forward should survive a save/reload.

## Related

- [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]]
- [[2026-08-09 - Autosave on Waypoint Activation]]

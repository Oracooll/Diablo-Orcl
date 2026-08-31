---
title: 2026-08-10 - Waypoint Unlock Survives New Game
date: 2026-08-10
tags: [dev-report]
summary: Root-caused why an unlocked waypoint reverted to locked on "New Game" with the same character - it was never persisted in PlayerPack, the compact struct that flow actually uses, only in the full save/load path used by "Continue". Fixed by adding a bitmask-packed unlock table to PlayerPack itself.
---

# Waypoint Unlock Survives New Game

## Context

Following [[2026-08-10 - Waypoint Spawn Black Flash Fix]], the user confirmed the waypoint feature works and no longer crashes, but reported the Level 1 waypoint's unlocked status wasn't remembered after "starting a new game." A clarifying question confirmed this was the *same* character, not a freshly-created one - a genuine persistence bug, not the expected "new character has no unlocks yet" behavior.

## Root cause

`Player::_pWaypointUnlocked` (added in [[2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table]]) was only wired into `loadsave.cpp`'s `SavePlayer()`/`LoadPlayer()` - the full save/load path, exercised only when `WM_DIABLOADGAME` fires (vanilla's "Continue" flow). **Correction after this report was first written: V1 doesn't actually offer a Continue/New Game choice - every session start uses `WM_DIABNEWGAME` unconditionally.** So `LoadPlayer()`/`SavePlayer()` isn't "the path New Game happens to skip" - in V1 it's inert for every normal play session, never invoked by anything a player does. It's kept only because it's harmless, and because a few narrower flows outside regular play (demo mode sets `gbLoadGame = true` directly) still route through it.

Every session, regardless of that, starts with `pfile_read_player_from_save()` calling `UnPackPlayer()` (`pack.cpp`) to populate `*MyPlayer` from the save file's compact `PlayerPack` struct - and `UnPackPlayer()` begins with `player = {};`, resetting the whole struct before repopulating it from `packed`. Since `PlayerPack` never carried the waypoint-unlock table, this reset silently discarded it every single session, regardless of what `LoadPlayer()` might otherwise restore (which, in V1, never runs at all during normal play).

This is the exact same bug class the existing `pStatPtsSpentStr/Mag/Dex/Vit` fields were added to fix, and the fix follows the same established pattern - see that field's own comment in `pack.h`. **`PlayerPack`/`UnPackPlayer` (`pack.cpp`) is therefore the only place that matters for any future per-character persistent field in V1** - `loadsave.cpp`'s `SavePlayer`/`LoadPlayer` should not be assumed to be the primary path for new work, despite being the more obviously-named "save the player" function.

## What changed

`PlayerPack` (`pack.h`) is a fixed-layout, exact-size-checked struct (`ReadHero()` rejects any save whose byte count doesn't match `sizeof(PlayerPack)` precisely), so new fields can't just be appended - that would break every existing save file. Repurposed three already-unused "reserved for future use" fields instead, keeping the struct's total size unchanged:

- `reserved2[2]` → `pWaypointUnlockedNormal` (`uint16_t`)
- `wReserved8` → `pWaypointUnlockedNightmare` (`uint16_t`)
- `reserved3[4]` → `pWaypointUnlockedHell` + `pWaypointUnlockedTorment` (two `uint16_t`)

Each is a 16-bit bitmask, bit `(i-1)` = waypoint list index `i` (1-16) unlocked; index 0 (Tristram) is always unlocked and never stored, so 16 bits per difficulty is exactly enough. New `PackWaypointUnlocked()`/`UnpackWaypointUnlocked()` helpers (`pack.cpp`) convert to/from `Player::_pWaypointUnlocked`, wired into `PackPlayer()`/`UnPackPlayer()` right alongside the existing `pStatPtsSpent*` handling.

`test/pack_test.cpp`'s `PlayerPack` aggregate-initializer test literal was updated to match the new field shapes (a 2-byte array collapsing to one scalar, a 4-byte array splitting into two scalars) - the struct's total layout and byte count are otherwise unchanged.

Bumped `ORACOOL_VERSION` to `1.0.50` and rebuilt the Debug config clean.

## Why

Reusing existing zero-filled reserved bytes rather than growing the struct means old saves - including the user's current test character - decode these fields as `0` (no waypoints unlocked, matching the intended default) instead of failing to load altogether.

## Verification

Debug build completed with no errors. `pack_test` (58 tests, including the corrected `PlayerPack` round-trip test) passes. Not yet manually retested in-game - the user's existing character predates this field, so it should load with Level 1 locked one final time; unlocking it again should now survive both "Continue" and "New Game".

## Related

- [[2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table]]
- [[2026-08-10 - Waypoint Spawn Black Flash Fix]]

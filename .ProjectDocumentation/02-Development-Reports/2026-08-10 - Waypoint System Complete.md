---
title: 2026-08-10 - Waypoint System Complete
date: 2026-08-10
tags: [dev-report]
summary: Closing milestone for the waypoint feature - all 16 dungeon levels plus town tested with zero problems after a series of root-cause fixes. Marking the core feature done; two items remain deliberately out of scope.
---

# Waypoint System Complete

## Context

Closing entry for the waypoint arc that began with [[2026-08-09 - Autosave-Only Play, Part 1]]'s staging decision to iron out one real dungeon level before expanding. The user tested all 16 dungeon levels plus town today and reported zero problems - a clean end-to-end pass across every level type (Cathedral, Catacombs, Caves, Hell), not just the Cathedral level the fixes were originally proven against.

## What's built

- A waypoint sigil in town (fixed position) and on every dungeon level 1-16 (randomly placed on each visit, since dungeon layouts regenerate).
- Click-to-activate: first interaction lights the sigil, plays a sound, and opens a Quest-Log-styled travel list.
- Real unlock state, persisted per character *and* per difficulty (Normal/Nightmare/Hell/Torment don't share progress), surviving both save/reload and "New Game" with an existing character.
- Real travel: selecting an unlocked entry warps you there and spawns you at the destination's actual sigil, not the level's default entrance - both directions, town-to-dungeon and dungeon-to-dungeon-via-town.
- Town's Stash Chest and waypoint sigil now survive returning to a previously-visited town within a session (a real, separate engine gap this work exposed and fixed).
- Autosave triggers on activation, plus a `givewp` debug command to unlock everything at once for testing.

## Bugs found and fixed along the way (chronological)

1. Waypoint/Stash object pool collision in town (town never initialized `Objects[]` the way dungeon levels do) - [[2026-08-09 - Autosave-Only Play, Part 1]]'s era.
2. Menu not closing when walking away from the sigil.
3. Spawn position landing at the default entry point instead of the waypoint, both directions - [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]].
4. Town's Stash Chest/sigil vanishing on a return visit - same report; root cause was `SaveLevel()`/`LoadLevel()` never handling objects for `DTYPE_TOWN`.
5. A stale one-shot "spawn here" flag getting consumed on the wrong level when a revisit skipped the normal placement code path - [[2026-08-10 - Stale Waypoint Spawn Flag Bug]].
6. A hard crash approaching a revisited level's sigil - traced to its graphic never being re-registered on a revisit, leaving `_oAnimData` unset - [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]].
7. A second timing bug in the same fix: the spawn-reposition flag being consumed a tick too early, before the level transition had actually started.
8. A black flash on arrival, and then a worse regression (corrupted red/blue town rendering) from the first fix attempt at it - [[2026-08-10 - Waypoint Spawn Black Flash Fix]] and [[2026-08-10 - Revert Lighting Force-Recompute - Corrupted Town]] - resolved by scoping the fix to dungeon levels only, where a lighting baseline is reliably established before the reposition runs ([[2026-08-10 - Scoped Black Flash Fix to Dungeon Only]]).
9. Per-character persistence silently failing because `V1` has no Continue/New Game distinction - every session starts via the compact `PlayerPack` path, not the full save/load path the unlock table was first wired into - [[2026-08-10 - Waypoint Unlock Survives New Game]].

## Deliberately out of scope

- **Town's own black flash on arrival** - the dungeon-side fix (forcing lighting to recompute immediately after the reposition) was found to corrupt town's rendering instead, since town has no equivalent pre-established lighting baseline. Scoped out rather than risk that regression; town keeps a brief flash.
- **The "channeled portal" mechanic** - 5-second channel, desaturated portal effect, interrupted by taking a hit or moving, from much earlier in the project's roadmap. Warping is currently instant. This was always staged as a later addition, not part of getting the core mechanism working.
- **Pre-generating all levels at game start** - discussed and explicitly declined (see prior turn): the engine keeps only the current level's state resident by design; pre-generating everything would need redesigning how every subsystem stores per-level data, for no real gameplay benefit over the lazy per-visit generation already in place.

## Related

- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table]]
- [[2026-08-10 - All 16 Dungeon Waypoints and givewp Debug Command]]

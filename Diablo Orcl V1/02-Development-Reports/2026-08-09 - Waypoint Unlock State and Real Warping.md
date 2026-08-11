---
title: 2026-08-09 - Waypoint Unlock State and Real Warping
date: 2026-08-09
tags: [dev-report]
summary: Waypoints now actually unlock on first activation (lit sprite + sound), the travel list reflects real unlock state instead of a hardcoded Tristram-only count, and selecting an unlocked entry genuinely warps the player there instead of being a no-op.
---

# Waypoint Unlock State and Real Warping

## Context

The user found and tested the Cathedral Level 1 waypoint sigil placed in [[2026-08-09 - Autosave on Waypoint Activation]]'s predecessor work and reported three problems:

1. Clicking the dormant sigil didn't turn it lit/active.
2. Opening the travel list still showed "Cathedral Level 1" in gold (locked), not white.
3. Clicking Tristram in the list did nothing.

All three traced back to the same root cause: the waypoint system had no real unlock state or warp logic at all yet. That was a known, deliberate gap from the original build-out (see [[2026-08-09 - Autosave-Only Play, Part 1]]'s pending-tasks note: "no unlock-persistence system... `OperateWaypoint()` just opens the menu regardless of locked state" and "the actual warp-on-select mechanic... currently a no-op"), not a regression - but the user asked for a real look at the whole system rather than another incremental patch.

## What changed

- **`oracool/waypoint_menu.h`/`.cpp`**: replaced the hardcoded `UnlockedCount = 1` constant with a real runtime `std::array<bool, 17> WaypointUnlocked` (index 0 = Tristram, always unlocked; 1-16 = that dungeon level, matching `currlevel` numbering), exposed via new `IsWaypointUnlocked(int)` / `UnlockWaypoint(int)`. `DrawWaypointMenu()` now colors each entry from real state instead of the old fixed count.
- **`CheckWaypointMenuClick()`**: selecting an unlocked entry now closes the menu and calls `StartNewLvl(*MyPlayer, WM_DIABNEXTLVL, entry)` - the same function vanilla stairs/portals and the `goto` debug command already use - with a no-op guard if the player is already on that level. Entry index doubles directly as the destination level number.
- **`objects.cpp` `OperateWaypoint()`**: clicking a still-locked sigil now unlocks it (`UnlockWaypoint`), switches its sprite to the lit frame (`_oAnimFrame = 2`), and plays `IS_MAGIC` (the same sound Shrines use) - all in the same click that opens the travel list, matching how Diablo 2's waypoints work (first click both activates and opens the list).
- **`objects.cpp` `AddWaypointSigilObject()`**: now stamps each sigil with its own list index in `_oVar1` (`currlevel` - free integer field, unused elsewhere on this object type) so `OperateWaypoint` knows which entry it corresponds to. A sigil's initial frame is also read from `IsWaypointUnlocked()` now instead of being hardcoded dormant - so revisiting a regenerated level after already unlocking its waypoint shows it lit again from the start, not dormant.

Bumped `ORACOOL_VERSION` to `1.0.40` and rebuilt the Debug config clean.

## Why

Storing the list index on the object itself (`_oVar1`) rather than inferring it some other way keeps `OperateWaypoint` a pure function of the object it's given - no hidden coupling to `currlevel` at click-time, which matters since nothing stops a future waypoint from being clickable from a different context than where it was placed.

`WM_DIABNEXTLVL` was chosen over `WM_DIABTOWNWARP` for the Tristram entry (level 0) specifically because that's what the already-proven `goto` debug command does for level 0 - `WM_DIABTOWNWARP` is only used there for level 21 (a Hellfire-specific case), and both modes reduce to the identical `player.setLevel(lvl)` call inside `StartNewLvl` regardless, so matching the known-working path was the safer choice over guessing.

## Not done / deliberately left alone

- **No save/load persistence yet.** `WaypointUnlocked` is runtime-only - quitting and reloading forgets which waypoints were unlocked. This was true before this change too; today's fix makes unlocking work *within* a session but doesn't add the save-file work needed to survive across sessions. Flagging this explicitly since it's the next thing likely to surface as "still broken" in testing.
- No instant-warp-only decision revisited: the long-deferred "5-second channel + desaturated portal + interrupt" Part 2 mechanic (noted in earlier waypoint dev history) still isn't built - warping remains instant, matching the current iron-out-level-1 stage.
- Waypoint placement itself is still only town + Cathedral Level 1 - extending to the other 15 levels is unchanged scope from before this fix.

## Verification

Debug build completed with no errors. Not yet manually retested in-game by the user - the original three symptoms should now be resolved: the sigil should light up and play a sound on first click, the list entry should turn white, and selecting Tristram from level 1 should actually warp back to town.

## Related

- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[2026-08-09 - Autosave on Waypoint Activation]]

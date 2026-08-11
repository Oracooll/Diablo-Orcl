---
title: 2026-08-10 - Revert Lighting Force-Recompute - Corrupted Town
date: 2026-08-10
tags: [dev-report]
summary: The black-flash fix from earlier today corrupted town's ground tiles into red/blue static instead. Reverted it - a brief flash is a smaller regression than corrupted rendering.
---

# Revert Lighting Force-Recompute - Corrupted Town

## Context

The user confirmed [[2026-08-10 - Waypoint Spawn Black Flash Fix]] worked - no more black flash warping into Level 1 - but reported a new problem: warping to the Tristram waypoint left the town's ground tiles covered in red/blue static-like noise instead of properly rendering. Converted the user's latest `.pcx` screenshot (DevilutionX's own screenshot format, not directly viewable - decoded by hand: 128-byte header, RLE-compressed 8bpp scanlines, trailing 769-byte VGA palette) to confirm: buildings, NPCs, and the HUD all rendered correctly, but most of the ground was speckled with what looks like garbage light-table indices being read out of bounds.

## What changed

Reverted the black-flash fix: [Source/objects.cpp](../../../Source/objects.cpp)'s `ApplyPendingWaypointSpawn()` no longer calls `ProcessLightList()`/`ProcessVisionList()` after repositioning the player.

Bumped `ORACOOL_VERSION` to `1.0.51` and rebuilt the Debug config clean.

## Why

Best working theory (not confirmed with a live debugger - noted as such): a dungeon revisit restores `dLight` from its own saved snapshot (`dPreLight`) as part of `LoadLevel()`, so forcing a recompute there is safe. Town doesn't go through that path - it's unconditionally freshly placed every visit - and whatever town's own equivalent lighting baseline step is, it doesn't appear to be settled yet at the exact point `ApplyPendingWaypointSpawn()` runs. Forcing `ProcessLightList()` there anyway seems to have computed light values against incomplete state, and since it also clears the `UpdateLighting` flag that would otherwise trigger a correcting recompute later, the corrupted result stuck around instead of self-correcting on a subsequent frame.

Rather than guess at a second fix without being able to verify it, reverted to the simpler, previously-acceptable trade-off: a brief black flash is a much smaller regression than a corrupted, static-covered town.

## Verification

Debug build completed with no errors. Not yet manually retested by the user - this specifically un-does the black-flash fix, so both symptoms (does the flash return, is the corruption gone) need re-confirming.

## Not done / deliberately left alone

A real fix for the black flash - this report only removes the broken attempt. Worth revisiting later with a better understanding of what town's own lighting-initialization sequence actually needs before it's safe to force an early recompute.

## Related

- [[2026-08-10 - Waypoint Spawn Black Flash Fix]]
- [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]]

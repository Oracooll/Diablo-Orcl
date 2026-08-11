---
title: 2026-08-09 - Mini-Map Waypoint and Portal Markers
date: 2026-08-09
tags: [dev-report]
summary: Added fixed-size blue mini-map markers for waypoint sigils and any open Town Portal on the current level, matching the existing bold stairs/door mini-map marker pattern.
---

# Mini-Map Waypoint and Portal Markers

## Context

User request: a 2x2 blue marker on the mini-map for waypoint sigils and an open Town Portal, matching the existing mini-map marker treatment for stairs (red) and doors (gray).

## What changed

[Source/automap.cpp](../../../Source/automap.cpp):
- New `MiniMapColorsWaypoint = (PAL8_BLUE + 1)` alongside the existing `MiniMapColorsDoor`/`MiniMapColorsStairs`.
- New `AutomapMarkerScreenPosition()` - the same tile-to-screen transform `DrawAutomapPlr`/`SearchAutomapItem` already use for markers drawn outside `DrawAutomapCore`'s own per-tile loop, factored out so a third caller doesn't have to duplicate it.
- New `DrawAutomapWaypointsAndPortals()` - iterates `ActiveObjects`/`Objects` for `OBJ_WAYPOINT` and checks `Portals[MyPlayerId].open && PortalOnLevel(MyPlayerId)`, drawing a 2x2 `FillRect` at each. Called from `DrawAutomapCore` right after the player-marker loop, gated on `MiniMapActive` - mini-map only, matching the request; the full automap doesn't get these since the sigils/portal are already visible there as real sprites.

Bumped `ORACOOL_VERSION` to `1.0.41` and rebuilt the Debug config clean.

## Verification

Debug build completed with no errors. Not yet manually tested in-game by the user.

## Related

- [[2026-08-09 - Waypoint Unlock State and Real Warping]]

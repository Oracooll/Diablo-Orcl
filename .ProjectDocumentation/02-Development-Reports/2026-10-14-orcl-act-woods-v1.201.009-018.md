---
title: 2026-10-14 - Orcl Act woods, v1.201.009-018
date: 2026-10-14
tags: [dev-report, orcl-act]
summary: The Orcl Act's woods grew from one floor into an act - four linked floors with monsters, waypoints, caves, fences, rivers, houses, a boss and weather - on the orcl-act line, while the user was away.
---

# Orcl Act woods, v1.201.009-018

## Context

The user went away and asked for work to go on without waiting ("you can keep working in the mean time on everything else you need to develop"). Their earlier notes set the list: linked floors, monsters scaled to the hero, waypoints (the plan page's D3, D9, D10, D8), "cave entrances which leave to 1-2 underground level", "houses which can be entered and their roof fading away", "grass, rivers, bridges", fences round the floors ("we can see them in levels 9-12"), and while the work ran: "i dont want us to limit ourselves to one tileset", smaller ground textures, an Orcl Testing Ground page whose settings were then applied, and "Game drawn roofs dont look good ... We do have vanilla roofs in tristram houses".

All of it is on branch `orcl-act` in the `DiabloOrcl-OrclAct` worktree, built to `C:\Diablo Orcl\x64-Debug-OrclAct`. Nothing is on the main line.

## What changed

| Version | What |
|---|---|
| 1.201.009 | Mixed look: the painted road over Tristram's grass with a soft edge, the grass graded to olive; the deep forest drawn one tree in four; faster tree sprites. |
| 1.201.010 | Four linked woods floors; monsters by the hero's rung (one per three levels, plus one per floor, fixed at the first visit); a waypoint on every floor (indices 25-28, the Orcl Act tab). Fixed: set level to set level saved the floor left under the next floor's name (`interfac.cpp` WM_DIABSETLVL). |
| 1.201.011 | `oracool/borrowed_tiles`: any vanilla tileset's pieces on any level, walking by their own `.sol`, drawn as true-colour pictures rendered from their own cels and palette. Ground at half its scale. |
| 1.201.012 | `oracool/woods_caves`: the Mossbound Den (one floor) and the Underroot (two), on the Caves tileset, with stairs, objects and the act's monsters. |
| 1.201.013 | The Caves' fences: a ring round each floor with gates on the roads, fenced pens; fewer trees; the cave mouth is Tristram's own cave hole in its rock outcrop. |
| 1.201.014 | Rivers: Tristram's west river traced and rebuilt as a generator, its wooden bridges where roads cross. |
| 1.201.015 | Walk-in houses: Catacombs walls (drlg_l2's pattern ids), stone floor, an arch, loot on the first visit; a game-drawn roof. |
| 1.201.016 | Champions in the woods and caves; a Dread boss always on the fourth floor. |
| 1.201.017 | The Testing Ground's scales (ground 13-16 %, trees and details 25 %), its tree mix; the drawn roof replaced by Tristram's own cottage drawn over each house, gone while the hero is inside. |
| 1.201.018 | Weather, rain, puddles, cloud shadows, footprints and fireflies under the woods' sky too (`IsUnderSky`). |

The [[Orcl Testing Ground]] page (an artifact) shows the woods textures at chosen scales with the warrior walking; its saved values were read back with the artifact database and put into `woods.cpp` `LoadArt`.

## Why

- **Borrowing tiles rather than a second tileset in memory per level**: the renderer resolves palette indices through one global palette, so a borrowed piece is rendered once into true colour in its own palette and then blitted like the woods' painted sprites - lit, see-through round the hero, no per-frame palette swaps.
- **Tristram's own art wherever it exists** (river, bridge, cave hole, cottage): it fits the woods' Tristram tileset exactly and costs no new art. The river and the cottage were read out of the town map by a preview test (`OracoolPreview.DISABLED_WoodsMixedView`) that renders through the game's tile renderer and prints the town's pieces.
- **The cottage as a shell over a Catacombs room**: Tristram's houses are solid art with no inside; a room built under the cottage's walls gives the inside, and the cottage is simply not drawn while the hero is in it.

## Verification

- Every version built clean, and the full ctest passed: 1066 of 1066 at v1.201.017. The v1.201.018 run was still going when this report was written.
- New tests:
  - `WoodsRosterHasFourKindsOnEveryRung`, which caught two-kind rosters on rungs 9-13 and led to the doubled image budget;
  - `BorrowedTilesKeepTheirOwnSetsRules`;
  - `WoodsCaveMouthIsReachableAndCaveFloorsHaveTheirStairs`, which walks six seeds to both road exits through the fence gates and bridges, the cave mouth and into every house, and checks cave stairs.
- Previews were rendered and looked at for the Mixed look, the mouth, a gate, the bridge and the houses (outside and inside).
- **Not seen in the running game**: the user plays and records; nothing here has been played yet.

## Not done / deliberately left alone

- Gates stand open: a Caves gate's leaf is an object in the Caves palette.
- The Hellfire Hive mound was tried as the cave mouth and replaced: it read as a slab.
- The game-drawn roof was built and removed at the user's word.
- Day and night lighting stay town-only; the woods keep their fixed light.
- Diablo 2 assets: discussed, nothing done (the user would point to their own MPQs).
- The desert is not started.
- RfA-64b props are still awaited.

## Related

- [[Road to the Orcl Act]]

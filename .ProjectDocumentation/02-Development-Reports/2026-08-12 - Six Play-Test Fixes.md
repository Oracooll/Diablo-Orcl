---
title: 2026-08-12 - Six Play-Test Fixes
date: 2026-08-12
tags: [dev-report]
summary: Textbox bevel around the mini-map and log, a clipped XP counter, waypoints landing on cave bridges, a click that fell through the Character panel to the waypoint list, and hover panels doubling up with item labels.
---

# Six Play-Test Fixes

Six findings from play-testing 1.1.5.

## 1. The textbox bevel, on the mini-map and the log

`textbox_frame00` is a 591x303 CEL, so it cannot frame a 306x175 mini-map or a log window whose height depends on the screen. What it *is*, though, is a plain 3px bevel - and that reproduces at any size.

Sampled at the mid-point of each edge and matched back to palette indices, the frame turns out to be lit from the bottom-right: all four outer edges share a mid-gold (202), the **second** ring carries the lighting - dark 204 on the top and left, bright 198 on the bottom and right - and the innermost ring is near-black (223) all round. Every one of those lives in the shared upper half of the palette, so the frame is the same colour in town and in all four tilesets.

`oracool/ornate_border.h` draws it procedurally. Both the mini-map and the event log switch to it, and the 1px dashed placeholder they shared - which `DrawMiniMap`'s own comment had described as "the placeholder the planned textured border art will replace" - is deleted along with `DrawDashedBorder1px` and the now-unused `EventLogBorderColor`.

The original's edges vary by a shade or two along their length; those are hand-painted texture rather than a rule, so the modal value is used. Reproducing the per-pixel noise would need the art itself, at which point it could not be resized - which was the whole problem.

## 2. The XP counter was clipping, not miscounting

"220,000,000 / 10" was the real string "220,000,000 / 100%" with the last two characters cut off. The counter draws into its own click target, which spans belt cells 1-4 - about 150px, and a ten-digit remainder plus a three-digit percentage needs more.

Widening the click target would have pushed it out over the plate's two skill wells and let it swallow clicks meant for them, so there are now two rects: `GetCounterDrawRect` spans the whole plate, `GetCounterRect` still spans the four cells. They share a centre - belt cells 1-4 are themselves centred on the plate - so nothing moves.

## 3 and 6. Waypoints on cave bridges

`AddWaypointSigilObject` runs from `InitObjects`, which is early: the level has no monsters and no items yet, so all it can use is `GetRndObjLoc(2)`, the engine's generic object placer. Its only geometric requirement is a 2x2 block of non-solid floor. A cave bridge satisfies that - hence the screenshot of a platform half-hanging over a drop. Fine for a barrel; not for a two-tile-wide landmark.

New `ImproveWaypointSpawnPosition`, called from `LoadGameLevel` after `CreateThemeRooms` so every monster and item already exists, re-scores every tile:

- **Hard requirements**: a 3x3 clear area (this is what rules out bridges, corridors and doorways), no arch overlay, no set piece, and at least 3 tiles from any level entrance or exit so it never lands on the stairs.
- **Scored**: openness first, capped at a 5x5 clear area; distance to the nearest living monster second, capped at 8. Openness dominates deliberately - a cramped spot is permanent and is what the user complained about, while monsters wander off and get killed. The monster distance is capped rather than maximised because a waypoint in the emptiest corner of the map is safe and useless.

Two deliberate refusals to act: it keeps the generated position if that already scores best, and keeps it too if nothing on the level meets the minimum, logging that instead. A badly placed waypoint still works; moving it somewhere worse would not help.

**No randomness at all**, which matters more than it looks. Level generation is seeded, so drawing from the shared LCG here would shift every later placement and change what a given seed produces. Scoring ties are broken by a hash of the tile mixed with the level seed - without which the first tile in scan order wins every tie and the waypoint drifts to the map's top-left corner on every level.

Fresh generation only. A revisited level restores the waypoint's saved position, which must not move under the player.

## 4. The click that fell through the Character panel

With the waypoint list and the Character panel both open, clicking the Character panel teleported the player.

The draw chain and the click chain each had their own hand-written precedence, and they disagreed. The renderer said Character wins and drew it; the click handler tested the waypoint list first, four `else if`s earlier, and routed the click there. The waypoint list was invisible and still live.

`GetLeftPanelContent()` is now the single authority - one enum, one order - and both chains read it. `IsLeftPanelOpen()` is defined in terms of it too. The same bug existed between the quest log and the Character panel and is fixed by the same change.

Worth noting what is *not* fixed: the burger menu and the quest-text popup both draw on top of the left-panel group but are still tested after it in the click chain. That is the same class of defect, but changing it alters what a click does while those popups are open, which needs its own play-test.

## 5. No hover panel over labelled ground items

With item labels on, every item on the floor is already named where it lies, so the hover panel over one is the same information twice. Ground items only - the inventory, stash and belt keep theirs, since labels do not cover them.

One consequence worth stating: a magic or unique item on the floor now shows only its name until it is picked up, because the label carries the name and nothing else.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.7**, version string confirmed in the exe. Tests **347/349** - the same two pre-existing failures (`Drlg_l1` stairs, `Timedemo`'s pre-save-break demo character), no new ones.

Needs a play-test on all six, and particularly: generate several fresh dungeon levels and check where the waypoints land, since that rule is entirely untested against real layouts.

## Related

- [[2026-08-12 - One Item Panel, Quieter Hovers, Waypoint Behind Everything]]
- [[2026-08-11 - Waypoint Sprite and the First CEL Encoder]]

# v1.12.420 - lighting/stutter audit, round 1 (2026-10-10)

Two read-only reviewers over v1.12.419 (coloured + smooth lighting; stutter fixes). No P1. One P2, fixed; P3s fixed where cheap.

## Fixed
- **P2 - see-through floor tiles undid the smooth floor.** DrawCell redraws the two floor triangles of a non-solid Transparent piece (archways, door frames while the hero's room is transparent) at the tile's own light - identical pixels in vanilla, a tile-sized step and seam after the smooth pass. DrawCell now skips those LeftTriangle/RightTriangle micros when the floor was smoothed this frame (`FloorSmoothedThisFrame`).
- **Sample point 1.5 px high.** A floor triangle is rows y-30..y, widest at y-15: the ground map's tile centre is y-14.5 (was y-16); the colour pass's box and the tests follow.
- **Hell's blood ramp biased the level brightness.** MakeLightTable rewrites Hell's 1..31 (LightTables[0] included); they are left out of the average. (Blood pixels on a smoothed Hell floor still fade to black far from light, where vanilla kept index 1 - accepted.)
- **An exception on the monster-size worker ended the game** (std::terminate, e.g. bad_alloc on a 250% rift boss). Caught; the sheet comes back empty, stays pending (never re-asked) and the stand-in stays.
- **A revisit's throwaway population queued scale jobs** prioritised from the last floor, ahead of the real ones. Not queued while SuppressMonsterLights.
- **The frame-time line showed a false spike after the window came back from the background.** Intervals over a second are not counted.

## Accepted (P3)
- The colour pass tints in-scene highlights drawn during DrawTileContent (hovered item/object/player outlines, curse markers); only the deferred monster outlines escape.
- With smooth floors on, floor-pass objects (waypoint, Stonegate arch) draw after every floor tile, so an overhang shows on top.
- The colour field refreshes with the light list: a hue-only change (berserk set, a tint changed) waits for the next light change - in combat, the next tick.
- Skill sounds accumulate across characters in one run (bounded, ~13 MB).

Build Debug, ctest 971/971.

# 2026-09-29 - Holy Lance sets off from the struck monster (v1.12.220)

**Date:** 2026-09-29. Debug only. The user: "Holy Lance should initiate its projectile from the tile the hit monster is, not from the hero tile."

## Change

`SwingArt` (`oracool/rfa12_actives.cpp`) drew the Holy Lance thrust sheet at the swing's origin, the hero's tile, facing the swing. It now draws it at `landedOn`, the tile of the monster the blow struck, along the same facing. A swing that hit nothing draws it on the tile in front.

The sheet's delivery anchor (`{0, 80}`, the feet at the cell's centre) puts its start point on that tile's centre, so the lance runs out from the target through the two tiles behind it. The strike already hit those two tiles (`LineOfTiles(ahead, ahead + facing, 2)`); only the picture moved.

## Test

Debug build and ctest: 878/878 passed. Not seen in play yet.

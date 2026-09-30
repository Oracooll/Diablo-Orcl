# 2026-09-30 - Loading screens at 21:9 (v1.12.261)

**Date:** 2026-09-30. Debug only.

The user asked for the new paintings to be used, and to "always zoom into them to fill the screen up to 21:9 aspect ratio.
beyond that ratio - leave black bars on the sides." The source is
`Resources/02. Oracooll Assets/01. Used/loading-screens-no-bars/21x9-3440x1440`.

## Assets

Thirteen 3440x1440 paintings, each without the baked-in loading bar, replace the 1280x720 16:9 set in
`Packaging/resources/oracool_assets`:
- **`gendata/`**: cut2, cut3, cut4, cutgate, cutl1d, cutportl, cutportr, cutstart and cuttt, under their own names.
- **`nlevels/`**: cutl5 and cutl6, the Crypt and the Hive.
- **The rift portals:** `cutportl-guardian-purple` becomes `gendata/cutriftg`, and `cutportl-nephalem-gold` becomes
  `gendata/cutriftn`.

oracool.mpq grows from 54 MB to 174 MB. The paintings are RGBA PNGs at about 10 MB each.

## Code

`interfac.cpp`: `FitToHeight` is replaced by `FitCutscene`. The painting always fills the screen's height:
- **A screen up to the painting's width (21:9)** is filled edge to edge. The painting is zoomed and its sides are
  cropped through a source rectangle, not left to SDL's clipping.
- **A wider screen** shows the whole painting centred between black bars.

The CEL fallback (a 4:3 painting from the player's diabdat) follows the same rule, so it keeps its bars on 16:9 as before.

## Tests

Debug build and ctest: 891/891. The Debug oracool.mpq was repacked with `tools\build_oracool_mpq.cmd`, and the Debug
`diablo.ini` was unchanged. Not seen in play: a screenshot on 16:9 and on 21:9 is the check.

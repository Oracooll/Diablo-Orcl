# Levski's Cube wears the user's painted UI (v1.12.092)

**Date:** 2026-09-20 · **Version:** v1.12.092 · **Tests:** 831/831

User: "build Levski's Cube UI with assets from this folder (Resources\Levski's Cube UI) and for the title use vanilla
font proper size instead of prerendered title. Buttons to be sinkable on click and a notch brighter on hover." Then:
"i just changed the background file to include the grid."

## The assets

| Delivered | Becomes | Size | Where |
|---|---|---|---|
| Levski Cube Background.png (320x352, the grid painted in) | `ui\cube_canvas.png` | as is | the window |
| Transmute Button.png (778x143) | `ui\cube_transmute.png` | 208x38 (ScalePainting.ps1) | (56,249) |
| Recipe Book Button.png (532x106) | `ui\cube_recipebook.png` | 142x28 | (89,293) |
| Levski Cube Full Design 2.png (1198x1313) | the assembled sample, filed as `cube_full_design_sample.png` | - | measured for the positions |

The sample is the canvas at 3.74x; its plate rims were measured by scanning for gold rows and columns
(title band y 32..68, TRANSMUTE 934..1068 / 213..990, RECIPE BOOK 1096..1196 / 335..865 in sample pixels).
The painted grid's silver rules are at x 115/144/173/202 and y 106/135/164/193/222 - a 29 px pitch with 28 px
cells inside, the Roar's own pitch, so the item sprites fit the painted squares without scaling. Cell (0,0) is at
(116,107). Everything is filed under `Resources\01-in-use-assets\ui\levski-cube-ui`.

## The code (`oracool/levski_roar.cpp`)

- `ListSkinGeometry` gains `recipes` (the RECIPE BOOK button's rect) and `title` (where the title is drawn);
  `CubeCanvasGeometry` is the new skin, worn by the Cube host whenever `ui\cube_canvas.png` loads, ahead of the
  artisan canvas and batch 43b's painting. Ogden's and Gillian's windows keep the artisan canvas.
- The title is the game's 24 px gold with a shadow, centred in the sample's band - no prerendered title.
- The two buttons are drawn from their paintings; under the cursor they brighten 15% (`BrightenRectRgb`), while
  pressed the face sinks 2 px down-left until LeftMouseUp (`ReleaseLevskiButtons`, called from diablo.cpp's
  mouse-up beside the other release hooks), and titlemov plays as the cursor arrives over a button and at each
  press - the standing button feel.
- No bezel list on this skin: RECIPE BOOK opens the tall recipe book beside the window, as the Roar's plate did.
  The book's drawing moved into `DrawTallRecipeBook` so both skins share it; the bezel list's wheel scroll and
  click loop are skipped on the painted skin.

## Not changed

The batch 43b painting (`cube_bg.png`) and its measured skin stay as the fallback; the artisan canvas stays for
Ogden and Gillian; the tall book's frame and rows are as before.

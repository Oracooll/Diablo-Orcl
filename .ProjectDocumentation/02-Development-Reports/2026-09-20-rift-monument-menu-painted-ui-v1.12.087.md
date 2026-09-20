---
date: 2026-09-20
version: 1.12.087
tags: [dev-report, rifts, ui, art, buttons]
---

# The Rift Monument menu in the user's painted UI (v1.12.087)

## The asks

> "Assemble Rift Monument UI with the assets in this folder: Resources\Rift Monument UI" and "when hovering over the buttons make them a notch brighter. when clicking on them sink them 2px down and left. hold until click released."

## The assets

`Resources/Rift Monument UI/`: `Rift Monument UI Background.png` (320x352, the canvas size, title and question painted in), `Nephalem Rift Button.png` (968x146), `Guardian Rift Button.png` (971x150, purple), `Guardian Rift Button RED.png` (979x152, "no keystone in the pack"), `Leave Button.png` (585x120), and two assembled samples (1195x1316) showing the intended layout. Filed under `Resources/01-in-use-assets/ui/rift-monument-ui/` as `riftmenu_*.png` and shipped as `ui\riftmenu_bg / nephalem / guardian / guardian_red / leave.png`.

The buttons are scaled by `tools/ScalePainting.ps1` to the sample's proportions (sample-to-canvas factor 0.268): the rift buttons 262x40 (the red face 262x41), Leave 157x32. Cells from the sample: rift buttons at x 29, y 123 and 174; Leave at x 82, y 224.

## The menu (`Source/oracool/stonegate_menu.cpp`)

- With the background present, the painted set draws: background 1:1, the red X at the canvas's corner, the three buttons over it. The Guardian button shows the purple face with a keystone in the pack and the RED face without; the click behaviour is unchanged (a click on the red face logs why).
- **Hover:** the button under the cursor is brightened 15% in place through a new `BrightenRectRgb` (primitive_render: each channel scaled and clamped, hue and saturation kept; on an indexed surface one shade up its PAL16 ramp).
- **Press:** the clicked button's face draws at its cell shifted by (-2, +2) from the press until `ReleaseStonegateMenuButton` (called from diablo.cpp's `LeftMouseUp`) or the menu closes.
- **Sound:** the move sound on every hover entry and every click, as on every other button given the feel today.
- The v1.12.069 artisan-canvas layout with code-drawn plates and text remains as the fallback when the background file is missing; a missing button file draws its old plate and label.

## Build

v1.12.087 built clean, ctest 831/831; the five riftmenu assets packed.

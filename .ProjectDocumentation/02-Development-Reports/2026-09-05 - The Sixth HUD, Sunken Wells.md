# The Sixth HUD, Sunken Wells (v1.9.240)

**Date:** 2026-09-05 · **Request:** "there is a new hud in oracool.mpq. use it" - GPT's delivery to the v6 brief (`oracool-hud-v6-sunken-wells.zip`, filed under delivered-packs with its archive).

## What arrived

`hud-v6.png` 1839x324 true alpha at exactly 3x, a `layout-manifest.json` with every rect, `hud-v6-liquid.png` (two opaque discs), component cuts, previews and GPT's own build script. The cradles are shorter (108 on screen), the wells are 56x56 openings with a 9-master-pixel inward alpha shadow (black, 255 → 0), the spheres empty glass.

## The cut

`tools/CutHudPlate.ps1` rewritten: it takes the manifest's numbers divided by three (the pack's README warns that alpha-thresholding the wells finds the shadow, not the edge), resamples the shell and the liquid to 613x108, and cuts both at x 112 and 499 into the five files. Inside each well opening the resampled alpha is shaped to what the engine can blend: >= 160 → 255 (the rim's own black), [30, 160) → 100, else 0 - one opaque pixel and two half-dark, three of shadow. Elsewhere alpha is binarised at 128 as before. Header: plate 387x108, wells {10,52,56,56} / {328,52,56,56}, belt cells 28x30 at the same x as the fifth HUD, `BeltBarTop` 67, spheres r 34 at (65,52) / (50,53).

## The code

- **A half-transparent layer per asset.** `ArtAsset::half`: pixels with alpha in [40, 128) quantised to their own colour; `DrawMiddleHudArt` blits it after the opaque pass through `BlitHalfTransparentSkipZero` (the palette's 50% table). Every asset gets the surface; only the plate has anything in it.
- **Draw order.** `scrollrt.cpp` draws the LMB well and `DrawSpell` (the RMB well) BEFORE `DrawMiddleHudArt`, so the vanilla plate and icon sit under the frame and the shadow falls on them.
- **The vanilla plate at 56.** Every well draw of a plate or a legacy icon uses `DrawLargeSpellIconCentredIn` - the 56px sheet's frame, centred on the 50px net, which puts it exactly in the 56px opening. `SkillWellNetSize` 46 → 50.
- The liquid comes from the delivered discs, no longer lifted from under the cradle's alpha.

## Look at

The wells: a spell plate sitting in a recess with a dark inner edge, the icon inside the 50. The half layer is a real blend, so the shadow's second and third pixels should read as darkened plate, not black. And the orbs' crowns are at 630 now, below the side panels' content line - the 4:3 clip only ever takes the arch tips.

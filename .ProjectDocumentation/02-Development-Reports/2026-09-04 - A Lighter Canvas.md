# A Lighter Canvas (v1.9.210)

**Date:** 2026-09-04 · **Request:** "the common 340x720 canvas seem a bit dark. can you brighten it up a bit?"

The dark-stone `ui\panel_bg.png` that every side panel shares was filed as its own master (`Oracool.MPQ/01-in-use/inventory-panel/panel-bg-340x720-dark-stone-master.png`, the file exactly as it shipped through v1.9.209) and `tools/BrightenPanelBg.ps1` now writes the shipped copy from it with a gamma lift.

Gamma 0.85: mean luma 79.7 → 93.9, about 18% brighter in the stone's body, blacks in the cracks untouched, nothing clipped. Re-run with `-Gamma` to taste; it always starts from the master, so lifts do not compound.

**v1.9.211** - "a notch brighter again": gamma 0.75, mean luma 79.7 → 105.1. The script's default moved with it.

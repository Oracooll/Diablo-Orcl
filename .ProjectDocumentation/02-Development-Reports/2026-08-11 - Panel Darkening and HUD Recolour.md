---
title: 2026-08-11 - Panel Darkening and HUD Recolour
date: 2026-08-11
tags: [dev-report, art]
summary: The inventory panel is darkened to match the original panel art, and the bottom HUD desaturated to sit with it. Both factors were measured rather than judged by eye, and in both cases the eye's first diagnosis was wrong.
---

# Panel Darkening and HUD Recolour

Two related art passes. Both are interesting mainly for how the numbers contradicted the obvious
reading.

## The panel was too light - but not by the amount it looked

The new inventory panel read much lighter than the original panel art. With the stash and the
inventory both on screen in one screenshot, under the same palette, the old panel's interior had a
median luma of 43 and the inventory's 100 - suggesting a factor of 0.43.

That was wrong, and the error is worth recording. **The inventory was full of items and the stash
was empty.** Item sprites and their quality-highlight blocks pulled the inventory's median up.
Applying 0.43 landed the panel at 32 - visibly *darker* than the original - and collapsed it from
41 palette colours to 18, which bands the stone.

Measuring the source composition, which contains no items, gives a true median of 74. The correct
factor is 43/74 = **0.58**, which lands on 43.0 exactly.

Lesson: measure the asset, not a screenshot of the asset in use.

The cost is real but acceptable: 41 palette colours down to 26. The Diablo palette simply has
fewer distinguishable entries in the dark range, so any darkening spends texture fidelity. 0.43
would have spent far more.

Implemented as `PanelDarken` in `tools/InvCompose.cs`, applied last over the whole composition so
background, silhouette, frames and sygil keep their relationships and only the level moves.

## The HUD was not too bright - it was too warm

Next to the darkened panel the bottom HUD read as "bronze/gold", which suggests it is too bright.
Measured, it is not. Every HUD asset is already darker than the panel:

| Asset | Median luma | Saturation |
|---|---|---|
| inventory panel (target) | 42.7 | **0.09** |
| middle_hud | 31.3 | 0.25 |
| health_orb | 24.8 | 0.82 |
| mana_orb | 34.6 | 0.82 |
| menu_icons | 24.0 | 0.73 |

The mismatch is entirely **saturation**. Darkening - the intuitive fix - would have widened the
gap. `tools/HudRecolour.cs` desaturates instead, keeping 36% of chroma (0.09/0.25), and does not
touch brightness at all.

## Where colour carries meaning, it is left alone

Two assets could not take a uniform treatment, because their saturation is not decoration:

- **The orbs.** Their 0.82 is the red and blue spheres themselves; draining those would destroy
  the health/mana read. Only the frame and statue *outside* the sphere are desaturated, using the
  same circle geometry the engine drains the orb against (`hud_layout.h` sphere centres and
  `OrbSphereRadiusPx`), with a 6px feather so no ring appears at the boundary. Overall saturation
  therefore barely moves (0.82 -> 0.78) while the frames change visibly - the metric is dominated
  by the protected spheres, which is exactly right.
- **menu_icons.** Its 0.73 is the three-state signalling. That sheet was chosen over a greyscale
  alternative *specifically because* it separates states by colour rather than brightness, which
  survives being drawn at 30px (see [[2026-08-11 - Burger Menu Icon Row]]). Left untouched, and
  should stay that way.

## Idempotence

`tools/hud_source/` holds a pristine snapshot of the HUD assets taken before the first recolour,
and `HudRecolour.cs` always reads from there. Transforming the installed files in place would
compound the desaturation on every run - a mistake that would be invisible for one or two runs and
then obvious and unrecoverable.

## Verification

`ORACOOL_VERSION` 1.0.93. Panel median luma verified at 43.0 against the original's 43.0; export
round-trip still stable at 26 colours. Recoloured orbs and plate checked at 3x against the
originals - frames neutralised, spheres untouched, no ring at the feather boundary.

## Related

- [[2026-08-11 - Asset Studio]]
- [[2026-08-11 - Burger Menu Icon Row]]

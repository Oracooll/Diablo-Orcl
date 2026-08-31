# Icons fill their plate (v1.9.67)

**Date:** 2026-08-27
**Version:** 1.9.66 → 1.9.67
**Tests:** 562, of which 560 pass — the two standing baseline failures, unchanged.

---

## The report

> "so make the new skills/spells icons the size of the vanilla background. i want them to completely
> overlap the background."

Raised the day after batch 01 landed, which is the clue: it is not that the icons were always wrong,
it is that **two kinds of icon are now on screen at once and they do not agree.**

## Measured first

Every frame of the Paladin strip, by opaque bounding box:

| frames | what they are | uniform transparent border |
|---|---|---|
| 0-8, 29, 30 | **batch 01**, the new colour icons | **4 px on every side** — opaque 48×48 in a 56×56 cell |
| 9-28 | the twenty aura icons | **0** — edge to edge, several fully opaque 56×56 |
| 31-48 | passives | empty |

So the new icons are inset by 4 px and the old ones are not. Scaled onto the same plate, the old ones
cover it and the new ones leave a ring of bare plate around themselves. That ring is the complaint.

## Three fixes were possible; only one was right

**Crop the strip to 48×48 cells.** Would have cut 4 px off every aura icon, and those run to the
edge — real artwork lost. Rejected on the measurement above.

**Upscale the new art to fill its cell.** A 48→56 resample of finished art, baked into the asset,
introducing colours outside the 192-207/255 set the batch was authored against. Rejected.

**Scale the artwork rather than the cell, at draw time.** Taken.

The icons were *already* being resampled at draw time — a 56 px cell has always been scaled to reach
a 46 px well. So this changes only **which source rectangle goes through the same single scale**, and
costs nothing in quality.

## What it does

`ArtAsset::cellInsets` records, per cell, the width of the uniform transparent border around its
artwork — measured from the loaded RGBA once, then remembered.

**Uniform, not the tight bounding box**, and that distinction is load-bearing. Aura frame 15's tight
box is 37 wide by 56 tall; stretched to a square plate it would come out visibly distorted. Taking
the same N off all four sides can only remove empty border and can never reshape what is drawn. For
edge-to-edge art N is 0 and nothing changes at all — which is exactly what should happen to the
twenty auras.

Measured rather than declared, so the next batch can arrive with a 6 px margin, or none, and need no
code change and no re-cut art.

## Three draw paths, not one

The first fix only covered the wells, and the Abilities sheet would still have shown the ring. The
sheet's cells scale the *plate* to the cell but were drawing the icon at native size — indis­tinguishable
from filling it for as long as every icon ran edge to edge.

- `DrawStripIconScaledTo` — the wells, the skill picker, the attack icons. Now insets the source.
- `DrawClassTreeIcon(Rectangle)` — the **Abilities sheet cells**. Now scales the icon to the cell
  instead of drawing it natively.
- `TryDrawSkillSpellIconLarge` — the speedbook's 56 px plate. Its tree branch now fills the plate too.

The scaled blit gained the half-transparent variant so that path is a drop-in for the native one.
Without it, scaling an Abilities row would have quietly drawn every **unearned** skill at full
strength — the greying-out is how the sheet says "not yet".

The legacy 38 px Paladin fallback strip is deliberately left centred on its larger plate: that is a
genuinely smaller icon, which is a different thing from one that only looks smaller.

## Sizes, for the record

The plate is the vanilla small spell icon, **37×38** native, recoloured through the game's own TRNs —
yellow, green, red, pink, grey are one sprite, not five. It is scaled to whatever it is drawn into:
**46×46** in the LMB/RMB wells (`SkillWellNetSize`), 56×56 on an Abilities cell. The icon now scales
to the same rect, so the two always agree.

# The Hellfire runes in vivid colours (v1.9.281)

**Date:** 2026-09-05
**Request:** "do the same for the hellfire runes" (after the oils, v1.9.280)

## What the five paint on

Unlike the oils, the five trap runes each have a sprite (cursor ids 193..197), but a probe of the archive showed all five as stone tablets in earth tones:

| Rune | Ramp in the sprite |
|---|---|
| Rune of Fire | orange (208..223) |
| Greater Rune of Fire | dusty rose (160..175) |
| Rune of Lightning | steel blue (176..191) body, gold bevel, grey runes |
| Greater Rune of Lightning | gold (192..207) |
| Rune of Stone | grey (239..253) |

Dusty rose next to gold next to grey is what read as "indistinguishable" in a grid.

## What was done

`oil_tint.{h,cpp}` became `Source/oracool/item_tint.{h,cpp}` and `OilTRN` became `ItemTRN`; the `Tint` row now names the source ramp as well as the target, so any consumable can join the table. Four rune rows were added:

| Rune | Now |
|---|---|
| Greater Rune of Fire | red (136) |
| Rune of Lightning | blue (128), the gold bevel kept |
| Greater Rune of Lightning | yellow (144) |
| Rune of Stone | green (152) |

Rune of Fire keeps its orange, already the loudest of the five. The draw sites are the same three as the oils (the shared item draw, the shop grid, the floor).

## Test

`OracoolItemTint.TheHellfireRunesLandOnFourVividRamps` - each moved ramp lands inside its eight-entry target, Fire returns no table, and each sprite re-read from the archive paints at least a third of its pixels on the ramp its table moves (Lightning is 289 of 597 because of the bevel and the runes).

# Item backings: seamless stone, teal runewords, ember primals (v1.12.051)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests**. The user's first look at v1.12.050 ("i like the result,
but a few adjustments"): no grid lines in a filled slot, a variety of stone rotated for diversity, and colours
that stand out for runewords and primals. Not yet seen in play.

## 1. The grid lines were mine

The v1.12.050 tile was the 28x28 at vanilla panel offset 17, 223 - and column 17 is vanilla's one-pixel cell
divider, so every 28 pixels of the tiled underlay drew a line. (The Orcl panel's own bezel is drawn BEFORE the
backing and is covered by it; it was never the cause.) The swatches are now the INTERIOR of the cells, two
pixels in from every edge: 24x24.

## 2. Variety

Four swatches (the interiors of four different vanilla backpack cells: origins {19,225}, {48,225}, {19,254},
{48,254}) in the eight square orientations (four rotations, each flipped or not) = 32 variants. The screen is
treated as a fixed plane of 24-pixel tiles; each tile's variant is a small integer hash of its tile coordinates
(`SlotStoneVariantAt`), so the stone is the same from frame to frame, two adjacent items continue one another's
texture, and no repeat is visible at grid scale. The whole vanilla panel is decoded once (`EnsureSlotStoneTiles`)
and the swatches copied out of it as palette indices.

## 3. Colours

- **Runeword**: teal `0x30C0B0` at the full 90% depth. Was the fork's green at 60% - a darker Set. No other backing
  is cyan; the gold rune rings read against it. Alternatives noted on the page: crimson 0xC03030, or the old dark
  green. Indexed fallback: the blue ramp.
- **Primal**: ember orange `0xF07820`. Was the beige ramp's hue (the BE-2 font ramp), a dull rose on stone; orange
  is the Primal's colour from before the beige move and is in no other backing (apart from the unique gold and the
  rare yellow by its red half). Alternatives: D3-style primal red 0xD83030, magenta 0xD040A0. Indexed fallback:
  the orange ramp.
- The socketed (white, 60%), ethereal, Set and the rest are unchanged.

## Verification

- 819 of 819; the ethereal test does not load the panel and is unaffected.
- Not verified: the look. If the 24-pixel plane still reads as a pattern, more swatches (there are forty cells to
  take from) is a one-line change to `SlotStoneSwatchOrigins`.

## Related

- [[2026-09-19-stone-underlay-backings-v1.12.050]] - the underlay this refines.

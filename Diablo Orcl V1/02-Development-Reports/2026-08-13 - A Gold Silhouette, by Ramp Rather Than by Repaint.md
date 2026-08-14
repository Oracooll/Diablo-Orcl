---
date: 2026-08-13
version: 1.1.68
area: UI / Inventory art
---

# A Gold Silhouette, by Ramp Rather Than by Repaint

## What was asked

> Can you make the silhouette goldish tint - closer to unique's backing?

## Why the obvious approach was wrong

The silhouette is a neutral grey cut-out (`ui\silhouette_paladin.png`), quantized to the game's
palette by `QuantizeAsset`'s `NearestGlobalPaletteIndex`. Tinting it by recolouring the PNG would
work, but it puts the colour decision in the asset - which means a repack to change it, a second
copy per class when the other five silhouettes are cut, and no relationship to the palette entry
the unique backing actually uses.

More to the point, nearest-palette matching *cannot* tint. Given a grey pixel it will faithfully
find the nearest grey. The hue has to come from somewhere other than the source.

## What was done

`QuantizeAsset` gained an optional tint ramp. When set, a pixel's own hue is discarded and only its
luminance is used, mapped onto one of the palette's 16-shade ramps:

```cpp
uint8_t RampIndexFromLuminance(uint8_t rampBase, uint8_t r, uint8_t g, uint8_t b)
{
    constexpr int DarkestOffset = 14;
    constexpr int LightestOffset = 4;
    const int luminance = (299 * r + 587 * g + 114 * b) / 1000;
    return rampBase + DarkestOffset - luminance * (DarkestOffset - LightestOffset) / 255;
}
```

The silhouette is quantized with `PAL16_YELLOW`. Shape and shading still come from the art; the
colour now comes from the game.

### Two numbers that are not arbitrary

**Brighter luminance takes a SMALLER offset.** PAL16 ramps run light to dark as the offset grows -
engine/palette.h states it outright: "(dark blue): PAL16_BLUE+14, (light red): PAL16_RED+2". This
is the direction that has been got backwards twice before on this project (most recently the item
backings, where "darker" was moved the wrong way), so it is worth restating at every use.

**The range is 4..14, not 0..15.** The top of a ramp is bright enough that the figure reads as a lit
object rather than a tinted one, and the request was a tint. Centring near +9 also places it in the
same stretch of the ramp as the unique-item backing, which `InvDrawSlotBack` blends at
`PAL16_YELLOW + 10` - that shared neighbourhood is what makes the two read as one family rather
than as two unrelated golds.

The draw path is unchanged: still `BlitHalfTransparentSkipZero`, so the final pixels are gold
blended with the panel behind, not flat gold.

## The 1px outline (v1.1.66)

> Can you also outline the silhouette with thin 1px gold outline?

`BuildOutline` fills a second surface with a one-pixel band tracing the figure, and
`DrawClassSilhouette` blits it **opaquely, after** the blended body.

**Opaque is the whole point.** The body is drawn half-transparent so it takes the colour of the
panel behind it - that is what makes it a silhouette. Blending the outline too would sink it into
that same muted gold and there would be nothing to see. The edge is the one part that must not
blend.

Three details worth keeping:

- **Outside the figure, not inside.** An inner outline would eat a pixel of an already small
  figure, and on thin features - fingers, a weapon haft - it would eat the feature entirely.
- **Eight-neighbour, not four.** With a four-neighbour test the band breaks into dashes wherever
  the edge runs diagonally, which on a human figure is most of it.
- **`PAL16_YELLOW + 2`**, near the light end of the same ramp the body uses. The outline has to
  out-read a body that is both half-transparent and mid-ramp, and at one pixel wide, bright is what
  makes it read as an outline rather than a smudge.

The band lives in the source bitmap, so a figure touching the bitmap's own edge has no room for an
outline there. The cutter leaves a transparent margin, so this only bites if the art is recut tight
to the subject.

## The clipped feet (v1.1.67)

> Shrink the silhouette 5% to fit his feet as well above the inventory grid, because their outline
> is cut. Dial down the brightness of the outline.

Both confirmed in the screenshot: the outline ran down the legs and stopped dead at the ankles with
no band closing under the boots, and the edge read as near-white.

**The cut feet were the caveat in the section above, arriving.** `CutClassSilhouette.ps1` scaled the
tight-cropped figure to exactly `TARGET_H = 370` and wrote a canvas exactly 370 tall - so the figure
touched the canvas on every side, and `BuildOutline` had no row to draw into anywhere along those
edges. The feet were simply where it showed.

So the fix is two things, and the second matters more than the one that was asked for:

- `SUBJECT_SCALE = 0.95` - the requested shrink. The figure is now 352 tall in the same area, so the
  feet clear the tab row by about 30px instead of 14.
- `MARGIN = 2` - a transparent border on every side, which is what actually guarantees the outline
  has somewhere to live. Without it, a 5% shrink alone would still have produced a figure flush to
  its canvas.

The border is asserted rather than assumed: the cutter scans all four canvas edges after the
resample and throws if any pixel came back opaque. A bicubic resample that overshot its destination
rect would otherwise silently put figure pixels back on the edge and quietly restore the bug.

Result: subject 236x352 inset 2px in a 240x356 canvas, border clear.

**Outline brightness** was walked down the ramp over two passes: `PAL16_YELLOW + 2` read as
near-white, `+ 6` was still hot, and it settled at **`+ 9`** (v1.1.68).

The reasoning behind +2 was sound but the conclusion was not. The outline does not need to be
bright to be legible, because it is drawn opaque against a body that has been halved into the
panel - being solid is what separates it, not being light.

+9 is also the principled place to stop rather than a third guess: it sits one step off the
unique-item backing's own `PAL16_YELLOW + 10`, so the silhouette's brightest element is exactly the
tone the whole request started from. There is no risk of the two being confused where they meet -
the silhouette is drawn between the panel fill and the equipment slots, so a slot's backing covers
it rather than abutting it.

## Cost

The tint and the outline are runtime properties, so the other five class silhouettes inherit them
for free when they are cut. The re-cut needed an MPQ repack: `oracool.mpq` is searched before every
other archive (assets.cpp's `FindMpqFile`), so a stale copy there would have shadowed the new PNG.
Repacked, 59 files; the silhouette's packed length matches the cut file's on disk exactly (73703
bytes).

## Files

- `Source/oracool/hud_art.cpp` - `RampIndexFromLuminance`, `BuildOutline`, the `outline` surface on
  `ArtAsset`, the optional `tintRampBase` parameter, and the silhouette's quantize and draw calls.

## Verification

Debug config builds clean at `1.1.66`. Not yet seen in game.

Two tuning knobs:
- Body: `DarkestOffset` / `LightestOffset` in `RampIndexFromLuminance`. Lower is brighter gold,
  higher is deeper; a narrower span flattens the internal shading, a wider one increases it.
- Outline: the `PAL16_YELLOW + 2` at the `BuildOutline` call site. Larger offsets are deeper gold
  and will read as a quieter edge.

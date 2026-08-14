---
date: 2026-08-13
version: 1.1.71
area: UI / HUD art
---

# Tinting the HUD Plate Without Repainting It

## What was asked

> Now that our UI theme is predominantly goldish I say we apply goldish tint on the main HUD.

## Tint, not repaint

The silhouette's gold came from `RampIndexFromLuminance`, which throws the source's hue away and
puts every pixel on one 16-shade ramp. That is right for a flat cut-out whose only job is to be a
shape - and wrong here. The HUD plate is modelled art with highlights, recesses and its own
existing metalwork; ironing all of it onto a single ramp produces one flat sheet of gold with the
detail gone.

So the ramp mapping gained a strength, and `TintedPaletteIndex` blends toward it rather than
snapping to it:

```cpp
const SDL_Color &target = orig_palette[RampIndexFromLuminance(rampBase, r, g, b)];
return NearestGlobalPaletteIndex(mix(r, target.r), mix(g, target.g), mix(b, target.b), cache);
```

The destination is taken **from the palette**, so it is exactly the gold the rest of the UI is
already using rather than an invented one. Only the distance travelled is a parameter. At 100 it
degenerates to the old full remap, so the silhouette's behaviour is unchanged and there is one code
path rather than two.

The plate is tinted at **50%**: unmistakably in the theme, with the stone still reading as stone.

## The burger menu (v1.1.71)

> Now tint also the burger menu.

Both halves of it, because the phrase covers both and each has a claim: the ☰ button
(`ui\burger_menu_button.png`) sits on the HUD that was just tinted, and the popup it opens
(`ui\menu_icons.png`) is the menu itself. All three assets now share one
`HudTintStrengthPercent = 50`, so they cannot drift into different golds - reading as one surface is
the entire point.

`menu_icons.png` is the one sheet where chrome and content share pixels: each cell is a frame with
its pictogram baked inside, so tinting the frame necessarily tints the glyph. That is survivable
**only** because the tint is partial. At 50% each pictogram keeps half its own hue, so the ten
entries are still told apart by colour as well as by shape. At full strength they would collapse to
one gold and the row would read as ten identical buttons - which is a concrete reason not to reach
for 100 here even if the plate ever wants it.

## What was deliberately left alone

**The orbs.** Red and blue there are not decoration - they are how the player reads health and mana
at a glance, and a gold wash over either would cost real information for a cosmetic gain. Their
ornament also already sits warm against the gold, so there is less to gain than it first appears.

If the orb *frames* alone are wanted in gold later, that is a genuinely different change: the tint
would have to reach `frame` while sparing `sphereDim`, and `bright` (which carries the whole orb,
glass included) would need splitting or skipping. Worth knowing before anyone assumes it is a
one-line follow-up.

**The button icons.** Belt potions, the portal, the readied spell - all content rather than chrome.

## Files

- `Source/oracool/hud_art.cpp` - `TintedPaletteIndex`, the `tintStrengthPercent` parameter, and
  `PlateArt`'s quantize call.

## Verification

Debug config builds clean at `1.1.70`. Runtime-only: `ui\middle_hud.png` is untouched, so no
re-cut and no MPQ repack.

Not yet seen in game. The knob is the `50` at the `QuantizeAsset(PlateArt, ...)` call site - higher
is more gold and flatter, lower is subtler and keeps more of the original art.

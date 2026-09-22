# Her icon row moves beside the frame, and Recharge becomes a pump — v1.12.154

2026-09-22

Two instructions, both the user's, both landing on the same row of buttons.

## The pump

> use the pump instead of the star for recharge.

I had left it out of v1.12.153 as an anachronism — a petrol pump is roughly eight centuries
early for Tristram — and said so. The user's answer stands, so `shop_glyph_recharge.png` is
the pump now, cut by `tools/CutShopGlyphs.ps1` with the same chroma recipe as the rest.

**Gillian's Imbue did not follow it.** Her Imbue borrowed Griswold's Recharge glyph at the
user's own instruction ("Imbue uses Recharge icon from Griswold"), back when that glyph was a
star. A fuel pump is not what working a shard into an item looks like, so the star is now
`shop_glyph_imbue.png` and her button points at that. The picture on her button did not
change — only which file it comes out of, which is exactly what makes it reversible if the
user would rather the two shared again.

## The row

> move the icons of tabs 1 2 3 of gillian flush with lower border of smaller frame, 6px away
> from smaller frame, distributed parallel to the top bezel of the bigger frame.

Three requirements, one place:

- **flush with the lower border** — the plates' feet sit on y 280, the small frame's outer
  lower edge;
- **6px away** — the rightmost plate ends six pixels clear of x 207, its outer left edge;
- **parallel to the big frame's top bezel** — a horizontal row, spread across the floor to the
  left of the frame rather than packed against it.

There is no room *under* the small frame: its foot is at 280 and the big frame's painted bezel
begins at 289, eight pixels. Beside it is the only placement that satisfies all three at once,
and it is the one that puts the row on the same line as the thing it acts on.

The layout is **distributed**, as asked, rather than laid out from a fixed gap. The first plate
starts at the canvas's own inner edge (x 22), the last ends at x 200, and the rest are spaced
evenly between: for the three-plate Imbue row that is x 22, 95 and 167. A row of one is centred
in the same band at x 94, so Reroll's plate and Craft's Transmute stand on the middle plate's
place and nothing jumps as the tabs change.

`ServiceIconGap` survives only as the width the price line may overhang into. It no longer
places anything.

### The price line moved above the plate

It is below the plate everywhere else in the game and it cannot be here: the row's feet are at
y 280 and the bezel starts at 289, so a line under the plate would sit at y 282..295 — half on
the painted moulding. Above it goes, at y 231..244.

The overhang is also **symmetric and shrinks to whatever both sides can spare**. The leftmost
plate of the row stands on the canvas's inner edge, and a box clamped on one side only would
still be centred on itself, putting its number a few pixels right of the plate it prices.

## Files

- `Source/oracool/workshop.cpp`
- `Source/oracool/shop_grid.cpp` — untouched this build; the pump arrives through the asset
- `tools/CutShopGlyphs.ps1` — the pump added to its job list
- `Packaging/resources/oracool_assets/ui/shop_glyph_recharge.png` (replaced — the pump)
- `Packaging/resources/oracool_assets/ui/shop_glyph_imbue.png` (new — the star, preserved)

Asset changes, so `oracool.mpq` was repacked.

## What to look at in play

1. Griswold's service row: Recharge is a pump.
2. Gillian's Reroll, Imbue and Craft tabs: the plates sit on the small frame's lower line, the
   last one six pixels clear of it, prices above.
3. The one-plate tabs (Reroll, Craft) put their button in the middle plate's place, so flipping
   between her first three tabs should move the picture but not the button.

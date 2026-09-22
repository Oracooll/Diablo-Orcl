# The Icons folder, triaged — and a green square put right — v1.12.153

2026-09-22

> check this folder - "Resources\Icons". see what you can use and where.

Five 56x56 glyphs, white art on chroma green. Three are in; two are not, and the reason is
worth writing down.

## The bug the folder uncovered

`shop_glyph_transmute.png` was installed on 2026-09-22 by scaling the source straight down
to 28x28. The source is white on chroma green, so what shipped had **zero transparent pixels
and 520 green ones** — a bright green square on Griswold's button plate, on the Transmute
button of three windows: Levski's Cube, Ogden's Craft tab and Gillian's Craft tab.

Nothing caught it because a glyph that draws is a glyph that looks like it works. Every other
chroma cut in `tools\` keys the green out at source resolution first; that one did not go
through a script at all.

So there is a script now — `tools\CutShopGlyphs.ps1` — carrying the same recipe the rest of
the folder uses:

1. green to fully transparent **at source resolution**, before any resampling, so the chroma
   edge is never averaged into the art by the resampler;
2. trim to the ink's bounding box;
3. fit the longest side to 21px and centre it in a 28x28 ARGB canvas — which is what every
   glyph already in `ui\` measures (refresh 21x20, recharge 21x20, sell 21x19, repair 18x21).

It then **refuses to write** a file that still has chroma in it or that has no transparent
pixel at all. That is the check the first install did not make.

## What went where

| Source | Asset | Button |
|---|---|---|
| Transmute heart potion | `shop_glyph_transmute.png` (replaced) | Levski's Cube, Ogden's Craft, Gillian's Craft |
| Reroll — single die | `shop_glyph_reroll.png` (new) | Gillian's Reroll |
| Reroll — two dice | `shop_glyph_refresh_until.png` (new) | Griswold's Refresh-until |

**The single die to Gillian.** Her Reroll wore Griswold's restock arrows because his was the
nearest thing on the shelf, and the two services are not the same thing: his refreshes a
shop's stock, hers gambles one affix on the item in front of her. A die says the second.

**The two dice to Refresh-until.** That button had no glyph of its own — the table says so in
as many words — and wore Refresh's, so two buttons on one painting carried the same picture
and were told apart only by position and hover text. Refresh keeps the circular arrows for
one restock; the pair of dice rolls again and again until the stock answers.

## What was left out, and why

**The gas pump and the hose nozzle.** Both are legible ideas for "recharge" and neither
belongs in Tristram: a petrol pump is roughly eight centuries early. The nozzle is also the
one glyph of the five that does not survive the trip down to 28px — its ink is 46x30, so it
fits to 21x14 and reads as a grey smear on the plate.

They were offered as replacements for the Recharge glyph, and that glyph *is* weak — it is
currently a plain five-pointed star, which says nothing about charges. The fix is a different
drawing, not these: a lightning bolt over a wand, or a wand with a filling bar. Left as it is
rather than made anachronistic.

**A related gap, not filled here.** Gillian's Imbue borrows that same Recharge star. Whatever
replaces it will be serving two different services on two different paintings, so Imbue
probably wants its own glyph too — a shard going into a socket, or a gem over an item.

## Files

- `tools/CutShopGlyphs.ps1` (new)
- `Packaging/resources/oracool_assets/ui/shop_glyph_transmute.png` (replaced — the fix)
- `Packaging/resources/oracool_assets/ui/shop_glyph_reroll.png` (new)
- `Packaging/resources/oracool_assets/ui/shop_glyph_refresh_until.png` (new)
- `Source/oracool/shop_grid.cpp` — Refresh-until points at its own glyph
- `Source/oracool/workshop.cpp` — Gillian's Reroll points at the die

Asset changes, so `oracool.mpq` was repacked.

## What to look at in play

1. Any Transmute button — Levski's Cube, Ogden's Craft, Gillian's Craft. Green square gone.
2. Griswold's service row: Refresh and Refresh-until now carry different pictures.
3. Gillian's Reroll tab: a die rather than his arrows.

# Levski's Roar wears the painted skin (v1.9.203)

User, 2026-09-04: "an image Levski's Roar UI Background.png 2.5MB will appear in oracool.mpq. Place
it as Levski's Interface. I want to try it out. Also in 10min onedrive will deliver 10 more files
with hover and click states for the 10 buttons there are in this background image. Make all of
these work somehow."

## What arrived

A 1122x1402 painting of the whole window - title, 3x4 grid, a SALVAGE column of seven, TRANSMUTE,
Recipe Book, an X - with every label baked in. And ten 1536x1024 sheets, one per button, each two
plates on a grey gradient: **left lit, right plain.** Delivered as UUID filenames; filed by content
as `Oracool.MPQ/02-source-art/delivered-packs/oracool-levski-roar-skin/`.

Three sheets - Basic, Transmute, Ethereal - have the left plate running off the left edge.

## The two problems, and "somehow"

**The painting's grid cells are ~185px.** The game's item cell is 28px and every item sprite is cut
to it. Drawing the window at the scale where the painted cells are 28px would make it 170px wide.
So the window is drawn at 84/185 - painted cells become exactly 84px, three item cells - and
**items in the grid are drawn at 3x**, nearest-neighbour, through a scratch surface. The footprint
rules are untouched: a 2x3 armour still covers 2x3 cells; they are bigger cells.

**The state sheets are not sprites.** Each plate sits on a gradient with no alpha. The cutter finds
each plate as the bounding box of its gold - border, ornaments, lettering, which nothing in the
gradient is - then keys the gradient out of that box (light and unsaturated becomes transparent)
and resamples the plate to its button's on-screen rect. For the three cut-off sheets the right
plate serves both states and the game marks hover with the theme's outline instead.

## The pieces

- `tools/CutLevskiRoarSkin.ps1` - reads the pack, writes `ui\levski_bg.png` (509x637) and twenty
  `ui\levski_<button>_<hover|pressed>.png`, and **generates `Source/oracool/levski_roar_skin.h`**:
  window size, cell size, grid origin, the ten rects, the asset stems, and which hover files are
  plain. The art and the hit rects come from one pass and cannot drift. Re-run after any change.
- `oracool::DrawLoosePng` / `GetLoosePngSize` in hud_art - a ui\ PNG looked up by path from a
  cache, through the same load-quantise-blit path every declared asset takes, requantised when the
  palette moves. Twenty-one files as twenty-one globals threaded through `EnsureLoadedAll` and
  `NeedsQuantize` by hand was the alternative.
- `levski_roar.cpp` draws STATE only now: the painting, then items at 3x (grey when unusable, red X
  when broken, stack badge, quality outline on hover), then a state plate over any button under the
  cursor or mid-press, and a half-transparent shade over a plate that would do nothing right now
  (Transmute with no recipe ready, a salvage tier with nothing to consume) - the readout the old
  gold-vs-whitegold label carried. The close button is the painting's X. The recipe book page is
  unchanged.

## Not done, and why

- The socket overlay is not drawn on grid items: its gem dots are placed for a 1x sprite. The hover
  panel still names the gems, which was the 2026-09-03 request.
- The item art's baked shadows (index 0) drop at 3x - a smaller wrong than a black halo three
  pixels wide.
- Transmute's pressed plate carries the lion ornament under it and is squashed into the rect.
  Regenerating that sheet without the lion, or with the left plate intact, would fix both.

624/625, the standing baseline. 449 files in the archive, from 428.

## To look at in play

Open Levski's Roar. Hover each plate: nine light up, three (Basic, Transmute, Ethereal) get the
gold outline instead. Click: the plain plate flashes. Put a ring and an armour in: 3x, with the
quality outline on hover and the stack count in the corner. Is the window too big at 509x637?

## Addendum, v1.9.204 - regular game size

The user, on seeing it: "scale the whole thing down so the grid slots are regular game size - 3x4 28x28px."

One number in the cutter (`$gameCell` 84 → 28) and a re-run: the window is now **170x212** (scale 28/185), the grid 3x4 of 28px cells at (17,65), and every plate rect followed - close 11x10, transmute 77x18, recipes 53x17, the seven salvage plates 43x12 each.

In `levski_roar.cpp` `ItemScale` became 1 by itself, and at 1 the grid now takes the ordinary path - `DrawItem` (shadows, grey, red X, stack count as the backpack draws them), `ClxDrawOutline` and the socket overlay on the hovered item - instead of the nearest-neighbour blit, which stays behind `if constexpr` for any future scale other than 1.

Known at this size: the plates' painted lettering is 3-4 pixels tall and not readable. The hover text still names each button, and the hover/pressed plates still lay over it. If that reads as too small the number to move is `$gameCell` in `tools/CutLevskiRoarSkin.ps1` - 56 would give 2x cells and a 340x424 window.

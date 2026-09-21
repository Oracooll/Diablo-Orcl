# The stash loses its title, its tint and its shadows — v1.12.123

**Date:** 2026-09-21
**Version:** v1.12.123
**Branch:** renderer-32bit

## The request

Five lines, all of them about the stash the user had just looked at in play after
[[2026-09-21-stash-redesign-v1.12.122]]:

> remove the title of stash.
> remove the tint.
> remove the shadows of the nav arrows
> move the sort button under the gold counter.
> make the grid a bit transparent.

All five went in as written. Four are subtractions, which is the shape of this pass: the storeroom
canvas arrived last build and every one of these notes is about something drawn *over* it.

## What changed

### 1. The title is gone

`DrawStash` drew `_("STASH")` outlined at FontSize30 in the shared `PanelTitleTop` band. Removed.

Nothing moved with it. Since v1.12.122 every control in this window is placed from the painted grid
frame (`StashFrameTop`/`StashFrameBottom`) rather than from the title, so the band it vacated is
simply stone. The top-of-file layout map was rewritten to say so — it still described three stacked
bands under a title, a layout that stopped existing a build ago.

This matches Griswold, whose tabs lost their own name earlier the same day.

### 2. The tint is gone — from this canvas only

`DrawStashCanvasArt` (hud_art.cpp) called `DrawSidePanelDim` after the painting. Removed, along with
the `FrameKeptClear` rect it took: with no dim there is no hole to cut in it.

**Scope.** The other canvases keep theirs. The shared dim's own note — *"reduce it to one pass and
apply to all canvases"*, 2026-09-06 — is a decision about those, and `project_canvas_dim_settled`
says not to retune it unasked. This is the one window the user asked about, so this is the one
window it came off.

The fallback path is untouched: with no `ui\stash_canvas.png` the window falls through to
`DrawSidePanelGridArt`, which still dims.

### 3. The nav arrows lose their drop shadow

`UiFlags::Shadowed` off the four `<<  <  >  >>` labels.

The shadow was added when these sat on open stone. They have not since 2026-09-04 — they sit in a
legacy text box's dark field, and since v1.12.122 that box is 16px tall with a 3px bevel, leaving a
10px face for a 12px glyph. A drop shadow on a glyph already overhanging its own face is what made
them read as smudged rather than lit.

SORT and the gold total keep theirs. Those two *are* on open stone.

### 4. SORT moves under the gold counter

It was flush right on the gold's line — the mirror of the inventory's SORT-left/gold-right header,
and the compromise v1.12.122 settled for when SORT could not fit on the row above the frame.

Now it takes the gold's own x and the line below it:

```cpp
constexpr int StashSortRowY = StashGoldCountY + StashGoldRowHeight;
constexpr Rectangle StashSortButtonRect { { GoldDisplayRect.position.x, StashSortRowY },
	{ 3 * StashCellPx, StashGoldRowHeight } };
```

Left-aligned now (it was `AlignRight`), because a right-aligned word under a left-aligned number is
two controls, not one. Width unchanged at three columns — the width is the click target, and a
target that ends exactly where its own glyphs do is one the player misses from either side.

The pair now runs pile (633..660) → total (660..676) → SORT (676..692) down the panel's left edge,
which is 28px clear of the panel's foot and clear of the health orb (the orb starts around x 175;
this block ends at x 109).

**The asserts changed with it.** The two that guarded the old row tested a *horizontal* collision
between the gold readout and SORT. Stacked, that test is meaningless — it would pass forever while
the two sat on top of each other. Replaced with the vertical one that now matters, plus a check that
the block finishes inside the panel:

```cpp
static_assert(GoldDisplayRect.position.y + GoldDisplayRect.size.height <= StashSortButtonRect.position.y, ...);
static_assert(StashSortButtonRect.position.y + StashSortButtonRect.size.height <= StashPanelSize.height, ...);
```

This is the same trap as the 2026-09-03 gold-row assert that passed while the row was drawn on the
grid's frame: an assert keeps testing whatever it was written to test, including after the thing it
tested stopped being the risk.

### 5. The grid is a bit transparent

`DrawThemedFill(out, gridRect, 2)` → `DrawThemedFill(out, gridRect, framedCanvas ? 1 : 2)`.

Each pass is the same half-transparent blend, so two leave roughly a quarter of what is underneath
and one leaves roughly half. Over the storeroom painting that is the difference between the grid
being a dark plate and the grid being a grid drawn *on* the room.

Conditional on the canvas, and that is not hedging: with no canvas the fill is not covering art, it
*is* the grid's face. One pass there would give a paler grid on grey stone rather than a transparent
one, so the fallback keeps its two.

## Files

- `Source/qol/stash.cpp` — title removed, layout map rewritten, nav `Shadowed` dropped, SORT rect and
  alignment moved, asserts rewritten, grid fill made canvas-conditional.
- `Source/oracool/hud_art.cpp` — `DrawStashCanvasArt` no longer dims. **CRLF file** — edited with the
  Edit tool, per `project_crlf_files_and_inplace_edits`.

No asset changes, so no MPQ repack.

## Still open from v1.12.122

- The nav buttons are 16px tall around a 12px glyph because "font16" has no face in this engine
  (tables are 8/9/10/11/12/22/24/30/42/46). Dropping the shadow should help how they read; 18 tall
  would fit the glyph outright if they still look cramped.
- Griswold's gold pile (y 627..654) and his Refresh-until button (627..660) still overlap his
  canvas's frame bottom band, which ends at 628, by 2px. Under the same 4px rule the stash now
  follows, both would go to y 633.

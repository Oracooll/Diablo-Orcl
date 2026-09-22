# Ogden's nine, part two: his recipes stay home and he gets a bench — v1.12.140–141

**Date:** 2026-09-22
**Version:** v1.12.141 (v1.12.140 failed to link; see below)
**Branch:** renderer-32bit

Items 6 and 7 of the user's nine, closing the list.

## 6. The Recipes tab stays in his window

> Tab recipes must not lead to Levski Cube - must lead to Ogden's recipe canvas and list recipes.

It used to close the workshop and open the Levski page: a different window, a different painting and
its own tab column, leaving the player two clicks from the board they started on.

Now it is a page here. His recipe canvas, measured when it arrived (opening x 29..310, y 299..618),
with the six recipes `HostOfRecipe` hands him listed on the same two-pass dark layer Levski's page
uses. **Two lines each** - the name in gold, the inputs in white - because "Punch Sockets" alone does
not say what to put on the bench.

The MYSTIC keeps the hand-off. She has no recipe page of her own here, and sending her to the shared
book beats a tab that shows nothing.

## 7. The Craft tab

> There must be another tab - Craft, which uses Ogden Cube Canvas and where user performs other
> Ogden crafting recipes, not possible in his other tabs.

His cube canvas, its grid frame measured on 2026-09-21 and reused exactly: **3x4 at (128,416)**, 28px
cells on a 29px pitch. Griswold's Refresh plate as Transmute at the frame's centre four pixels below
its foot - the same plate and the same derivation Levski's Cube uses, because the user asked for that
button by name on both.

**One item per cell, whatever its footprint.** This is the bench's own rule (2026-09-21: "a 2x3 slot
that holds exactly one item whatever its size") applied twelve times, and it is exactly the format
the crafting API wants: `CanCraftFromLevskiGrid` takes an `Item` array indexed by anchor, and the
multi-cell footprint packing the Cube does is a drawing nicety on top of that rather than something
the recipes read. Items are drawn with `DrawSpriteToFit` so a 2x3 sword does not cover half the board.

`FirstReadyLevskiRecipeFor(..., TransmuteHost::Tavern)` - **his** recipes only. A grid that happens to
satisfy one of Griswold's would do nothing here; a recipe firing at the wrong artisan's window is a
bug that looks like a feature.

The grid is returned to the player when the window closes, backpack then stash, and the window stays
**open** when there is nowhere to put it - the same answer the bench gives. It is never persisted, so
a crafting station stays out of the save format entirely.

## Three mistakes worth recording

**`Tab4` inserted after `Tab0`.** Every handler derives its slot as `control - Control::Tab0`, so
placing the new value second silently renumbered the whole column - and the comment I wrote claimed
the opposite of what the code did. Tab slots must stay contiguous and in order; the enum now says so.

**A `/g` that was too broad.** Adding the Transmute plate to the control walks also added it to
`DrawBoardMessage`'s YES/NO loop, which draws a plate per entry - it would have rendered Transmute as
a "NO" box. This is the mirror image of the v1.12.134 bug where a substitution *without* `/g` patched
one of two twins: same class of error, opposite direction. A substitution's scope is a decision, not
a default.

**The bench gate was still too loose.** `!IsStockTab(OpenTab)` was correct while stock tabs were the
only alternative; with Craft and Recipes here it would have drawn the Mystic's 2x3 slot over Ogden's
cube grid and his recipe list. Both the draw and the click now name her two tabs explicitly. The same
audit finding as v1.12.136's invisible bench, one tab set later - a gate written as "not the other
thing" rots the moment a third thing exists.

## The failed link

**v1.12.140**: `workshop.cpp` has TWO anonymous-namespace blocks (41..1173 and 1565..1884), and
`ReturnCraftGrid`'s definition sits *between* them at `oracool` scope. My forward declaration went
inside the first block, which declares a different function with the same name - internal linkage
against external. Moved to `oracool` scope beside `CloseWorkshop`.

Second time this session an anonymous namespace ate a declaration (v1.12.130 was `GetSideTabRect` and
`DrawSideTab` defined *inside* one while declared in a header). The rule: in a file with more than one
`namespace { }` block, "declare it near where it is used" is not enough - it must land in the same
block as the definition, or outside both.

## Files

- `Source/oracool/workshop.cpp` — `Tab::Craft`, `Control::Tab4`, `Control::Transmute`, `CraftGrid`
  and its helpers, `DrawCraftPage`, `DrawRecipesPage`, the per-tab canvas, the hand-off removal.

No asset changes - both canvases were packed at v1.12.133.

## The nine, closed

| # | | |
|---|---|---|
| 1 | Titles up 40px | v1.12.139 |
| 2 | Griswold's gold on every tab | v1.12.139 |
| 3 | Gems/Runes see the stash; landing reported | v1.12.139 |
| 4 | Same for Jewels | v1.12.139 |
| 5 | Griswold's tabs | v1.12.139 |
| 6 | Recipes tab in his window | **v1.12.141** |
| 7 | Craft tab | **v1.12.141** |
| 8 | His welcome audio | v1.12.139 |
| 9 | Jewels 5x3 board, icons at 80% | v1.12.139 |

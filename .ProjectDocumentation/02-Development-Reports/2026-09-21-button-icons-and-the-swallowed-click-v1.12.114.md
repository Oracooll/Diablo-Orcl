---
version: v1.12.114
date: 2026-09-21
area: UI / Griswold's shop
tests: 832/832
---

# The user's own button icons, and the click the store router was eating

## The asks

> take the new icon set "004. Button Icons.png" and replace current icons in Griswald Shops. The order of icons
> is Repair, Repair All, Recharge, Sell, Sell All, Refresh.

> There is a bug with Salvage tab - when i click on it to enter it, once here, i can't click on any of the other
> tabs or the icons inside Salvage tab. [...] I can click on ground and walk away to close it.

## The swallowed click - a regression from v1.12.113

Making the Salvage tab a store screen put it inside the store's click router in `LeftMouseDown`, and that branch
ends:

```cpp
    CheckStoreBtn();
    return;
```

`CheckStoreBtn` speaks for the vanilla text box, which this page does not use. So every click landing on the
panel was swallowed there and never reached `CheckLevskiRoarClick` further down the function: the salvage icons,
the confirmation buttons and the tab column were all inert.

**The ground still worked, and that asymmetry is the whole diagnosis.** A click outside the panel fails
`IsPointOverShop`, skips the branch entirely, and reaches the world routing - so the player could walk away but
could not press anything. The user's report described exactly that split.

The page's own handler now runs inside the branch, before `CheckStoreBtn`.

The guard needs BOTH halves - `IsShopTab && !IsShopGridScreen`. The outer test is also taken by the towner
dialogs, which are not tabs at all, and asking only "not a grid" would have handed their clicks to a Levski
window that happened to be open behind one.

## The icons

A 336 x 56 sheet, six 56 x 56 frames, white marks with black outlines on a pure `#00FF00` ground - the project's
cut-out key again. Measured before cutting: 13947 keyed pixels, **zero** green-ish fringe, and exactly **two**
subject colours (`#ECECE8` and black). So the cut is lossless.

**The glyph is 28 x 28 now, not 24.** Half of 56 lands on a whole pixel, so the reduction is an exact 2:1 box
average and the strokes stay crisp; 56 -> 24 would have aliased them. In a 34 px frame that leaves a 3 px margin
instead of 5, which suits the bolder marks.

The averaging counts only the OPAQUE samples in each 2x2 - averaging straight ARGB would drag the transparent
ground's colour into every edge pixel - and a cell is ink when two of its four samples are.

Audited after writing: all twelve exact size, strictly binary alpha, and zero green surviving.

## `ShopServiceGlyphInset` is gone

It was 5, from a 24 px glyph in a 34 px frame. The draw now centres each glyph on the size of the file it
actually finds. The constant had already been wrong once when the art changed size, and would have pushed every
icon off centre again here - the second time is the one that makes it a rule rather than an accident.

## Build

Debug, clean. 832/832. Twelve icons repacked.

## Not verified

Not seen in play. The two to check: that every control on the Salvage page answers a click again, and that the
new marks read on the painting at their real size rather than at the 6x this was judged at.

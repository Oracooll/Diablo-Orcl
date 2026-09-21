---
version: v1.12.111
date: 2026-09-21
area: UI / Griswold's shop, sounds
tests: 832/832
---

# A bulk action carries its identity, and two sounds are corrected

## The asks

> There is an extra sound being played when i click Salvage tab. remove it.
> When i click Sell All it doesnt play the proper gold sound. Substitute the current sound with gold drop sound.
> Refresh all button appears on Sold tab when i have items in my backpack. Fix it.

(The fourth ask, white-on-black icons, is RfA-26 - no code.)

## The Sold-tab bug: a row identified by where it sits

This is a regression from v1.12.110, and the first diagnosis of it was WRONG. Recorded because the wrong
diagnosis was plausible and nearly shipped in a comment.

`ShopAction` carried a `label` and a `line`. `line` is a text-list ROW INDEX, and every store screen numbers its
own rows - so the same number means different things on different screens. In English `BackButtonLine()` is 22,
which puts **three different actions on line 20**:

```
SmithSellAllLine()        = BackButtonLine() - 2 = 20
SmithRepairAllLine()      = BackButtonLine() - 2 = 20
PremiumRefreshUntilLine() = BackButtonLine() - 2 = 20      (non-CJK)
PremiumRefreshLine()      = BackButtonLine() - 3 = 19
```

v1.12.109 gave Griswold's redesigned page six fixed buttons, which meant asking each tab *which* bulk actions it
offers - a question the old variable row never had to ask, because it was handed a list. Those lookups matched by
line. So the Sold tab's "Sell all" row, which appears exactly when `storenumh > 0` (the backpack holding
anything), answered yes to "do you offer Refresh until" - and v1.12.110's seventh button appeared there.

**The wrong first diagnosis:** that `PremiumRefreshLine()` collided with `SmithSellAllLine()`. It does - but only
when `IsSmallFontTall()`, which is true only for Chinese, Japanese and Korean. That could not have been the user's
build, and checking `IsSmallFontTall()` rather than assuming is what caught it. The lesson is the ordinary one:
an explanation that fits the symptom is not the same as the cause.

**The fix.** `ShopAction` gains a `ShopActionKind` - SellAll, RepairAll, Refresh, RefreshUntil - set where each
row is built, and the six lookups collapse into one `ActionLineOn(id, kind)`. Identity is carried, not inferred
from position. This closes all three collisions at once, including SellAll/RepairAll, which collide in every
language and would have mis-answered on the Repair tab.

## The Salvage tab's extra sound

`OpenLevskiWindowFor` played `PlayUiSelectSound()`. Every path into it has already sounded by the time it opens:

| Caller | Already sounds |
| --- | --- |
| the shop's tab column | `titlemov` at the press (v1.12.107) |
| `StoreEnter` | `IS_TITLSLCT` before it dispatches |
| the workshop's Recipes tab | `titlemov` at the press |

So a Salvage tab click answered twice. `ui_sound.h` already states the rule - "call these only on a path that is
otherwise silent" - and this path is not. The sound is removed from `OpenLevskiWindowFor`. `ToggleLevskiRoar`,
the Cube object in town, is a different function and keeps its lid-grinding cue.

## Sell All's gold

It sold in silence. `StoreSellItemAt` plays nothing, so the only sound was the button's own click.

`SmithSellAllItems` now plays `IS_GOLD` **once at the end**, and only if something was actually sold - per item
would be twenty overlapping coins. It also sounds before the no-room screen, because the items sold up to that
point really were sold, and leaving in silence would read as nothing having happened.

## Build

Debug, clean. 832/832.

## Not verified

The three fixes are logic and sound; none has been heard or seen. Worth checking in play: that the Salvage tab
now clicks once rather than twice, that Sell All's coins land, and that the Sold tab shows no seventh button
whatever the backpack holds.

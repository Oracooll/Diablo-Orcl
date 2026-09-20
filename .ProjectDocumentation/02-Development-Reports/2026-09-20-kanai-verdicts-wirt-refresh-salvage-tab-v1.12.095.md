# The Kanai verdicts built, Wirt's Refresh buttons, the Forge tab is Salvage (v1.12.095)

**Date:** 2026-09-20 · **Version:** v1.12.095 · **Tests:** 832/832 (one new)

## The user's verdicts on the Kanai's Cube Recipes page

Read from the page's `verdicts` collection (ArtifactData, 2026-09-20):

| Recipe | Verdict | Note |
|---|---|---|
| Law of Kulle, Reforge Legendary | yes | "make promotion very rare but possible." |
| Darkness of Radament, Convert Crafting Materials | yes | "Make it work the way you suggest." |
| Archive of Tal Rasha, Extract Legendary Power | later | "we will come back to it when and if we introduce Legendary powers." |
| Caldesann's Despair, Augment | no | |
| The Puzzle Ring, The Vault | no | |
| The Bovine Bardiche, Not The Cow Level | no | |

The four already-built ones (Upgrade Rare, Convert Set Item, Remove Level Requirement, Convert Gems) took no verdict
and needed none.

### Reforge Legendary: the rare promotion

Reroll Uniques (13, Griswold's book, four Unique Encrustments) now rises to Primal one time in fifty
(`RerollPromotionOneIn = 50` in crafting.cpp) instead of staying on its rung; the log line says "- and it rose to
Primal!" when it happens. Awaken (11, eight encrustments) stays the sure way up. The selection test that pinned the
rung accepts the promotion as well - the four encrustments left prove which recipe ran.

### Convert Crafting Materials: the ladder

Two recipes on Griswold's book: **Refine Materials** (26): three salvage materials of one kind become one of the tier
above; **Break Down Materials** (27): one becomes two of the tier below. The ladder is White Scales, Magic Powder,
Rare Fibres, Set Engravings, Unique Encrustments, Primal Vines (`MaterialLadder`); Ethereal Imbueities are not on it.
The top rung refines into nothing, the bottom breaks down into nothing. Counted in stack units as Refine Gems is; the
product joins a stack of its kind with room, else takes a free slot, asked on a paid copy first so a full grid refuses
before anything is spent. Test `MaterialsRefineUpAndBreakDownTheLadder`.

## Wirt's Refresh buttons

`ServiceButton::Refresh` on Wirt's Shop and Gamble tabs (shop_grid.cpp): `RefreshBoyStock(tab)` (stores.cpp) rolls
the Shop tab again or restocks the Gamble tab with fresh bases, trims it to the page and puts the cursor back on the
first item. Free; the hover hint says so.

## Griswold's tab

`ShopTabName(TalkID::SmithTransmute)` reads "Salvage" (was "Forge"). The window's own title is still "Griswold's
Forge"; the user is redesigning the tab.

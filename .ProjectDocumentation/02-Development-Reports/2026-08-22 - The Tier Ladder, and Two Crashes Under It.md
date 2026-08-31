# The Tier Ladder, and Two Crashes Under It

**Version:** v1.9.17 -> v1.9.18
**Date:** 2026-08-22
**Tests:** 505/507 (the two standing baseline failures)

Levski's Roar now has **seventeen** recipes.

## The ladder

| Recipe | Takes | Gives |
|---|---|---|
| Enrich Magic | 1 plain/magic item + 4 Magic Powder | rolled as a rare |
| Ennoble Rares | 1 rare + 5 Rare Fibres | a unique of the same base (shipped v1.9.17) |
| Consecrate Rares | 1 rare + 6 Set Engravings | a set piece for the same slot |
| Awaken Uniques | 1 unique + 8 Unique Encrustments | rolled as a primal |
| Reroll Rares | 1 rare + 3 Rare Fibres | the rare rolls taken again |
| Reroll Uniques | 1 unique + 4 Unique Encrustments | a different unique, or the unique rolls again |
| Reroll Primals | 1 primal + 6 Primal Vines | the primal rolls taken again |
| Make Ethereal | 1 weapon/armour + 5 Ethereal Imbueities | +35%, half durability |
| Mend the Ethereal | 1 damaged ethereal + 12 Ethereal Imbueities | fully repaired, still ethereal |

Climbing a rung always costs more than rerolling at it - that relation is asserted rather than
merely intended. Mending is the dearest recipe in the game, deliberately: ethereal is a bargain, and
a repair removes the only price the item was paying.

Every bump and every reroll goes through one function, `RetierOracoolItem`. A bump and a reroll are
the same act at different rungs, and one implementation is what stops "reroll at Primal" and "climb
to Primal" drifting into two distributions.

## Two things I said I would not build

**Reroll Set** was asked for and is not here. Set pieces are not rolled - `MakeSetItem` applies a
fixed stat list, which is exactly why they are kept out of the affix roller - so rerolling one
returns the identical item. The useful operation is changing *which* piece, and that is Recast Set
Pieces, already shipped.

**Reroll Uniques** has two behaviours under one name, because a player calls two different things "a
unique": a vanilla unique has fixed powers, so its reroll re-picks *which* unique it is; a
BuffedUnique-tier item genuinely is a roll, so that one is rerolled in place.

## Selection, because no automatic rule survives seventeen recipes

The monument auto-picked: first the lowest-numbered ready recipe, then (v1.9.17) the one consuming
the most grid slots. Both die here. A reagent stack of five sits in **one** slot, so nearly every
item recipe ties at two - and Ennoble Rares and Reroll Rares want the **same target and the same
material** at different counts, which no tie-break can resolve correctly.

So the recipe book became a control surface. Clicking a recipe selects it; clicking it again clears
back to automatic. The book is capped to the screen and scrolls on the wheel, because at seventeen
recipes it simply grew past the bottom of a 720-tall window.

A selected recipe that is **not ready runs nothing**. The fallback is the dangerous half: a player
short one fibre would have had the monument ennoble the item instead, spending a different pile of
materials on a change they did not ask for and cannot undo.

## Two real bugs, both found by the tests

**1. A forced tier silently did not happen.** `SetupAllItems` only reaches its forced-tier branch
when `GetItemBLevel` returns something other than -1, and that call has a random component unless
`onlygood` is set. So a large share of the time Enrich would match, run, change nothing, and return
false - the player presses Transmute on a ready recipe and watches nothing occur. `RetierOracoolItem`
now passes `onlygood` whenever it is forcing a tier, which is what makes "force this tier" mean it.

**2. Rerolling the same item twice corrupted memory.** `applyPrefix` writes at
`_iOracoolPrefixCount` and increments it - an **append**, correct for a freshly attributed item and
wrong for one being rolled a second time. Nothing reset the counts, so the second reroll carried the
first roll's affixes and walked off the end of a `std::array`. This is a crash any player would hit
within minutes of using a reroll recipe twice.

Fixed at both levels: `ClearOracoolAffixRecord` resets the record before every reroll, and
`applyPrefix`/`applySuffix` now refuse to write past the end at all. The guard is the more important
half - it is a memory-safety net under the whole affix pipeline, not just under these recipes.

The second bug only appeared when two tests ran in sequence, because it depends on RNG state. Run
alone, each passed.

## A lesson, twice

Two of my own tests failed for a number they do not depend on: `TemperJewels` asserted
`CraftingRecipeCount == 5`, and the Cube test asserted `== 9`. Both went red the moment the table
grew. A test should pin what it protects - that its recipe is reachable - and leave the count to
whichever test is about the table.

## What to look at in game

The recipe book is now clickable and scrolls, and **that needs eyes on it** - none of it is
verifiable from tests. Open Levski's Roar, press Recipes, scroll with the wheel, click a row and
check the highlight lands on the row you pointed at. Then put a plain helm and four Magic Powder in
the grid, select Enrich Magic, and Transmute.

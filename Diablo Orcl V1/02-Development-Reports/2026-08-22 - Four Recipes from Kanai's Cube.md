# Four Recipes from Kanai's Cube

**Version:** v1.9.16 -> v1.9.17
**Date:** 2026-08-22
**Tests:** 503/505 (the two standing baseline failures)

## What was picked, and what was not

Kanai's Cube has five named recipes and four utility ones. Assessed against this engine:

| D3 recipe | Verdict |
|---|---|
| Reforge Legendary | **Adopted** as Reforge Gear. `SetupAllItems` already does exactly this. |
| Hope of Cain (upgrade rare) | **Adopted** as Ennoble Rares. The unique table's `UIItemId` already says which base each unique belongs on. |
| Skill of Nilfur (convert set item) | **Adopted** as Recast Set Pieces. 94 pieces across 15 sets, and `FindSetItemByCursor` already identifies one. |
| Convert Gems | **Adopted** as Recolour Gems. Seven types by five qualities, already decomposable. |
| Archive of Tal Rasha (extract power) | **Skipped.** Needs the legendary-power system to exist first, plus save storage for which powers are held. Already its own backlog row. |
| Caldesann's Despair (augment) | **Skipped.** Per-item augments are a save format bump. |
| Law of Kulle (remove level req) | **Skipped deliberately.** The Hel rune already reduces requirements, capped at 60%. A recipe that removed them outright would make a rune pointless. |
| Convert Crafting Materials | **Skipped** - our seven materials are now recipe reagents, which is a better use than swapping between them. |
| Staff of Herding | No analogue. |

## The finding that shaped the design

**The seven salvage materials had no consumer anywhere in the game.** They dropped, stacked, sorted
into a row of their own in the stash, and nothing ever spent one. A faucet with no drain, shipped
across four versions.

So the four adopted recipes are reagent-consuming, and which material pays for what is deliberate:

| Recipe | Takes | Gives |
|---|---|---|
| Reforge Gear | 1 magic-or-better item + 3 Unique Encrustments | the same item, every roll taken again |
| Ennoble Rares | 1 rare + 5 Rare Fibres | a random unique built on the same base |
| Recast Set Pieces | 1 set piece + 3 Set Engravings | a different piece of that set |
| Recolour Gems | 1 gem + 2 Magic Powder | another type, same quality |

You salvage uniques to reforge, rares to ennoble, set pieces to recast. Each recipe is funded by the
kind of item it operates on, so the loop closes on itself.

## Nine recipes needed a selection rule

`FirstReadyLevskiRecipe` returned **the lowest-numbered ready recipe**. That was fine while five
recipes had disjoint inputs. It stops being fine the moment four arrive that all eat "one item plus
a reagent": a socketed item with reforge materials beside it satisfies both Free the Sockets (one
slot) and Reforge (four), and lowest-index picks the former every time - so reforge reagents would
be silently unusable on anything socketed, with nothing saying why.

The rule is now **most slots wins, ties to the lowest index**, because it is the one a player can
predict without reading the source: the monument runs the recipe that uses the most of what you put
in front of it. Putting in only what a recipe needs is how you choose - which is how the Horadric
Cube always worked.

## Two costs, one table

The match and the consume are separate code paths reading the same requirement. A recipe that
matched on three engravings and charged two would work perfectly and quietly hand out free crafts
forever, so both read `ReagentFor(recipe)` and there is no second copy of the number.

Consuming is stack-aware: a stack of five paying a cost of three leaves two behind rather than
having the slot confiscated.

## The dead row, finally gone

`CraftingRecipeUsesGrid` now marks the five transform recipes, and the burger Crafting window skips
them. That removes "Free the Sockets", which had sat in that window permanently greyed out since the
day it shipped - it has no backpack case at all, so `CanCraft` answered false for it forever and
nothing said it lived on the monument instead.

Two things had to move with it: the window height, which was sized off `CraftingRecipeCount` and
would have grown a band of empty panel; and the click handler, where the clicked ROW is no longer
the recipe INDEX.

## Design notes worth keeping

- **Reforge rerolls at the item's OWN item level**, with the same arguments a fresh drop at that
  depth would use. Rerolling in town cannot launder a shallow item into a deep one.
- **Reforge refuses a socketed item.** Its stats would come back without the stones inside it, and a
  completed runeword's name comes from the word rather than from a seed. Empty it first.
- **Ennoble is gated on the unique's own `UIMinLvl` against the ITEM's level**, not the character's.
- **Recast never returns the same piece** - a convert that could no-op would be a way to spend three
  engravings on nothing.

## Bug caught by the suite

`TemperJewelsClimbsTheGradeAndStopsAtRadiant` asserted `CraftingRecipeCount == 5` and went red the
moment the table grew. What that test cares about is that recipe 4 is reachable, not how many
recipes exist; loosened to `> 4`. A test that pins a number it does not depend on is a test that
fails for reasons unrelated to what it protects.

## What to look at in game

- Salvage a few uniques, put a magic item and three Unique Encrustments on the monument, Transmute.
  The item should come back rerolled and the stack should drop by exactly three.
- Try it with a socketed item: it should free the sockets instead, which is correct.
- Check the burger Crafting window - it should now list three recipes, none of them permanently grey.

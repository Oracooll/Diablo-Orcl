# Temper Jewels

**Version:** v1.9.14 -> v1.9.15
**Date:** 2026-08-22
**Tests:** 500/502 (the two standing baseline failures)

## The gap

Jewels shipped at v1.9.9 with three grades - Flawed, Plain, Radiant - and no way to climb them.
"Refine Gems" tests `IsOracoolGemIdx` and a jewel is deliberately not a gem, so the recipe declined
them. The ladder existed on paper and nowhere else: a Flawed jewel was a permanent Flawed jewel.

## The recipe

A fifth entry: **Temper Jewels**, three identical jewels into one of the next grade. Same shape as
Refine Gems, same 3-into-1 cost, Radiant excluded because nothing sits above it.

Its own recipe rather than widening Refine Gems, and its own `IsJewel` predicate rather than
folding jewels into `IsGem`. Both would have been one-line changes and both would have been wrong:
the gem recipe takes three of a `(type, quality)` pair and the jewel ladder is `(family, grade)`, so
a shared recipe would have to work out which of two decompositions applied to the index in front of
it. That is a branch pretending to be a generalisation.

It runs from both larders - the backpack's Craft path and Levski's grid - so it is not a repeat of
the "Free the Sockets" situation, where the grid has a case and the backpack does not.

## The fact the ladder rests on

The fifteen jewel ids are **grade-major**: all five Flawed, then all five Plain, then all five
Radiant. So the next grade is exactly `JewelFamilyCount` ids further on, which makes `NextJewelGrade`
one addition.

That is a property of the generator's one-walk-one-order discipline rather than a coincidence, so it
is asserted at compile time in `gems.cpp` rather than trusted:

- the fifteen are one contiguous run of 5 x 3;
- `FERVOR_PLAIN - FERVOR_FLAWED == JewelFamilyCount`.

Change `tools/GenJewels.ps1` to emit family-major and the build stops. Silently, it would temper a
Flawed Fervor into a Flawed Focus.

`NextJewelGrade` also checks `IsOracoolJewelIdx` before doing arithmetic. It is a public function,
and an arithmetic-only version would happily "climb" a rune into a gem.

## Tests

`TemperJewelsClimbsTheGradeAndStopsAtRadiant` walks all fifteen rather than sampling, so a reordered
generator cannot pass by getting one family right. For each: the family does not move, the grade
moves by exactly one, the result is worth more, and a Radiant returns itself.

Then the recipe end to end on Levski's grid - three Flawed Fervor in, exactly one Plain Fervor out
and no Flawed left - plus the three refusals that matter: two of a kind is not three, three Radiants
are not offered a grade above Radiant, and three jewels of different families are not three of a
kind.

The wiki picks the recipe up with no change: the recipe table has been parsed out of
`CraftingRecipeName` and `CraftingRecipeInputs` since the day the hand-written list said three
recipes and "Sol is the top of the ladder" long after neither was true.

## What to look at in game

Put three identical Flawed jewels on Levski's monument and press Transmute. One Plain of the same
family should come back. Three Radiants should refuse.

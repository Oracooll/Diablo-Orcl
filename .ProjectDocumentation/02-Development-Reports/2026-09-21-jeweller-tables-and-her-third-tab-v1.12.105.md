# Ogden's gem and rune tables, and Gillian's third tab (v1.12.105)

**Date:** 2026-09-21 · **Version:** v1.12.105 · **Tests:** 832/832

User: "add the third tab for her recipes" and "Ogden The Jeweller Innkeeper UI: 1. Gems Tab ... a list of all Gem
types with the number the user curently owns of each and clicking on certain type provides Upgrade/Downgrade options.
2. Runes Tab - same logic. 3. More tabs with whatever other abilities he has."

## Gillian: Reroll | Imbue | Recipes

The third tab opens her recipe book on its docked page - Rework Charms, Recast Set Pieces, Enrich Magic, Cleanse
Shards - and the workshop stands down while it is up, as Griswold's two pages trade places.

## Ogden: Gems | Runes | Jewels | Recipes

His Enter Shop opens the workshop now, and it is list-driven with no grid at all, as asked:

- Each stock tab lists every kind of that family the pack holds, in item order, with the number carried, counting
  stacks and every inventory tab. A click selects a row.
- **UPGRADE** spends the ladder's step - three gems, three jewels, two runes - and gives one of the kind above.
- **DOWNGRADE** spends one and gives two of the kind below.
- A row at the top of its ladder cannot be upgraded and one at the foot cannot be downgraded, and the buttons say so
  by going dim. If the pack cannot hold what the step makes, the spent stones are put straight back.
- The fourth tab is his recipe book, where the sockets live: Free the Sockets, Punch Sockets, Refine Gems, Ascend
  Runes, Temper Jewels, Recolour Gems.

The ladders are the game's own: `NextGemQuality`, `NextRune` with `IsTopRune`, `NextJewelGrade`, and the step down is
the rung whose step up lands on this one.

## Both

`oracool/workshop` now carries two hosts with their own tab lists, titles and painted canvases (`ui\mystic_workshop.png`
and `ui\artisan_workshop.png`). Every control still presses and runs on the release inside itself.

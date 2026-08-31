# The monument is the only forge (v1.9.142)

**Date:** 2026-08-31
**Version:** 1.9.142
**Tests:** 593/594 (the standing `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`; the suite lost two
tests, see below)

## The ask

> i want levski to be the only place recipies can produce an item. no crafting in hero backpack, so
> that column in the recepi book mentioning where recepies can happen should disappear.

## What was done

**The backpack transmute path was deleted, not unhooked.** `CanCraft`, `Craft`, and the four
anonymous-namespace helpers they stood on — `FindMaterials`, `SameKindUnits`, `MaterialsFor`,
`OutputFor` — are gone from `Source/oracool/crafting.{h,cpp}`. Leaving them compiled but unreferenced
would have made "Levski's is the only place" a UI convention rather than a fact about the code: a
second path that can still mint items is exactly the thing the rule is about. The grid walks were
always separate — they answer to twelve fixed slots with neither `InvList`'s compaction rules nor its
footprint grid — so nothing had to be untangled to remove the other half.

**`CraftingRecipeUsesGrid` and `CraftingRecipeVenue` went with it.** Both existed to name which of
two venues a recipe belonged to. With one venue, the predicate answers true for all seventeen and the
venue line prints the same words on every row — so the column was seventeen repetitions of one fact.
It lasted exactly one version (v1.9.140–141).

**The burger window is now a reading room.** It still lists all seventeen recipes, scrolled, but:
- the venue column is gone, replaced by one line under the title: *Every recipe is crafted at
  Levski's Roar, the monument in town.*
- the live/dim colouring by backpack contents is gone. It promised "you have the materials" from a
  window that cannot craft, and the materials that matter are the ones carried **to** the monument.
- a click no longer crafts. It is absorbed and names the recipe in the event log with where to go —
  the difference between a window that ignores you and one that has answered.

**Levski's book** dropped the venue line and the extra line its row height reserved for it.

## Tests

Two backpack tests were deleted: `AscendRunes_OneStackSatisfiesThePair` and
`AscendRunes_SurplusUnitsSurviveTheCraft`. Both already have monument twins
(`MonumentAscendRunes_OneGridStackSatisfiesThePair`, `MonumentRefine_SurplusUnitsSurviveTheCraft`)
covering the same stack-unit contract on the path that survives.

Two were **moved to the grid** rather than lost, because what they pin was never about the backpack:

- `AscendRunesConsumesPairAndProducesNextRung` — El + El makes **Eld**, not Tir. This guards against
  crafting walking the enum (`index + 1`), which skips Eld because the five original runes and the
  28 appended ones are separate islands.
- `MixedGemsDoNotSatisfyThreeOfAKind` — three of one KIND, and Zod never ascends while Sol still does.

The old `CraftingRecipeUsesGrid` assertion was replaced with one that fits the new shape: every
recipe in the table must refuse an empty grid, so a recipe added without a grid case fails here
rather than silently listing as uncraftable everywhere.

## Wiki

`BuildWiki.ps1` no longer parses a venue (the parsing added yesterday is removed with what it
parsed); the sockets page's recipe table is back to Recipe / Formula, and the prose above it now says
the monument is the only place a recipe produces anything. Artifact republished to the existing URL.

## To look at in game

1. Belt burger → **Crafting**: title, then the one-line note, then seventeen rows all reading the
   same — no gold/grey split by what you happen to be carrying.
2. Click a row: the event log should name it and say Levski's Roar. Nothing should ever appear in
   your backpack from this window.
3. Levski's Roar → **Recipes**: rows one line shorter again; check selection and scroll still land on
   the row under the pointer.

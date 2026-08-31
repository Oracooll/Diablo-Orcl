# One recipe list, two doors (v1.9.140)

**Date:** 2026-08-31
**Version:** 1.9.140
**Tests:** 595/596 (the two standing baselines: `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` failed,
`Timedemo.WarriorLevel1to2` skipped)

## The ask

> merge the two lists into one list accessible from both places - levski and the burger menu book.

## What was wrong

The game held **two** recipe lists that never appeared together.

- Levski's Roar's recipe book walked `0..CraftingRecipeCount` and showed all **seventeen**.
- The belt burger menu's Crafting window filtered on `CraftingRecipeUsesGrid(i)` and showed **three**.

The filter had a good reason and the wrong shape. Before it existed, "Free the Sockets" sat in the
burger window as a permanently grey row: it has no backpack path at all, so `CanCraft` answered
false for it forever, and nothing on the row said it lived on the monument instead. The filter made
the grey row go away — and with it, any way for a player who never walks up to the monument to learn
that fourteen more recipes exist.

## What was done

**One list, one vocabulary, two layouts.**

- `CraftingRecipeVenue(int index)` (new, `Source/oracool/crafting.h` / `.cpp`) is the single source of
  the venue line: `"Backpack or Levski's Roar"` for 0–2, `"Levski's Roar"` for 3–16. Deliberately
  short, because the monument's book is as narrow as 220px.
- `Source/oracool/crafting_menu.cpp` — `BackpackRecipes()` became `ListedRecipes()` and no longer
  filters. All seventeen rows draw; the venue is right-aligned on the name line (the window is 944
  wide, so there is room). Clicking a monument recipe **logs where it lives** instead of doing
  nothing — a row that answers a click with silence reads as a broken button, and the venue note is
  easy to miss on a list this long. The scroll bound already came off the listed count, so it grew
  with the list without further change.
- `Source/oracool/levski_roar.cpp` — the same venue string is drawn as a line under each formula, and
  counted in `RecipeBookRows`. That counting is the load-bearing part: the row heights are variable,
  and the draw, the click router and the scroll bound all read that one geometry, so a line drawn
  without being counted is a row whose foot the click handler cannot see.

Nothing about what runs where changed. The monument's Transmute has a case for all seventeen; the
backpack has only the generic several-into-one path and runs the first three. The merge is of the
**list**, and the list now says so per row rather than hiding the difference.

## Wiki

`tools/BuildWiki.ps1` parses the venue rather than restating it — the grid threshold out of
`CraftingRecipeUsesGrid`'s `return index >= 3`, the two strings out of `CraftingRecipeVenue`'s
ternary. The sockets page's recipe table gained a **Where** column (verified in the bundle: 3 rows
"Backpack or Levski's Roar", 14 "Levski's Roar"), and three stale "four recipes" lines in
`world.html`, `ui.html` and `sockets.html` were corrected — they had been wrong since the list
reached seventeen. Artifact republished to the existing URL.

## Test

`Test/oracool_audit_test.cpp` now asserts every recipe carries a non-empty venue, that exactly two
distinct venues exist, and that a backpack recipe and a monument recipe do not claim the same one.
A single venue string for both would put "Levski's Roar" beside the three the belt can actually run,
and nothing else in the suite would notice.

## To look at in game

1. Belt burger menu → **Crafting**: seventeen rows, scrolled, each with its venue on the right.
2. Click a monument row there — the event log should say where it is crafted, not sit silent.
3. Levski's Roar → **Recipes**: the same seventeen, each with its venue line under the formula, and
   selection/scroll still landing on the row under the pointer now that the rows are one line taller.

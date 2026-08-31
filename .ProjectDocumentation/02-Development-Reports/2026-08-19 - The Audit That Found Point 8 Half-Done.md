---
date: 2026-08-19
version: 1.8.16
area: Reaudit of 1.8.14-1.8.15, and the wiki caught up to Levski's Roar
---

# The Audit That Found Point 8 Half-Done

Two builds to check: the Pipeline page (1.8.14) and Levski's Roar (1.8.15).

## The finding: point 8 is not done

Directive point 8 reads "Crafting of gems, runes will be happening in Levski's Roar." The monument
shipped with all four recipes - and **the burger menu's own Crafting window was left exactly where
it was**, still running the same three recipes against the backpack.

That is not a move, it is a duplication: two places crafting the same things from different larders,
which is worse than either alone. My own 1.8.15 report said "Point 8: crafting moves here", which
was wrong, and this audit is what caught it.

**It is not fixed in this build, and the reason is worth recording.** Removing the burger entry
shifts every icon after it: `hud_menu.cpp`'s row order must match `assets/ui/menu_icons.png`
exactly, and the file says so in a comment written the last time that bit someone. Retiring the
window properly needs the icon sheet recut first, which is art work rather than a code edit. I
attempted the removal, saw the ordering constraint, and reverted rather than shipping a menu whose
icons no longer match their actions.

It is now a Pipeline entry with the dependency named, and the wiki's window list shows the legacy
window as superseded rather than pretending it is gone.

## The wiki did not know Levski's Roar existed

The sockets page still said retrieval was "planned", the crafting section described the backpack
window, and nothing anywhere mentioned a monument in town. Fixed across four pages: the sockets
page has a Levski's Roar section, the town page lists it, the window table names it, and the
version history covers 1.8.12 through 1.8.15, which had gone unlisted.

## One more hand-typed table, now parsed

`socketRules.recipes` was a hand-written list in `BuildWiki.ps1`: three recipes, and a note saying
"Sol is the top of the ladder" that had been false since 1.8.9. It now parses `CraftingRecipeName`
and `CraftingRecipeInputs` out of crafting.cpp, so it reads four recipes and cannot drift again.
The grid dimensions come from `levski_roar.h` the same way, which is what makes the page say 3x4
rather than a number I typed.

That is the third hand-typed table this line of audits has found inside the generated wiki. The
pattern is stable enough to state as a rule: **if the wiki says a number, it should have parsed
it.**

## Verified

Wiki rebuilt and checked in the browser: the Levski's Roar section renders, the grid reads 3x4 from
the header, and all four recipes list including Free the Sockets. No engine code changed in this
unit, so the test state stands at 1.8.15's **454, the usual two**.

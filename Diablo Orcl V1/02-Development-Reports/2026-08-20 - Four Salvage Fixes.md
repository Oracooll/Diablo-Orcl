# Four salvage fixes

**Version:** 1.8.76
**Date:** 2026-08-20

Four user reports against the salvage system, two screenshots.

## 1. The buttons only swept the page you were looking at

> "Salvage buttons to sweet all tabs."

`SalvageAllInBackpack` walked through `GetActiveNumInv` / `GetActiveInvListItem` /
`RemoveActiveInvItem` - the helpers that read **whichever tab is displayed**. So "Salvage all rares"
pressed on page 1 left every rare on pages 2-10 alone, and the only way to find out was to page
through afterwards and see them still sitting there.

Rewritten to collect across the main backpack and all nine extra tabs, then remove in reverse order.
Each victim now carries its tab index (`-1` = main backpack), and removal routes to
`Player::RemoveInvItem` or the existing `RemoveExtraTabItem` accordingly. The vector is built tab by
tab, ascending within a tab, so a single reverse walk removes the highest index of every list first -
which is what keeps list compaction from making the walk skip items.

**The lit-button readout had the same bug** and is fixed with it. The draw loop ran its own copy of
the same active-tab walk, so a button could sit dark while page 3 was full of rares. Both now call
one new `AnySalvageableInBackpack`, so what the button *looks like* and what pressing it *does*
cannot disagree.

## 2. Materials spawned red

> "Check the red salvage material. all of them spawn red then recolor after some interaction with
> inv."

Not a palette or frame problem. `DrawItem` (`Source/cursor.cpp:713`) picks the draw path off
`_iStatFlag` alone:

```cpp
if (usable) ClxDraw(...); else ClxDrawTRN(..., GetInfravisionTRN());
```

The infravision TRN is red. `InitializeItem` leaves `_iStatFlag` false, so every freshly made
material rendered as "you cannot use this" until some later inventory action happened to run
`CalcPlrInv`, which sets the flag for everything at once - hence "recolor after some interaction".

Both creation sites now call `material.updateRequiredStatsCacheForPlayer(player)` immediately.
The vendor path already set the flag, which is why bought charms never showed it.

## 3. You could hover Ogden through Levski's panel

> "You can see i can hover over ogden. Fix this. It is annoying bug."

`CheckCursMove` excludes each **docked** window by its own rect - inventory, stash, spellbook, left
panel - but the **free-floating** windows had no such test. Levski's Roar and the runeword book are
centred over the world, so the world kept being probed straight through them. Clicks were already
absorbed by `CheckLevskiRoarClick`, which made it worse rather than better: the cursor advertised a
target the router had already decided to swallow.

New `oracool::IsPointOverFloatingWindow`, beside `IsPointOverHudChrome` in `hud_layout` and called
from the same block in `CheckCursMove`. **One function, not a test per window** - for exactly the
reason `IsPointOverHudChrome` exists: the next floating window should join a list rather than become
a fourth place that has to remember this rule. It covers Levski's Roar, its recipe book, and the
runeword book - which had the same bug and was never reported.

## 4. The buttons gave no sign they had been clicked

> "Make some visual feedback when i click on levskis buttons."

A pale gold fill inside the border for 170ms, with the label going white over it. Fires on the click
itself, **whether or not anything was salvaged** - it acknowledges the click, not the outcome; the
log line already reports the outcome.

Held as a button index plus an expiry tick rather than a bool, copying the inventory SORT button
exactly: these buttons act on mouse-down and nothing here polls a mouse-up, so a flag would either
linger until the next click or need a second owner to clear it. One mechanism covers all nine
buttons - the seven tiers, Transmute, and Recipes.

## Verification

- Build clean. Tests **487/489**, the two standing baseline failures.
- **No asset changed**, so no MPQ repack this round.
- Items 3 and 4 are drawn over the world, and per this project's own rule a screenshot is the only
  real verification of those - the user runs the game.

## What to look at

1. Fill several inventory pages with rares, press **Rare** on page 1; every page should be swept.
2. A freshly salvaged material should be its own colour on the very first frame, never red.
3. Open Levski's Roar over the tavern and sweep the mouse across the panel - Ogden must not
   highlight, and no target cursor should appear over any part of the window. Same test for the
   runeword book.
4. Every button on the panel should flash pale gold on click.

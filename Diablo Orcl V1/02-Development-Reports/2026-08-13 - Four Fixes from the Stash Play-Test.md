---
date: 2026-08-13
version: 1.1.88
area: UI / Stash, Inventory, Abilities window
---

# Four Fixes from the Stash Play-Test

Follow-on to [[2026-08-13 - The Stash Joins the Theme]]. Three came out of the screenshots, one out of
a later request; all four are small, and two of them are the same shape.

## The stash's buttons were invisible

> Buttons in stash are invisible. the grid lines are missing.

The four page-navigation arrows were **real controls the whole time** - drawn, hit-tested, and doing
their job. They just had nothing to show for it until they were held down.

`data\stashnavbtns.clx` holds only the *pressed* frames. Every arrow's unpressed state was painted
into `data\stash.clx`, the 320x352 panel background - which the theme pass removed, because the panel
is drawn procedurally now. So the button art was complete only in combination with an asset that no
longer existed, and nothing said so: no reference broke, the CEL still loaded, the build was clean and
the archive packed.

The arrows became text - `<<`, `<`, `>`, `>>` through the same `DrawString` the SORT button already
used, which had been converted from art for its own reasons in the previous pass. The `stashnavbtns`
load is gone with them.

Worth naming the class of bug, because the asset pipeline can produce more of it: **a sprite sheet
that is only half a control.** A pressed-state-only sheet is a perfectly reasonable optimisation when
the rest lives in the background plate, and it is invisible the moment the plate does not.

## Grey grid lines, in both grids

> i want to take that opportunity to make them in different color than the grid line in inv grid.
> make them dark gray if you can. 1px thick.

then, after seeing it:

> I love the gray grid lines in stash. Apply them to Inventory grid as well.

Both grids divided their cells with `DrawOrnateSeparator` - the theme's 3px gold bevel. That reads
well as a *frame*; as a *mesh* it does not. The stash draws 170 cells and the inventory 70, and at
three gold pixels per boundary the grid becomes the thing you look at and the items sit inside it.

One pixel of `PAL16_GRAY + 11` per boundary, in `oracool::ThemeGridLineColor` (ornate_border.h)
rather than a literal in each file - same argument as `ThemeEdgeColor` next to it: the two grids are
on screen **together** whenever the stash is open, so any drift between them would read as a mistake
rather than as two decisions.

There is one geometric difference between them that is worth writing down, because it will look like a
bug later:

| | pitch | where a rule lands |
|---|---|---|
| Stash | 29px (28px slot + a dedicated rule pixel) | in a real gutter, between two slots |
| Inventory | 28px flat, no gutter | on the boundary pixel of the cell to its right |

At 1px that difference is invisible. At 3px it was exactly why the old code had to centre its rules on
the boundary via `OrnateBorderWidthHalf` - without it, the whole bevel sat inside one cell and shifted
that cell's visible interior half a rule off its own rect. That centring is gone now along with the
bevels, so if inventory item icons ever look a pixel off, this is the first place to look.

## Descriptions that stopped mid-sentence

Not reported - found in a screenshot of the Barbarian sheet while checking something else.

The aura and Barbarian skill descriptions were written to about 75 characters, on an estimate of ~38
per line in a two-line block. The column actually fits closer to 25, so every longer description
wrapped to three lines and lost the third: *"Reduces the duration and damage of poison and other"* and
then nothing.

`DescribedRowDescLines` 2 -> 3, and `DescribedRowHeight` 60 -> 78 to match. The row grew rather than
the text being cut, because for several of these the third line is where the ability says what it
actually does.

Measured from the rendering rather than re-estimated, and the row height is now derived from the line
count rather than being its own number:

```cpp
constexpr int DescribedRowHeight = 2 * DescribedRowPadding
    + AbilitiesLineHeight * (1 + DescribedRowDescLines);
```

## Town Portal leaves the Skills sheet

> town portal doesn't need to appear in skills list.

It was put there on the reasoning that it is not a spell but it *is* a skill the character has - see
[[2026-08-11 - HUD Follow-ups and Town Portal as Built-In Ability]] for why it stopped being a spell
at all. That reasoning was about what the portal *is*; it did not check what a **row** is for.

Every affordance a row in this window offers is meaningless for the built-in portal. You cannot ready
it (it is cast from a fixed HUD button). It has no mana cost to print. It cannot be unlearned, so the
greyed-out treatment never applies. It cannot be clicked to any effect.

That showed up in the code as **four special cases in four different functions**, each one teaching a
different step of the pipeline to make an exception for a single row:

| Where | The exception |
|---|---|
| `IsSpellKnown` | return true without consulting the spell bitmasks |
| `GetSpellDetail` | a bespoke string, "Innate - cast from the belt" |
| `DrawSpellRow` | force the icon translation to `Skill`, because asking would grey it |
| `CheckSBook` | swallow the click |

Deleting the row deleted all four. `BuildSpellRows`'s filter is now the only place in the window that
knows the built-in portal exists, and its job there is the opposite one: keeping it out of the Spells
sheet, where it would otherwise arrive from `SpellPages[1][4]`.

The general lesson is cheap and reusable: **a row that needs an exception at every stage it passes
through is a row the list was not built to hold.** Four exceptions was the list saying so.

One consequence to look at in game: the Skills sheet is now a single row for every class - Item Repair
for the Paladin, Trap Disarm for the Rogue, Staff Recharge for the Sorcerer, Furious Charge for the
Barbarian. It is a 340x720 window showing one line. That is correct, and it may still be worth
deciding later whether Skills earns its own sheet or folds in somewhere.

## Files

- `Source/qol/stash.cpp` - text nav arrows, `stashnavbtns` load removed, grid lines.
- `Source/inv.cpp` - grid lines, 3px gold bevels to 1px grey.
- `Source/oracool/ornate_border.h` - `ThemeGridLineColor`, shared by both grids.
- `Source/panels/spell_book.cpp` - `DescribedRowDescLines` 2 -> 3; Town Portal row and its four
  special cases removed.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.88**. Tests **351/353** at each of 1.1.86, 1.1.87 and
1.1.88 - `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, the same two
pre-existing failures as every build this session.

The stash arrows, grid lines and Barbarian descriptions were confirmed from the user's screenshots.
The Skills sheet after the Town Portal removal has **not** been seen in game: worth confirming the
sheet still draws its one row correctly, that the scrollbar stays hidden with nothing to scroll, and
that the Portal button on the HUD still casts.

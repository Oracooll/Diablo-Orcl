# Transmute stops talking, and the passive hint moves off the title

**Version:** 1.11.068
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "remove the pop up text when hovering over Transmute button in Levskis Roar"

> "move the gold explanation text in Passive skills windows a few px down close to the 4 slots.
> because now it is overlapping a couple of pixels with the title."

## Transmute

`SetLevskiHoverInfoString` named every control in the window. The Transmute plate is painted with
the word TRANSMUTE already, so the info line was repeating what the button says.

It now returns `false` for that button rather than `continue`-ing the loop: the cursor is on a
button, so there is nothing underneath it for the grid hover check further down to find, and saying
so directly is clearer than falling through a loop that cannot match anything else.

Recipes and the four Salvage plates keep their lines - those plates carry icons, not words.

## The passive hint

The gold instruction line ("Click a slot, then a passive") sat in panel-y **77..95**, and this
window's title band is **57..95** - it was inside the title outright, which is what showed as a
couple of pixels of collision on the glyphs.

**The cause was v1.11.062, in this same file.** The Abilities window's tab row moved its title from
the shared `PanelTitleTop` of 28 down to 57, and this hint is positioned *upward* from the passive
slots - so the two grew into each other. Nothing was wrong with either change on its own.

The gap above the slot frames goes 6px -> 2px, moving the line 4px down:

| | before | after |
|---|---|---|
| hint box | 77..95 | 81..99 |
| slot frames start | 101 | 101 |

It cannot be lifted clear instead: the title ends at 95 and the frames begin at 101, so there are
only six pixels of clear space and the box is eighteen tall. Down is the only direction, and it is
where the line belongs anyway - it is the slots' own instruction, so sitting on them reads correctly.

The gap is a named constant (`HintGapAboveFrames`) rather than a literal, so another pixel either
way is a one-line change.

## Verification

Debug and Release build clean; **718/718** tests pass. RTM updated.

Both are visual and only the real window can settle them: whether the hint now clears the title's
descenders by enough, and whether losing the Transmute line leaves the button readable.

---
date: 2026-08-19
version: 1.8.21
area: Levski's Roar - the presentation pass a screenshot forced
---

# A Wireframe on Top of Tristram

The user sent a screenshot of the working window and asked what I thought. The mechanism was
right - 3x4 grid, both buttons, the recipe book, the title all rendering exactly as designed - and
the presentation was wrong in three ways that no amount of reading the code would have surfaced.

## Three faults, all only visible in a screenshot

**The panels were see-through.** One `DrawHalfTransparentRectTo` pass over open ground leaves the
town's rooftops legible straight through the grid. Every other window in the game sits on painted
art and never had to solve this; Levski's Roar is centred over the world, so it has to build its
own opacity. Now a dark fill plus a transparency pass, through one `DrawPanelGround` helper so the
window, the twelve cells and the book cannot drift apart.

**The recipe text was clipped in two directions.** Horizontally, the formulas were never wrapped -
"3 identical gems -> one of the next qualit" ran off the right edge mid-word. Vertically, the panel
was sized as `recipes x 40px` while each formula actually needs two lines, so the fourth recipe's
second line was sliced by the bottom border: "1 socketed item -> the item, emptied, and its
**stones back**" simply stopped.

Both halves came from the same mistake - a guessed row height instead of measured text. The book is
now wrapped with `WordWrapString` and **sized from the wrapped string**, so the panel cannot be
shorter than what it holds, and widened to 420.

**The book opened into the mini-map.** It was placed to the right of the window, which is where the
mini-map lives. It opens left now; the window is centred, so there is always room.

## What is still not right, and is not code

The twelve cells wear the same ornate border as the panel around them, so at a glance they read as
decoration rather than as places to put something. A slot wants to look recessed - a socket, not a
frame. That is an art job for the real asset pass rather than something to solve with more border
work, and it is recorded here rather than quietly left.

## The lesson worth keeping

Four defects in this feature were found by reading code and one round of play. All three in THIS
report needed a picture. The suite cannot see them, the data tables do not describe them, and
reading my own draw code did not reveal them because the code does exactly what it says - it is
the RESULT that was wrong.

For any window drawn over the world rather than over panel art, a screenshot is not a nice-to-have
verification step; it is the only one that works.

## Verified

**454 tests, the usual two**, build clean.

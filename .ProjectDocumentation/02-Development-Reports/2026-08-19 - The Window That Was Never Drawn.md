---
date: 2026-08-19
version: 1.8.20
area: Levski's Roar - the fourth defect, and the first one that was mine
---

# The Window That Was Never Drawn

User play-test of 1.8.19: "i can click on Levskis but what happens is my hero just walks to that
coord. No pop-up ui emerges. No interaction."

That is the discriminator 1.8.19 named, and it answered the question: **the hero walks**, so the
click routes, the command sends, the path is made, and `ACTION_OPERATE` fires. Everything up to and
including `ToggleLevskiRoar()` was working the whole time.

## The bug

`DrawLevskiRoar(out)` was inside `case LeftPanelContent::Crafting:` in scrollrt.cpp's left-panel
switch.

It went there because that is where `DrawCraftingMenu` lives and I added the call beside it without
asking whether the two windows are the same KIND of thing. They are not: the crafting menu is a
left-panel slot, and Levski's Roar is a free-floating centred window. `GetLeftPanelContent()` never
returns Crafting on the monument's account, so the case never ran.

So every click on the monument opened the window, and nothing ever rendered it. `WindowOpen` was
true; there was simply nothing on screen.

Moved out of the switch to a top-level call beside the HUD menu, which is the other free-floating
window and the correct neighbour.

## The tell that should have found it three builds earlier

**It failed identically every single time.** `ToggleLevskiRoar` toggles - so an operate path that
was genuinely broken would have produced no change on click one and no change on click two, but a
WORKING operate on an invisible window produces exactly the same visible result forever. The
symptom could not distinguish them, which is why 1.8.17's selFlag fix, 1.8.19's name fix and
1.8.19's solid-flag fix all looked plausible and none of them changed what the user saw.

Three of those four fixes were still real defects and are worth keeping. The solid-flag change is
the one now in doubt: it may have been unnecessary. It is harmless - a monument that does not block
movement is fine - and I am not reverting a change I cannot re-test, but it is recorded here as
possibly-unneeded rather than left looking like a confirmed fix.

## What actually distinguishes these, for next time

The four defects in this feature split cleanly:

- **Inherited from the vanilla data row** (selFlag, name, Solid) - found by reading objdat.cpp
  against the new use.
- **Mine** (the draw call) - found by asking where a call was placed and whether that placement's
  precondition can ever hold.

The second class is invisible to data-table auditing, and the first is invisible to reading my own
diff. This one needed the question "does this branch ever execute?", which neither audit asked.

## Verified

**454 tests, the usual two**, build clean. The fix itself is unreachable by the suite, as with
every other defect in this feature.

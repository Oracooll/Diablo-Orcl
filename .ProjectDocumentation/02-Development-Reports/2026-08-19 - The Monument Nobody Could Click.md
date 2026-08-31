---
date: 2026-08-19
version: 1.8.17
area: Reaudit of the whole 1.8 line - a functional bug, and the fourth hand-typed table
---

# The Monument Nobody Could Click

Fourth audit of the 1.8 line, this time from 1.8.0. The previous three each found a different
class of problem - wrong numbers, superseded prose, a half-done directive - so this pass went
looking somewhere none of them had: **the data tables the code inherits from vanilla.**

## Levski's Roar was unreachable

`OBJ_STAND` carries `selFlag = 0` in objdat.cpp. In the Caves that is correct - it is scenery the
Anvil of Fury quest swaps out, never something the player clicks - and `AddObject` copies it
straight into the placed object's `_oSelFlag`. Every selection path tests `_oSelFlag == 0` and
refuses.

So the monument drew perfectly in town, sat one tile from the stash exactly as asked, and **ignored
every click**. Levski's Roar shipped in 1.8.15 unreachable.

Fixed by stamping `_oSelFlag = 3` after placement - the bookstand's value, chosen because it shares
OBJ_STAND's animWidth and solid flags. The same shape as the stash chest pinning its own
`_oAnimFrame` after AddObject, and for the same reason: the type's defaults are right for where the
type normally lives, not for where we put this one.

### Why 454 passing tests said nothing

**No test clicks a town object.** The suite covers item math, save formats, drop pools and the
recipe walks; it has never had a reason to assert that a placed object is selectable. Three green
runs across 1.8.15 and 1.8.16 were all true and all silent on whether the feature worked at all.

That is worth stating plainly because it is the limit of the current suite, not a lapse in it: a
test that would have caught this has to assert against the OBJECT INSTANCE after placement, and
nothing in the codebase does that yet. Building that harness is a Pipeline entry now.

## The fourth hand-typed table

`BOOK_ILVL` in spells.html - the band-to-book-ilvl map - was typed into the page's JavaScript. Its
six values were correct, which is exactly how the previous three survived so long.

That is now four found across four audits: the quality band curves (1.8.12), the crafting recipes
and the runeword grid size (1.8.16), and this. All four were parseable from source the whole time.
The rule stated at 1.8.16 - **if the wiki says a number, it should have parsed it** - has enough
evidence behind it now to be a build rule rather than an aspiration, and the remaining candidates
are worth a sweep of their own rather than being found one audit at a time.

## Verified

**454 tests, the usual two** (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`). The selFlag fix is engine code and cannot be covered by the existing
suite, which is the finding above; it needs a play-test to confirm the monument now opens.

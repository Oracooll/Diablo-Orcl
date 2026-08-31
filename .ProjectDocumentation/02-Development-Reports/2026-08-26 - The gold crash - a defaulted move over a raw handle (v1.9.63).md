# The gold crash — a defaulted move over a raw handle (v1.9.63)

**Date:** 2026-08-26
**Version:** 1.9.62 → 1.9.63
**Reported by:** the user, from actual play — the first real session since v1.9.9
**Tests:** 560, of which 558 pass — the two standing baseline failures, unchanged.

---

## The report

> i can get into cathedral lvl1 and kill a few monsters, but i crashed two time while walking near
> gold, so picking it up causes problems. it also crashed once when i hit Main Menu from the game
> menu.

Two symptoms, one bug, and it is mine — introduced in v1.9.60 and shipped in the v1.9.62 release.

---

## Why gold

In single-player, picking up gold does not put it in your pack. It goes **straight to the stash**
(`inv.cpp:2495`), and that sets `Stash.dirty = true`. Picking up gold therefore:

1. marks the stash dirty
2. schedules an autosave
3. and the save takes the stash branch of `SaveHeroAndStash`

That branch held its writer in a `std::optional` and filled it with
`stashWriter.emplace(GetStashWriter())`.

**That move-constructs an `MpqWriter`.** Which was, until now, a disaster.

Main Menu is the same path by a different door: `GamemenuNewGame` calls `SaveOnExit()`, which calls
`SaveHeroAndStash`. Any gold picked up during the session leaves the stash dirty, so the same branch
runs. The user's two symptoms are one fault.

---

## The fault

`LoggedFStream` wraps a raw `FILE *` and had **no user-declared move constructor**. The
compiler-written one copies the pointer and leaves the source holding it too. `MpqWriter`'s move was
`= default` as well, so the husk left behind by a move kept a live-looking stream.

For years that was harmless, because the destructor did very little. **It stopped being harmless on
2026-08-25**, when the shadow archive landed and the destructor started doing real work: write the
tables, close the file, publish the archive over the target.

So `emplace` did this:

1. move-construct the writer into the optional — the `FILE *` is now held by two objects
2. destroy the temporary — which writes tables, **`fclose`s the handle**, and calls
   `ReplaceFileAtomically(name_, target_)` where both strings are empty, having been moved out
3. the surviving writer carries on using a closed handle → access violation

Reproduced in a test as `SEH exception with code 0xc0000005` — the user's crash, exactly.

---

## The fix, in three layers

**`LoggedFStream` owns its handle properly.** A written-out move constructor and assignment that
null the source, and copying deleted outright — two owners of one handle has no correct meaning
here. Written out rather than defaulted *precisely because* `= default` is what was wrong.

**`MpqWriter`'s move leaves the source inert.** `MakeInert()` clears the shadow flag, the paths and
the transaction state, and sets `finished_ = true` — which the destructor already reads as "the
caller has made the publish decision", so it makes none of its own. Move-assignment publishes what
it is overwriting first, because dropping an open archive on the floor would silently lose a save.

**The call site no longer moves at all.** `stashWriter.emplace(GetStashSavePath())` constructs in
place from the path. The move is safe now, but not making one is better: there is no husk to reason
about.

---

## What this says about the last four days

The save path has been through six external audits and gained a lot of tests. **None of them caught
this**, and it is worth being precise about why rather than filing it under bad luck.

Every test wrote to a writer, or failed a writer, or published a writer. **Not one of them moved
one.** The bug lived entirely in a construction the tests never performed — and it was reachable
from the single most-travelled action in the game, picking up gold.

It is also the second time in three days that a change was correct in isolation and wrong in
context. The shadow was right. The destructor doing the publishing was right. What was wrong was
that making the destructor meaningful silently changed what `= default` meant for every move of that
class, and nothing in the language or the tests said so.

The new test, `MovingAWriterDoesNotCloseOrPublishTheOriginal`, covers the move, the move-assignment
and the husk's destruction. With the defaulted moves restored it crashes with an access violation
rather than merely failing, which is the strongest form of "this test is not vacuous" available.

---

## Not investigated

The user reported crashes "very soon after starting" and named two triggers. Both are explained by
this one fault and both are fixed. **If crashes continue after this build, they are something
else** — and the next thing I would want is the log at
`%APPDATA%\diasurgical\devilution\diablo.log`, plus what was on screen.

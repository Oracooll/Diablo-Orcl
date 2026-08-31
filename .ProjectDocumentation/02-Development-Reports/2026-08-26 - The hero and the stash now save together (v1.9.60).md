# The hero and the stash now save together (v1.9.60)

**Date:** 2026-08-26
**Version:** 1.9.59 → 1.9.60
**Tests:** 557, of which 555 pass — the two standing baseline failures, unchanged.

---

## Why this, and why now

This was the last item I had flagged in my own report on the fifth audit, and it moved to the top of
the list for a reason that is worth stating plainly: **the shadow I built in v1.9.59 made this
particular problem worse rather than better.**

A character and their stash are one state spread over two files. Move an item from the stash into
your pack and the two disagree until both are written. If only the hero lands, the item is in your
pack *and* still in the stash. If only the stash lands, it is in neither.

Before the shadow, both files were written at roughly the same time, and the gap between them was a
matter of chance. After it, the ordering became deterministic and unfavourable:

1. hero — build the shadow (all the risky writing), publish it
2. stash — build the shadow (all the risky writing), publish it

The stash's risky work now happens *after* the hero has already published. A disk that fills up
lands squarely in the gap, every single time, rather than occasionally.

That is a good example of a fix that is correct in isolation and harmful in context, and it is only
visible if you look at the two callers together rather than at each archive on its own.

---

## What changed

Two files cannot be swapped in one act; a filesystem does not offer that. What it does offer is that
**a rename needs no disk space, and disk space is what actually runs out.**

So the order is now:

1. hero — build the shadow (all the risky writing), **stop**
2. stash — build the shadow (all the risky writing), **stop**
3. publish hero
4. publish stash

A full disk now fails at step 1 or 2, while both archives are still invisible, and **neither** is
published. The character on disk stays the last one that fully succeeded, and it still agrees with
the stash beside it — which is the property actually worth protecting.

### The mechanism

`MpqWriter` gained three methods:

- `Finish()` — writes the tables, closes the stream, leaves the finished archive as a shadow.
  Spends all of the archive's risk without publishing any of it.
- `Publish()` — the single replacing rename that makes it real.
- `DiscardShadow()` — throws it away, for when the *other* archive failed.

The destructor keeps its old behaviour for every existing caller: if `Finish()` was never called, it
finishes and publishes in one go, exactly as before. If `Finish()` *was* called and no publish
decision followed, it discards — a finished archive nobody published is a save nobody asked for, and
the destructor must not guess.

The unpacked backend gets the same three, mapped onto its existing transaction: a directory already
stages every record under a temporary name, so `Finish()` has nothing to do but report, and
`Publish()` is the batch of renames.

### The caller

`SaveHeroAndStash(bool writeGameData)` replaces the `pfile_write_hero(); sfile_write_stash();` pair
at all four sites that used it: the exit path, `init_cleanup`, `SaveGame`, and both autosave paths.

`sfile_write_stash()` remains for the one place that writes only the stash.

---

## What this does not fix

**The window between the two renames is not zero.** A crash or a power cut in that gap still leaves
the hero published and the stash not. This is the honest limit of the approach and it is written
into the comment on the definition rather than left for a future audit to discover.

The reason it is acceptable: a rename needs no disk space and does no I/O beyond a directory entry,
so it does not fail for the reason saves actually fail. Closing it completely needs a journal — a
third file recording "these two publishes belong together", replayed on load — and that is a larger
change than the remaining risk justifies today.

---

## Testing

One new test, 556 → 557: `AFinishedArchiveIsInvisibleUntilItIsPublished`. It pins four things,
because the contract has four states and three of them are new:

- finished but not published → the archive on disk is **still the old one** (this is the state the
  hero sits in while the stash is being written)
- finished then published → the new one, and the destructor does not undo it
- finished then discarded → still the old one, and no shadow left behind
- finished then simply abandoned → still the old one; the destructor does not guess
- never finished at all → publishes, exactly as before, which every existing caller relies on

**Verified by reintroduction**: making `Finish()` publish immediately — which is effectively what
the old code did — fails the test on three separate assertions.

The `UNPACKED_SAVES` configuration was re-checked with the same syntax-only compiles set up during
the fifth audit, since this change touches both backends.

---

## Still open

**The autosave state machine still has no test.** Unchanged from the last report: its state is
file-local and its triggers need live game state.

**A stray `.tmp` after a crash** is cleaned at the next open of that archive rather than at startup.

**None of this has been played.** The save path remains the best-tested part of the fork and the
least exercised in an actual session.

# A failed save no longer ends the session (v1.9.64)

**Date:** 2026-08-26
**Version:** 1.9.63 → 1.9.64
**Tests:** 561, of which 559 pass — the two standing baseline failures, unchanged.

---

## The evidence

The user sent the contents of their save folder after a crash on the first gold drop. It is the most
useful bug report this project has had, because it pins the moment of death exactly:

| file | size | meaning |
|---|---|---|
| `single_0.hsv` | 151.2 kB | the previous save, intact |
| `single_0.hsv.tmp` | 110.4 kB | a **finished** hero shadow, never published |
| `stash.hsv.tmp` | **0 B** | a stash shadow just created, nothing written |
| `stash.hsv` | *absent* | the stash has never been saved |

Read in order, that is a precise trace through `SaveHeroAndStash`:

1. the hero writer copied, wrote, and **completed** — the shadow is 110 kB because `Finish()` ran
   its `ResizeFile`; an in-progress shadow could only be ≥ the 151 kB it was copied from
2. the stash writer was constructed, creating its shadow file
3. the process died before the stash record was written

So the fault is between constructing the stash writer and writing to it. And a **zero-byte** shadow
is not an arbitrary point to die at — it is the exact state the `MpqWriter` constructor leaves
behind between creating the file and reopening it.

---

## What was wrong

Two things, and the second is the one that turned a bad moment into a lost session.

### The constructor created the file, closed it, and reopened it

Inherited from upstream, where it makes sense: the writer cannot know whether the archive is already
there, so it appends to create, closes, measures, and reopens read/write.

**The shadow always knows.** The constructor removes any stale `.tmp` a few lines earlier, so at
that point the file provably does not exist. The dance bought nothing and cost a window in which a
brand-new file is closed and immediately reopened — which is the classic way to meet a transient
sharing violation from a virus scanner or a sync client reacting to file creation.

That window is new. Before 2026-08-25 a save reopened a long-lived archive; since the shadow landed,
**every save creates a fresh file this way**, multiplying the exposure by every save the game makes.
Now it is one `w+b` open, which creates and gives read/write in a single call.

### A failed open killed the game

`on_error` ended in `app_fatal`. So one unlucky file operation did not fail the save — it ended the
session.

And it *guaranteed* the loss it was reacting to. At that moment the hero archive is finished and
waiting to be published; a dead process never publishes it. That is exactly what the user's folder
shows: a complete, correct, 110 kB save that nothing was left alive to move into place.

The judgement here is not new — it was already written into this same constructor for the staging
step, thirty lines above:

> Deliberately leaves the writer inert rather than calling `app_fatal`: this runs from the autosave,
> and killing the game because one save could not be staged would turn a recoverable full disk into
> a lost session.

I applied that reasoning to one failure path and left the other one fatal. The `on_error` path now
does the same thing: log it, remove the useless shadow, mark the writer inert, and report the save
as failed through the seam the player actually sees.

---

## Why gold, again

Not a coincidence, and worth stating for anyone reading this later. In single-player, gold does not
go into the pack — it goes to the **stash**, and marks it dirty. Picking up gold is therefore the
most reliable way in the whole game to make a save touch the stash file, and the stash file is the
one that does not exist yet on a new character.

The first gold drop is, in effect, the game's own integration test for the new-archive path.

---

## Honesty about what is proven

The previous crash was **proven**: reintroducing the defaulted move reproduced the user's access
violation in a test. This one is not.

I could not reproduce this failure. `SaveHeroAndStash` is now covered by a test that creates a new
character, dirties the stash and saves twice — the exact shape of the reported crash — and it passes
both before and after this change. What I have is a trace from the file sizes that identifies the
region, a reachable `app_fatal` in that region, and a mechanism that explains why it started
happening now.

That is a strong case, not a demonstration. **If the game still dies on the first gold drop, the
fix is wrong** and the next thing needed is the text of any error dialog — `app_fatal` shows one and
names the failure, whereas an access violation closes the window silently. That single fact
separates the remaining hypotheses.

The change stands on its own merits regardless: a save that cannot open a file should never end the
session, and the create-close-reopen dance was pure cost on a file we had just proved absent.

---

## Also worth recording

`SaveHelper`'s `app_fatal` on a short buffer stays fatal, deliberately. That one means a size
constant is wrong and the record on disk would be truncated — real, silent save corruption. It was
checked against this crash and does not fire: an empty stash writes exactly the 18 bytes it
declares.

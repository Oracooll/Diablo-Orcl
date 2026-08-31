# The sixth external audit — my own fix, audited back (v1.9.61)

**Date:** 2026-08-26
**Version:** 1.9.60 → 1.9.61
**Audited:** the published v1.9.59 release, commit `d7e9641`
**Tests:** 558, of which 556 pass — the two standing baseline failures, unchanged.

---

## The shape of this one

Seven findings: three P1, three P2, one P3. **All seven confirmed against the source.** That is two
audits running with a clean sheet.

What makes this round different from the five before it: **five of the seven are defects in the fix
I shipped the day before.** The shadow archive introduced in v1.9.59 closed the metadata tear and, in
doing so, opened three new ways to lose a save. The migration I wrote for old auras misread a whole
window of saves. The modal guard I added blocked one input path and not the other.

The auditor also pinned every conclusion to the published v1.9.59 commit while the workspace had
already moved to v1.9.60, and said so. That is the right call and it made the findings easy to
place — none of them were affected by v1.9.60's changes.

---

## The three P1s, all mine

### 1. The shadow failed OPEN

If staging the copy failed, the constructor logged "writing in place instead" and mutated the live
archive.

**A copy fails because the disk is full — which is the exact condition the shadow exists to
survive.** So the protection removed itself at the only moment it was needed. I wrote that fallback
deliberately, reasoning it was "a degradation, not a new failure". That reasoning was wrong: the
degradation lands precisely when the guarantee matters.

The second half is worse and I had not seen it at all. The staging check was:

```cpp
FileExists(target) && GetFileSize(target, &size) && size > 0
```

A **failed** `GetFileSize` on an existing archive collapsed into "the target does not exist" — so the
writer would build a *brand new* archive containing only the records this save writes, and publish it
over a real save. Everything else in that archive, the level records included, would have been
discarded.

Now fails closed in both directions. Any doubt about the target, or any failure staging the shadow,
and the writer opens nothing: `WriteFile` refuses, nothing publishes, the archive on disk is left
exactly as it was. Deliberately not `app_fatal` — this runs from the autosave, and killing the game
over one unstageable save turns a recoverable full disk into a lost session.

### 2. The shadow was published after an unchecked `fclose`

`LoggedFStream::Close()` discarded `fclose`'s result, and the publish is the very next thing.

Buffered data is flushed by the close, so **a disk that fills up on the final few kilobytes reports
itself there and nowhere else.** A shadow that had failed to write its tables was then swapped over
the good archive — the exact outcome the shadow was built to prevent.

`Close()` now returns a bool and the writer checks it. Sharpened by the fact that I had *already*
added exactly this check to the unpacked backend's `WriteFile` two days earlier, with a comment
saying it is where a full disk reports itself, and did not think to look for the same hole in the
packed one.

### 3. The unpacked commit ignored every rename

`CommitTransaction()` fired off its `ReplaceFileAtomically` calls and checked none of them, then
returned `true`. A batch where the second rename failed left the hero at the new generation and the
items at the old one — a mixed character — and reported success.

Now checked. On the first failure it stops (publishing more of an already-incomplete batch only
widens the mixture), cleans up the unswapped temporaries, and returns false.

**This does not make the batch atomic**, and the comment says so: a directory offers no way to swap
four files at once. What it does is stop the lie, so the caller keeps the stash dirty, tells the
player, and tries again — instead of announcing success over a character that is half of two saves.

---

## The three P2s

### 4. The aura migration misread v1.9.45–v1.9.57

The one I would have been most likely to defend, and the auditor is plainly right.

Tag 4 has had **two meanings**, and the width tells them apart:

| width | written by | numbering |
|---|---|---|
| one byte | before v1.9.45 | legacy, 163-row enum |
| two bytes | v1.9.45 onward | current, 273-row enum |

The same commit that renumbered the enum also widened the field, because the enum passed 255 rows. I
had that fact in front of me — my own comment said "two bytes since 2026-08-25; one byte before
that" — and still routed both widths through the legacy table whenever tag 13 was absent. That window
is v1.9.45 to v1.9.57 exactly. A Bard's Melody of Life is 195 there; the legacy table stops at 163,
so it was discarded.

Now the width is the discriminator. Two-byte tag 4 without tag 13 is read as current numbering, and
**validated rather than trusted** — bounds-checked and class-checked, the same refusals the other two
paths make.

Impact is limited: no *released* build sits in that window (v1.9.44 then v1.9.58), so only dev builds
are affected. The fix is still right and cost almost nothing.

### 5. The modal guard missed the virtual gamepad

`CanPlayerTakeAction()` covers the padmapper. The virtual gamepad builds its actions directly in
`GetGameAction`, so touch could still attack, cast and quaff behind a drop-gold prompt.

Fixed there — **and the other half the auditor raised is the one worth noting**: blocking a
controller from acting through a prompt, without giving it a way to answer the prompt, traps the
player in it. Physical controllers had no route to confirm or cancel a numeric prompt at all.
`TranslateControllerButtonToGameMenuKey` already maps A to Enter and B to Escape, which is exactly
what the prompt reads, so it is reused rather than duplicated.

### 6. F-key bindings still bypassed the skill-change autosave

The fifth audit found four readied-skill paths with no trigger; this is the fifth. Editing a binding
with the Abilities window open mutates `_pSplHotKey`/`_pSplLHotKey` directly. These persist in the
hotkeys record like everything else, and arranging a full set of F-keys is several minutes of
deliberate work to lose.

I saw this line during the previous round, noted it, and did not fix it.

---

## The P3, and why it matters more than its rating

### 7. The full-grid refusal was silent

The fifth audit's crafting fix correctly stopped consuming materials when there was no room for the
output — but it returned an empty string, and the caller only logs a *non-empty* one. So the
Transmute button appeared to do nothing at all.

**And my test explicitly accepted that**, with `result.empty() || result.find("room")`. I wrote an
assertion that permitted the bad outcome, which is a softer version of the vacuous-test problem the
reintroduction discipline exists to catch — the test was not vacuous, it was *lenient*, and leniency
in the assertion is how a silent failure gets blessed.

The refusal now returns the same "not enough room" message the sibling path already returned, and the
test requires a message rather than tolerating its absence.

---

## Testing

One new test, 557 → 558: `ATwoByteTagFourWithoutTagThirteenIsCurrentNumbering`, the v1.9.57-era
fixture the audit asked for. It reads the expected index **from the enum** rather than hard-coding
195, so it keeps testing the right thing when a class block next grows. Verified by reintroduction:
routing the two-byte value back through the legacy table fails it.

The crafting assertion was tightened from permissive to required.

`UNPACKED_SAVES` re-checked; both affected translation units still compile.

**Not covered, and worth stating plainly.** The audit listed six gaps in the v1.9.59 tests. This
round closes one of them. Copy failure, close failure and unpacked rename failure all need an
injection seam that does not exist — `LoggedFStream`'s countdown covers `Write` only, and
`CopyFileOverwrite`/`fclose` go nowhere near it. Extending the seam to cover close and copy is the
obvious next piece of work, and it is what would let three of this round's P1 fixes be pinned rather
than merely reasoned about. Virtual-gamepad and F-key-autosave coverage need live input and game
state respectively.

---

## The pattern worth recording

Five of seven findings were defects in a fix that was itself sound. The metadata tear was real, the
shadow was the right answer, and the shadow shipped with three new holes in it — every one of them in
the *error* paths, not the happy path.

That is the third time in three days a fix has needed a fix, and the common thread is not carelessness
about the mechanism but carelessness about what happens when the mechanism fails. The next thing to
build here is the seam that makes those paths testable.

# The injection seam now reaches the close and the copy (v1.9.62)

**Date:** 2026-08-26
**Version:** 1.9.61 → 1.9.62
**Tests:** 559, of which 557 pass — the two standing baseline failures, unchanged.

---

## Why

The sixth audit's report ended with a gap I wrote down rather than fixed: three of its P1 fixes could
not be pinned by a test, because the failure-injection seam reached `LoggedFStream::Write` and
nothing else. They were reasoned about, not demonstrated.

That is exactly the position the seam was built to get out of in the first place. A guarantee about
failure that has never seen a failure is a guess — and the previous three days had shown twice over
that my reasoning about error paths is the part that goes wrong.

---

## What the seam could not reach

Three operations, none of which goes through `LoggedFStream::Write`:

**The close.** Buffered data is flushed by `fclose`, so a disk that fills up on the last few
kilobytes reports itself there and at no earlier point. This is not reachable through the write
counter *by construction*: arming that makes a write fail, which is precisely the case this is not —
every individual write succeeds and the archive is ruined anyway.

**The staging copy.** `CopyFileOverwrite` never touches the stream wrapper.

**The size query.** Neither does `GetFileSize`, and the subtler half of the copy finding turned on
its failure being misread.

---

## What was added

Three seams, same shape as the existing one — a countdown, `-1` disarmed, compiled into every build
so the code the test exercises is the code that ships:

- `FailClosesAfter` / `StopFailingCloses` in `logged_fstream`
- `FailFileCopiesAfter` / `StopFailingFileCopies` in `file_util`
- `FailFileSizeQueriesAfter` / `StopFailingFileSizeQueries` in `file_util`

Two details worth recording, because both were found by the tests rather than designed in:

**The close seam still really closes the file.** Only the reported result is injected. A handle must
not leak because a test is pretending.

**A one-shot was needed for the size query.** `FailNextFileSizeQuery()` fails exactly one query and
disarms itself. The countdown form fails every query from its trigger onward, which is right for a
dead disk and wrong for one unreadable file — and the difference is not academic. The archive writer
queries a size twice while constructing, and failing both makes it give up at an *earlier* point than
the one under test, so the interesting path is never reached. With the fix reverted, the countdown
form made the test process `app_fatal` instead of failing an assertion, which is evidence of
misbehaviour but not evidence of the specific fault.

---

## The test

One new test, 558 → 559: `EveryFailureInThePublishLeavesThePreviousSaveWhole`. Four cases sharing one
assertion, which is the only one that matters — whatever goes wrong, the archive on disk is still the
last save that fully succeeded:

1. the close fails
2. the staging copy fails
3. the target cannot be measured
4. nothing fails, and the save still works

Case 4 is not decoration. Without it the whole test could be satisfied by a writer that had simply
stopped writing anything.

### What reintroduction showed

Reverting all three fixes at once fails all three cases, each under its own `SCOPED_TRACE`:

| reverted fix | assertion that fires |
|---|---|
| unchecked close | *a save that failed at the close was published over the good archive* |
| fail-open copy | *a writer whose staging failed accepted a record anyway* — and *damaged the archive it could not copy* |
| size-query collapse | *an archive that could not be measured was replaced by a fresh one* |

The third is the one worth dwelling on. It was the half of the audit's finding that I had not spotted
myself and had described in the report as a reasoned-about risk. It is not a risk — the reverted code
demonstrably **replaces a real save with a fresh archive containing only the current save's
records.** Everything else in it, level records included, is gone. That is now a line of test output
rather than a paragraph of my prose.

---

## Still not covered

**The unpacked backend's rename failure.** `UNPACKED_SAVES` is a compile-time alternative and is not
built by the test binary, so its `CommitTransaction` check is verified by compilation only. Reaching
it needs a second test configuration, which is a build-system change rather than a code one.

**The autosave state machine**, unchanged from the last two reports: file-local state, live triggers.

**Virtual-gamepad and F-key-binding coverage**, both needing live input or game state.

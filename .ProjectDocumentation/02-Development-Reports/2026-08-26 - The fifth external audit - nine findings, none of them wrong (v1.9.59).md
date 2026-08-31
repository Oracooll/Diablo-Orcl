# The fifth external audit — nine findings, none of them wrong (v1.9.59)

**Date:** 2026-08-26
**Version:** 1.9.58 → 1.9.59
**Audited:** the published v1.9.58 release, local commit `4e95996`
**Tests:** 556, of which 554 pass — the two standing baseline failures, unchanged.

---

## What arrived

Nine findings: four P1, five P2. Every one of them was verified against the source before a line
was changed, which is the standing rule here — the first audit in this series contained a claim that
was simply false, and two later recommendations were declined with reasons recorded.

**This is the first audit of the five with a clean sheet. All nine were real.** Two of them were
things I had built myself in the previous two days and got wrong; one was a build configuration that
had not compiled since I touched it and I had never checked.

---

## The four P1s

### 1. `UNPACKED_SAVES` had not compiled since the save transaction landed

`pfile.cpp` calls `BeginTransaction`/`CommitTransaction` unconditionally, and the unpacked
`SaveWriter` — a directory of loose files rather than an archive, which is how the RG99 port builds
— had neither. Four `C2039` errors.

I had added the transaction to `MpqWriter` and never once compiled the other backend.

Fixed by giving the unpacked writer the same contract: records written under temporary names, a
commit that renames them into place, a destructor that aborts anything uncommitted. **The comment
on it says plainly what it does not provide** — four renames are not one atomic act the way the
archive's single replacing rename is. What it does buy is that all the data is on disk before any
of it becomes visible, and a full disk fails during the writing, not during the renaming.

While there, `WriteFile` now checks `fclose`. Buffered data is flushed by the close, so that is
exactly where a full disk reports itself, and ignoring it is how a truncated record gets called a
success.

**Verified by reintroduction**: with the pre-fix header restored, the syntax-only build reproduces
the audit's exact errors at `pfile.cpp:512` and `:526`; with mine, both translation units compile
clean.

### 2. The save could still tear — at the very end, publishing the metadata

The transaction made the *records* all-or-nothing. But the header, block table and hash table are
written at close, one after another, straight into the live file. A failure between the block table
and the hash table leaves old hash entries — which name the old records — pointing at block entries
the commit has already changed.

The archive is then neither save, and it is not detectably either: the names resolve, they just
resolve to the wrong bytes.

No ordering of three writes fixes this. The guarantee needed is "all three or none" and a file
cannot give it. **So the whole session now works on a copy**, and the finished archive is swapped in
by one replacing rename. Every failure — a record, a table, the resize — leaves the original
untouched, because nothing ever wrote to it.

This needed a new primitive. `RenameFile` could not stand in: it returns `void`, so a caller cannot
tell a move that happened from one that did not, and on Windows it calls `MoveFileW`, which refuses
when the destination exists — the only case that matters here. `ReplaceFileAtomically` uses
`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`, and reports back.

Two things were checked rather than assumed. The copy is verified byte-count-equal before the shadow
is used, because `CopyFileOverwrite` reports nothing and building on a truncated copy and then
swapping it over the real archive would destroy the save — the exact outcome this exists to prevent.
And when the copy cannot be made, the writer falls back to editing in place, which is the old
behaviour: a degradation, not a new failure.

### 3. Character creation reported success after a failed save

`pfile_ui_save_create` wrote four records and returned `true` regardless, having already published
the name into `hero_names` before a single byte was written. A full disk at character creation
produced a character the menu listed, the game entered, and the archive did not contain.

Now wrapped in the same transaction, with the name claimed only after everything lands.

**The writer is scoped deliberately.** The commit only swaps records *inside* the archive; the
archive itself is published by the destructor. "Did this save happen" cannot be answered until the
writer is gone — so asking before the closing brace would have been wrong even with the commit
checked. The check is `!committed || SaveAttemptFailed()`, after the scope.

The UI already handled `false`: it shows "Unable to create character." That path had simply never
been reachable.

### 4. A full crafting grid plus a surplus stack destroyed the materials

The refine recipes' room check simulated each material *slot* emptying completely. Consumption is
unit-accurate and does not — a stack of four gems paying a cost of three leaves one behind, in the
slot the check had already written off.

On a full grid that is the difference between a craft and a theft. The preflight sees a free slot
that will not exist; the materials are consumed; the output loop finds nowhere to put the result and
returns empty-handed; and because the grid left behind is perfectly *valid*, the caller's rollback
sees nothing wrong and never fires. The player pays three gems for nothing and is told nothing.

The simulation is now the real thing — a copy of the grid with the real consumption run against it,
by the same function that does it for real, so the two cannot drift.

---

## The five P2s

### 5. Old saves lost their aura, and it was measured rather than guessed

Tag 4 stores an *absolute* `ClassTreeSkill` ordinal. v1.9.45 appended 110 passive rows, one batch at
the end of each class's block, which moved every ordinal after the Paladin's.

The numbers, extracted from the actual revisions rather than reasoned about:

| | old first | old count | new first |
|---|---|---|---|
| Paladin | 0 | 31 | 0 |
| Barbarian | 31 | 30 | 49 |
| Sorcerer | 61 | 30 | 98 |
| Rogue | 91 | 30 | 146 |
| Bard | 121 | 21 | 195 |
| Monk | 142 | 21 | 234 |

**132 enumerators shifted.** A Bard who saved at v1.9.44 with Melody of Life stored 121; 121 is now
a Rogue row, so the class guard threw it away and she loaded with nothing playing. Auras are not
Paladin-only — the Bard's songs and one of the Monk's are auras too — so for the four classes past
the Paladin this was every aura they had.

Two things were checked before writing the migration:

- **Every released build that had auras used one layout.** v1.9.31, v1.9.42, v1.9.43 and v1.9.44 all
  carry 163 rows with identical block starts. There is one legacy layout, not a series of them.
- **All 163 legacy rows still sit at the same index within their own class block.** The additions
  went on the end of each block throughout, so relative position was preserved. That is the property
  the whole migration rests on, and it was verified row by row rather than assumed from the design
  intent.

So the translation is a pure block-offset one into exactly the (class, relative index) pair tag 13
would have stored, and the existing path does the rest.

Tag 4 is now *held back* during the chunk walk and settled afterwards, so which representation wins
no longer depends on the order the two happen to appear in the file.

### 6. Controller actions went straight past open modals

The two mouse paths were taught to respect a numeric prompt. The controller was not: pad actions
dispatch through `CanPlayerTakeAction`, which did not check. Gamepad attack, spellcast and potion
quaffing all reached the world behind drop-gold, withdraw and "Refresh Until".

The check went into `CanPlayerTakeAction` rather than at the forty-odd call sites, because every one
of them is a keymapper or padmapper gameplay action and the answer is the same for all of them. The
prompt's own confirm and cancel do not come through this predicate.

### 7. The autosave backoff was defeated by ordinary play

Two problems in one variable. `ScheduleAfterSeconds` overwrote the pending time with "now" on every
trigger — so a failing save scheduled a retry sixty seconds out, the player picked up an item, and
the backoff that exists to stop a full disk being hammered every frame was gone. And
`ResetAutoSave` left `FailedSaveAttempts` alone, so a new session inherited the previous run's rung:
the next failure waited minutes instead of five seconds and, because the message only prints on the
first failure of a run, said nothing at all while it did.

Split into two variables with two meanings. `PendingSaveTime` is "the player did something worth
saving" and any trigger may move it. `RetryNotBefore` is "the disk said no" and no trigger may move
it at all. The gate sits after both due-checks so it covers the periodic interval too.

### 8. Four readied-skill changes never asked to be saved

Selecting Regular Attack in the picker, the shift-click that clears the RMB well, the shift-click
that clears the LMB well, and the Shift+F-key left-hand assignment — all four are the routes that
*clear* or are written out separately, and all four were missed.

Fixed at the four sites. **Full centralisation into setters is still declined**, for the reason
recorded earlier: twenty call sites include load and creation paths, where scheduling a save on
every assignment would be wrong.

### 9. The aura loop tracker desynchronised on every level change

Level loading and `FreeGame` called the sound layer directly, leaving `LoopedAura` describing a loop
that was no longer playing — and `SetAuraLoop` believes `LoopedAura`.

Both directions are audible. Silence without clearing it means the next tick sees "already playing"
and never restarts, so a lit aura goes quiet for good. Resume without setting it means the next tick
sees a change that did not happen, stops the loop it just resumed, and plays the transition cues
over a level load.

Two wrappers now own both states together, in the file that owns the tracker.

---

## Testing

Four new tests, suite 552 → 556.

**Every one was verified by reintroducing the bug**, and that discipline earned its keep again: the
first version of the metadata-tear test was **vacuous**. It let the header through and failed the
block table — and with the fix reverted it still passed, because old hash entries plus an old block
table still resolve to old data that the new records were appended clear of. Only the hash table
landing while the block table did not actually tears.

Rewritten, it no longer needs to be right about which write is the dangerous one: it sweeps every
boundary in the publish and asserts the previous save survives all of them. In that form, reverting
the fix fails it at the hash-table boundary — precisely the one the audit named. It also asserts the
success case, so it cannot be satisfied by a writer that never writes anything.

The other three:

- `ALegacyAbsoluteAuraIsMigratedForClassesPastThePaladin` — a **genuine legacy fixture**, hand-built
  as a v1.9.44 build actually wrote it: `OEXT` plus a single one-byte tag-4 chunk, no tag 13. This
  is what the existing round-trip test was not; that one writes with the current serializer and
  picks a Paladin, whose ordinals never moved.
- `AFullGridWithASurplusStackRefusesRatherThanEatingTheMaterials`
- `ReplaceFileAtomicallyOverwritesAndReportsBack`

The `UNPACKED_SAVES` configuration was checked by syntax-only compiles of both affected translation
units, driven from `compile_commands.json` so the flags match the real build.

---

## Not done

**The autosave state machine has no test.** Its state is file-local and its triggers need live game
state; the fix is by inspection only. Worth an extraction if it is touched again.

**A stray `.tmp` after a crash** is removed at the next open of that archive rather than at startup.
Harmless, but it means an interrupted save leaves a file behind until the next save.

**The stash still commits separately from the hero.** Two archives, two publishes; a failure between
them leaves a hero saved and a stash not. Standing on the pipeline, unchanged by this round.

# Audit 7, findings 4-7: proving contents instead of timestamps (v1.9.99)

**Date:** 2026-08-27
**Version:** 1.9.99
**Source:** `chatgpt audits/CHATGPT_DEEP_AUDIT_v1.9.97_2026-08-27.md`, findings 4-7

Build and release tooling. The theme the audit names correctly: modification times and name tables
were being treated as evidence of what an archive contains.

## Finding 4 - simultaneous packers shared a destructive temporary

`tools/oracool_mpq_pack.cpp` built into `outPath + ".building"` under a comment calling it "a unique
temporary". It is not one. Two packers aimed at the same archive shared that path, shared the
`MpqWriter` shadow derived from it, and shared cleanup that cannot tell whose file it is deleting.
Ninja serialises one producer inside one build; a manual repack beside a running build is an
ordinary thing to do.

**Fix.** The temporary is now `<out>.<pid>.<perf counter>.building`. The process id is what makes it
exclusive - two live processes cannot share one - and the counter separates successive runs of the
same pid after a crash left a shadow behind.

`tools/build_oracool_mpq.cmd` had the same problem for its response file, with
`%RANDOM%_%TIME:~9,2%` - 32,768 values and a clock in hundredths. cmd.exe has no exclusive file
create and no `$$`, but **`mkdir` is atomic and fails when the directory exists**, so the script now
claims a temp directory in a retry loop: whoever's `mkdir` succeeds owns it. And the packer's exit
code is captured rather than flattened by `|| exit /b 1`, so the response directory is removed on
**both** paths - the old form left it behind after every failed run and reported 1 rather than what
the packer actually returned. `build_devilutionx_mpq.cmd` got the same exit-code capture.

## Finding 5 - archive validation proved names, not contents

The post-pack check called `MpqArchive::HasFile()` for every expected name and stopped there. That
proves the **name table** and nothing else: an archive whose entries are structurally listed and
whose sector data is corrupt, truncated, or simply from a different build passed it.

`BuildReleasePackage.ps1` did less again for pre-existing artifacts - it checked that both archives
existed and were newer than their sources, and never opened either.

**Fix.** `VerifyArchiveContents()` opens the archive, reads every manifest entry back out with
`ReadFile`, and compares length and bytes with the file on disk. The packer runs it on the temporary
before the replace, and a new **`--verify`** mode runs the same code against an existing archive:

```
oracool_mpq_pack --verify <source_dir> <archive.mpq> (@listfile | files...)
```

Release packaging calls it for both archives - `devilutionx.mpq` against CMake's generated manifest,
`oracool.mpq` against a freshly walked list of the source asset folder - so there is one definition
of "this archive is correct" rather than two. Timestamps stay above it as an incremental-build
convenience and are no longer treated as evidence.

The audit's observation that Debug and Release `oracool.mpq` are byte-identical while the Release
one would be rejected on its timestamp is exactly the case this dissolves: content is now the
question.

## Finding 6 - the zip publication was not atomic

`BuildReleasePackage.ps1` created a uniquely named partial, **deleted** the published zip, then
moved the partial onto its name - under a comment claiming the output is "either the previous zip or
a complete new one". That is false for the interval between the delete and the move, where there is
no zip at all. A crash, permission error, antivirus lock or OneDrive race in that window loses the
last known-good package.

**Fix.** The finished partial is opened and its entry count compared with the staged file count, and
then `[System.IO.File]::Replace` performs a genuine same-volume replace, keeping the old file as a
backup until it succeeds. `Replace` requires the destination to exist, so a first-ever publish still
takes a plain `Move-Item` - safe, because there is nothing there to lose.

## Finding 7 - the wiki verifier masked its own second invariant

Two claims here, and they did not both hold up.

**The bundle-fingerprint failure did not reproduce.** `tools\BundleWiki.ps1 -Verify` at the audited
commit reports `wiki bundle: current (101 source files, 6a59b9b13a2e)` - the same stamped
fingerprint the audit quotes as the *bundle's*, matching the pages. The audit reports the pages
hashing to `f7ffddd9...` instead. The likeliest explanation is that the audit ran `BuildWiki.ps1`
first, which regenerates `data.js` and so changes the page set, and then read the resulting mismatch
as a committed-state defect. Recorded here rather than fixed, because there is nothing to fix.

**The masking is real.** The bundle check `exit 1`-ed where it stood, before the pages-versus-code
check ran - so a stale bundle hid the answer to the second question entirely, and the second is the
one that says whether what the player reads describes the game they have. Both invariants are now
evaluated and reported before either decides the exit code. Verified by appending a comment to
`wiki/index.html`: the bundle line reports STALE, the pages line still prints, and the exit code is 1.

**The pages were behind**, at 1.9.95 against 1.9.98. Regenerated at v1.9.99 and the bundle rebuilt;
both invariants now clean. The published artifact has been redeployed to the same URL, which is the
part a repo rebuild does not do.

## Verification

| Check | Result |
|---|---|
| pack a two-file tree, then `--verify` it | passes |
| edit a source file after packing, `--verify` | **fails**: "a.txt differs from its source" |
| touch a source file without changing bytes, `--verify` | passes |
| name an entry that is not in the archive | **fails**: "missing.txt is missing" |
| flip one byte inside an entry's stored data | **fails**: "a.txt differs from its source" |
| `build_oracool_mpq.cmd build\x64-Debug` | packs 416 files, leaves no temp directory |
| `BuildReleasePackage.ps1` parse check | clean |
| `BundleWiki.ps1 -Verify` with a drifted page | exit 1, **both** lines printed |

A byte flip in the archive's *slack* is deliberately not caught: `MpqWriter` pre-allocates around
60 KB of table space for a 5 KB payload, and those bytes are not entry data. The flip that matters
was located by finding the stored payload's offset and is caught.

Suite: **574/576**, the two standing baseline failures
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`) and nothing else.

## Not done, and why

- **Two-packer concurrency test.** The audit asks for a test that launches two packers at one output
  simultaneously. The exclusivity now rests on the process id, which cannot collide between live
  processes; a test would be asserting that property of the OS rather than of this code.
- **Sanitizer coverage** and **the two standing baseline failures** are the audit's finding-6 patch
  order item and need CI, which is unavailable until the Actions minutes reset.
- **Release rebuild.** The Release tree is still v1.9.92 and the packaging gate correctly refuses
  it. Rebuilding Release is a separate ask.

Not pushed: GitHub Actions minutes are exhausted until roughly 2026-09-01.

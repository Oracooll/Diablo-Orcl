# The test folder brought to v1.9.100, and a crash found on the way (v1.9.100)

**Date:** 2026-08-28
**Version:** 1.9.100
**Trigger:** user - "update folder `...\Diablo Orcl` to the latest build."

## What the folder was

A **Release** build at v1.9.92 - the 4.4 MB executable, with a blank `ProductVersion` because it
predates the VERSIONINFO resource added in v1.9.95. Meanwhile the only current build was Debug
v1.9.99, and that is not a drop-in: the Debug tree's DLLs are debug variants (SDL2 4.4 MB against
1.7 MB, plus a 16.6 MB `libdevilutionx_so.dll` Release does not produce), and the executable needs
`vcruntime140d.dll` / `ucrtbased.dll`, which are not in that folder.

Bringing it to the current version therefore meant building Release, which crosses the standing
"Debug only" rule. Asked, and approved as a one-off - the rule stands.

## The crash found on the way

Before deploying, both archives were checked with the `--verify` mode added in v1.9.99. The engine
archive verified. **`oracool.mpq` exited 0xC0000005** - an access violation, with no output at all.

It reproduced with the Debug build of the tool, so it was not a Release-only fault. Narrowing:

| Input | Result |
|---|---|
| the 416-entry list as written | **0xC0000005** |
| the same list written without a UTF-8 BOM | verified 416 files, exit 0 |
| a two-entry list, BOM | **0xC0000005** |
| a two-entry list, no BOM | exit 0 |
| a plainly missing entry name | exit 1, "is missing from" |
| a single name containing one byte >= 0x80 | **0xC0000005** |

So the BOM was only the delivery mechanism. **Any entry name with a high byte crashes libmpq**:
`libmpq__file_number`, which is all `MpqArchive::HasFile` is, faults instead of reporting "no such
file". An ASCII name that is absent reports correctly; the same name with one accented character
faults. libmpq is a third-party dependency and not this repository's to patch.

The BOM came from my own v1.9.99 work: `BuildReleasePackage.ps1` generated its verification list
with `Set-Content -Encoding utf8`, and Windows PowerShell 5.1 writes a BOM. So the archive-content
check I added to close audit finding 5 would have **crashed the release packager** the first time it
ran. It was never exercised end to end, because the Release tree was stale and packaging stops at
the version gate before reaching it.

**Three fixes, at the layer that can see the name:**

1. `IsPlainAscii()` guards every entry name before it reaches `HasFile`, in both the pack and verify
   paths - so any route to a non-ASCII name is a message, not a fault. No asset here has one, so
   refusing them costs nothing.
2. The list reader strips a UTF-8 BOM from the first line, alongside the CRLF and trailing-space
   tolerance already there for the same class of writer.
3. `BuildReleasePackage.ps1` writes its list with `WriteAllLines` and a BOM-less encoder.

Verified after the fix: the BOM list verifies clean, and the high-byte name exits 1 with
"entry name is not plain ASCII" instead of faulting.

## The deployment

Release rebuilt at v1.9.100. Both MPQs are in the build graph since v1.9.94, so they repacked
themselves; both then verified by **content** - 188 entries in `devilutionx.mpq`, 416 in
`oracool.mpq`.

Twelve files copied into the folder: `DiabloOrcl.exe`, the eight DLLs, both archives, and
`README.txt` stamped from `Packaging\windows\RELEASE_README.txt`.

**Deliberately not touched:** `diablo.ini` (the player's settings), `Saved_Games`, and the seven
Blizzard archives already sitting there - the copy loop refuses those by name rather than by
happening not to select them.

Confirmed after the copy: the executable reports `ProductVersion 1.9.100`, the README's first line
says v1.9.100, and both archives are SHA-256 identical to the build-tree copies that were verified.

## Verification

- Debug suite: **574/576**, the two standing baseline failures
  (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).
- `oracool_mpq_pack --verify` on both Release archives: clean.
- Wiki regenerated at v1.9.100, `BundleWiki.ps1 -Verify` clean on both invariants, artifact
  republished to the same URL.

The folder is ready to run. Not pushed: GitHub Actions minutes are exhausted until roughly
2026-09-01.

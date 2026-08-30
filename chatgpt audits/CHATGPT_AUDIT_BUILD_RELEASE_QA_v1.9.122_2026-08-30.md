# Build, release, and QA audit: v1.9.122

**Audited commit:** 2c3e8d15d0fdc475562024bfc9b2feee9eb8852a  
**Date:** 2026-08-30

## BR-01 — High — The archive-verification temp file is named before runId exists

### Observation

BuildReleasePackage.ps1 previously adopted a per-run GUID to stop concurrent packaging jobs from
deleting or replacing each other's staging files. The oracool.mpq verification list now includes
runId in its name, but that name is constructed 21 lines before runId is assigned.

The script does not enable Set-StrictMode. An uninitialized variable expands to an empty string, so
every process uses the same path:

~~~text
%TEMP%\oracool_verify_.txt
~~~

### Evidence

- tools/BuildReleasePackage.ps1:51 sets ErrorActionPreference but does not set strict mode.
- tools/BuildReleasePackage.ps1:205 constructs oracool_verify_$runId.txt.
- tools/BuildReleasePackage.ps1:210-212 truncates and rewrites that file.
- tools/BuildReleasePackage.ps1:213-216 passes it to the verifier and deletes it in finally.
- tools/BuildReleasePackage.ps1:226 assigns the GUID-backed runId for the first time.

### Race simulation

1. Packaging process A writes %TEMP%\oracool_verify_.txt.
2. Process B truncates/replaces the same file while A is starting or running the archive verifier.
3. Either process can then read a partial/wrong manifest or see the other process remove it.
4. Results range from nondeterministic false failure to verifying against another checkout's list.

This directly reintroduces the concurrency class the unique staging path was meant to eliminate.

### Recommended repair

Assign runId immediately after parsing version, before any temporary path is built. Better, create
one unique temporary directory for the complete packaging run and put the verification list,
staging tree, partial ZIP, and backup names under that run identity.

Enable Set-StrictMode -Version Latest near the top so any future use-before-assignment fails
immediately.

Add a packaging test that starts two verification/package processes against different synthetic
asset manifests and asserts that their temp paths and results are independent.

## BR-02 — Medium — ZIP “entry list comparison” compares only counts

### Observation

The final ZIP is opened and every non-directory FullName is collected, but the only validation is:

~~~text
zip entry count == staged file count
~~~

No entry name is compared with its expected relative path, and no final ZIP byte is compared with
the staged source. A ZIP with one missing file and one unexpected file has the correct count and
passes. Duplicate/wrongly prefixed names can pass for the same reason.

### Evidence

- tools/BuildReleasePackage.ps1:294-296 says the entry list is compared with staging.
- tools/BuildReleasePackage.ps1:297-302 actually collects the names.
- tools/BuildReleasePackage.ps1:307-310 checks only Count.

### Recommended repair

Build the expected ZIP FullName set from the staged directory, including the intended top-level
package-folder prefix. Normalize separators and compare exact sets:

- no missing names;
- no unexpected names;
- no duplicate file entries;
- expected directory/prefix shape.

For stronger assurance, extract to a unique temp directory or stream each ZIP entry and compare a
hash with its staged file. The MPQ preflight already proves archive contents byte-for-byte; the
transport ZIP should meet the same standard.

Add tests with equal counts but one renamed entry, one duplicate plus one missing entry, and a
wrong top-level directory.

## REL-01 — Release blocker — The Release tree is eight versions behind

The source and current Debug tree are v1.9.122, but the existing Release tree is still v1.9.114.

Observed artifacts:

| Artifact | Product version | Size | Last write UTC |
|---|---:|---:|---|
| build/x64-Debug/DiabloOrcl.exe | 1.9.122 | 10,263,040 | 2026-08-30 17:40:21 |
| build/x64-Debug/devilutionx.mpq | n/a | 4,123,041 | 2026-08-30 17:40:17 |
| build/x64-Debug/oracool.mpq | n/a | 35,729,453 | 2026-08-30 17:40:19 |
| build/x64-Release/DiabloOrcl.exe | 1.9.114 | 3,504,640 | 2026-08-30 03:46:25 |
| build/x64-Release/devilutionx.mpq | n/a | 4,123,041 | 2026-08-30 03:46:01 |
| build/x64-Release/oracool.mpq | n/a | 35,729,453 | 2026-08-30 03:46:01 |

This is not a defect in the version preflight: tools/BuildReleasePackage.ps1:145-152 should
correctly refuse the stale executable. It is an operational release blocker. Rebuild Release,
rebuild/verify both Release MPQs, run the full Release packaging script, and inspect the produced
ZIP before publishing.

## QA-01 — Medium — The regression baseline remains red at 577/579

### Full-suite result

- Serial CTest: **577 passed, 2 failed, 579 total**.
- Eight-way parallel CTest: **577 passed, 2 failed, 579 total**.
- No additional parallel-only failure appeared.

### Failure 1: Drlg_l1.CreateL5Dungeon_diablo_3_844660068

The failure was re-run alone with gtest_repeat=2 and reproduced identically both times:

- first tile mismatch at 1x0: actual 22, fixture 4;
- ENTRY_MAIN ViewPosition actual {67,72}, expected {67,52};
- ENTRY_PREV ViewPosition actual {73,83}, expected {85,45}.

Evidence:

- test/drlg_l1_test.cpp:49-58 loads diablo/3-844660068.dun and checks both entries.
- test/drlg_test.hpp:52-62 initializes a one-player full-quest game.
- test/drlg_test.hpp:64-80 creates the dungeon and compares the golden tile layer.
- Source/options.cpp:1358 defaults Randomize Quests to false.

Running alone rules out parallel contamination and prior-test state. Randomize Quests is false by
default, so it should not be presented as an established root cause. The committed
Source/levels/drlg_l1.cpp is unchanged from the 1.5.5 baseline; the next useful step is a clean
baseline build with the same data archives, followed by a bisect or an asset/initial-state
comparison. Do not merely regenerate the fixture until the divergent dungeon is explained.

### Failure 2: Timedemo.WarriorLevel1to2

The test fails with “Unable to load character.”

test/timedemo_test.cpp:21-62 points config and preference paths at the old fixture folder, loads the
recorded hero, starts playback, and compares the final hero. The fork's packed hero/save data has
grown substantially, while this fixture predates those changes. The likely maintenance action is to
regenerate or migrate the timedemo hero fixture under the current format, then decide whether the
recorded input is still a valid gameplay golden.

Even if this is fixture staleness rather than a shipping-game defect, it removes the only
end-to-end gameplay replay from the green test signal and should not remain indefinitely waived.

## Successful checks

- Current x64-Debug build completed at v1.9.122.
- Current Debug executable carries ProductVersion 1.9.122.
- Current Debug devilutionx.mpq verified all 188 expected entries byte-for-byte.
- Current Debug oracool.mpq verified all 416 expected entries byte-for-byte.
- Serial and parallel suites fail in the same two places only.
- The packaging script correctly has version-resource validation, per-run stage/partial ZIP names,
  MPQ byte verification, commercial-archive rejection, and atomic replacement of an existing
  published ZIP. BR-01 and BR-02 are narrow gaps in an otherwise materially improved pipeline.

## Suggested release gate

Before marking another version Latest:

1. Source version, Debug ProductVersion, and Release ProductVersion must match.
2. Both Release MPQs must pass byte verification against their current manifests.
3. All expected ZIP names and bytes must match staging, not only the count.
4. CTest must be green, or each temporary exception must have an owner, root cause, and expiration.
5. Run the package process twice concurrently in CI at least once to exercise its isolation
   guarantees.

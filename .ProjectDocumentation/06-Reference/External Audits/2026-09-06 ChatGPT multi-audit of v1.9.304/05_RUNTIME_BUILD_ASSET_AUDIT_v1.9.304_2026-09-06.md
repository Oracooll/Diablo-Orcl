# Relocated runtime, build, package, and generated-data audit

Audit date: 2026-09-06  
Runtime/build directory: `C:\Diablo Orcl\x64-Debug`  
Source directory: `C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1`

## Binary identity

- `DiabloOrcl.exe` file version: `1.9.304`
- Product version: `1.9.304`
- Size: 10,807,296 bytes
- Last write: `2026-09-06T03:59:57.6393281+03:00`
- SHA-256: `EAFDBE229DAC70DA3D6E596F558D7A056B8FC29F11C9CA2648251104CF0DF23A`
- Source `ORACOOL_VERSION`: `1.9.304`
- Source HEAD: `c659976d69906489c34c28c636b477f14f424bb6`

The executable predates the final documentation-only audit commit by about a minute, but its embedded version matches the v1.9.304 source. This is version alignment, not a reproducible-build proof that the exact bytes came from this Git commit.

## Build relocation

The CMake cache in the relocated build still points its source directory to the OneDrive repository. That is expected for a moved binary directory as long as the source remains there. A Ninja dry run requested only a CMake regeneration step (`Re-checking globbed directories` / `Re-running CMake`); it did not report a direct compile failure. Because regeneration was not performed during this read-only audit, the dry run cannot prove that no compile edges would appear after configure.

Recommended release discipline:

1. Regenerate CMake at `C:\Diablo Orcl\x64-Debug` after moves or source updates.
2. Build from a clean or recorded worktree.
3. Run the full tests.
4. Save the commit hash, build options, executable hash, and both MPQ hashes in release metadata.

The cache enables AddressSanitizer and UndefinedBehaviorSanitizer for Debug; ThreadSanitizer is off. Keep a sanitizer lane, but note that intra-object overwrites such as NET-01 may need canary assertions because ASan may not flag them.

## Full test baseline

Command used:

```text
ctest --test-dir C:\Diablo Orcl\x64-Debug --output-on-failure -j 8
```

Result: 636 discovered cases, 634 passed, one failed, one skipped, total about 6.38 seconds.

Failure: `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`

- `dungeon[1][0]`: actual 22, expected 4
- one view position: actual `(67,72)`, expected `(67,52)`
- another view position: actual `(73,83)`, expected `(85,45)`

Prior baseline comparison identifies this as inherited dungeon golden/fixture skew, not a new Oracool behavior regression. It should still be repaired or explicitly rebased so a red full suite is never normalized.

Skipped: `Timedemo.WarriorLevel1to2`, intentionally obsolete in this build.

## Focused mechanics simulation

Separate current-binary runs produced:

- Gems + runewords: 17/17 passed.
- Skill points + class tree: 30/30 passed.
- Cold pack + chill + cold mechanics: 3/3 passed.
- Rogue arrows + melee skills + warcries: 3/3 passed.
- Stack/tab/stash-focused inventory filters: 20/20 passed.

Total: 73/73. The class-tree run emitted missing-audio warnings because that direct headless invocation did not mount the runtime archives; the tests themselves passed, and independent archive verification confirms those assets exist in the package.

## MPQ/source-manifest integrity

The shipped verifier was run against the corresponding source directories and file manifests:

- `oracool.mpq`: 409 of 409 manifest files verified.
- `devilutionx.mpq`: 188 of 188 manifest files verified.
- Manifest counts exactly matched actual source-tree file counts: 409 and 188.

This confirms the recent removal of 44 dead assets did not leave missing or extra manifested content. It does not decode and visually inspect every image/audio asset; it validates archive membership/content against the build inputs.

## Generated table exports

The executable's own diagnostic modes generated fresh TSV data:

- Skill facts: 142 rows, every row five columns, no duplicate full rows.
- Class distribution: Barbarian 38, Bard 8, Monk 22, Paladin 20, Rogue 36, Sorceress 18.
- Runewords: 370 rows, every row five columns, no duplicate full rows, no duplicate names, no empty bonus column.

These are clean schema/content checks. They do not validate every numeric balance choice.

## Save-directory hygiene

At inspection time, `C:\Diablo Orcl\x64-Debug\Saved_Games` contained nonzero hero/stash archives and telemetry. No zero-length save, `.tmp`, `.partial`, `.previous`, or `.bak` residue was found. Screenshots were normal user data and were not altered.

## Release gates suggested

- Make the full suite green: update/fix the inherited dungeon golden and explicitly mark the timedemo status.
- Add whole-binary shuffled/repeated tests (see the test-harness report).
- Add conservation tests for every item transfer boundary.
- Add ID-boundary tests whenever `MAX_SPELLS`, packet fields, or legacy save arrays differ in extent.
- Verify both MPQs and both generated TSVs during packaging.
- Record hashes so a relocated binary can be tied to a source snapshot without inference.

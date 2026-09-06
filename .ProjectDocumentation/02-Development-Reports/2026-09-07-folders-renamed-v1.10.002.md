# The two folders renamed: "Diablo Orcl" and "Resources" (v1.10.002)

**Date:** 2026-09-07. The user: "two updates - main folder renamed to Diablo Orcl and oracool.mpq folder renamed to Resources."

- The repository folder is `Diablo Orcl` (was `Diablo Orcl V1` - the rename that had been pending since the v1.9.143 product-string pass). `tools\RenameProjectFolder.ps1`, written for that rename, is retired.
- The art folder beside it is `Resources` (was `Oracool.MPQ`). 38 tools named it by path or in their comments; all repointed with one case-sensitive substitution (the archive `oracool.mpq` is lowercase and untouched). Its README's title records the old name.
- Both build trees carried the old source path in their CMake caches. The Debug tree was reconfigured from the new path through `CMakeUserPresets.json` (`cmake --preset x64-debug-local`) and rebuilt; the Release tree's cache was cleared so its next configure starts clean (its objects stay).
- The memory notes moved with the session to the new project directory: the twelve newer notes were synced across, the index merged, and every path in them rewritten to the new names; the "rename pending" note is closed.
- Historical documents - briefs, plans, reports, the filed audits - keep the names they were written under.

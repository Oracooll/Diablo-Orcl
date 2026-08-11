---
title: 2026-08-09 - Version Bump on Rebuild Policy
date: 2026-08-09
tags: [dev-report, policy, build-system]
summary: Established a standing rule that every rebuild bumps ORACOOL_VERSION's patch number, and backfilled the bump for this session's earlier rebuilds.
---

# Version Bump on Rebuild Policy

## Context

Project owner set a standing rule: every time the project is rebuilt, `ORACOOL_VERSION` bumps from `1.0.x` to `1.0.x+1`, so the in-game version shown on the main menu always identifies exactly which build is running.

## What changed

- [`ORACOOL_VERSION`](../../ORACOOL_VERSION) bumped from `1.0.0` to `1.0.1`, backfilling the several rebuilds already done earlier this session (executable rename, resolution sort order, Fit to Screen dedup) before this rule was stated.
- Reconfigured both `build/x64-Debug` and `build/x64-Release` (`cmake <build-dir>`, no target specified) before rebuilding, and rebuilt the `devilutionx` target in both.

## Why the reconfigure step is required

`CMakeLists.txt:61-63` reads the version via:
```cmake
if(NOT ORACOOL_VERSION_STR)
  file(STRINGS "ORACOOL_VERSION" ORACOOL_VERSION_STR)
endif()
```
`ORACOOL_VERSION_STR` is a plain CMake variable, not a `CACHE` variable, and `file(STRINGS ...)` reads are not automatically tracked as configure-time dependencies. Editing the `ORACOOL_VERSION` file alone does not cause Ninja to detect that CMake needs to re-run — an explicit `cmake <build-dir>` reconfigure is required before `cmake --build` will pick up the new value. This is now the documented procedure for every future version bump.

## Verification

Both `build/x64-Debug` and `build/x64-Release` reconfigured without errors and rebuilt successfully (`devilutionx` target, exit code 0 both times).

## Going forward

This is now a standing rule for all future sessions on this project, saved to session memory: bump `ORACOOL_VERSION`'s patch digit before or as part of every rebuild, including ad-hoc verification builds, not just intentional releases.

## Related

- [[2026-08-09 - Rename Executable to DiabloOrcl]] — the rebuild workflow this reconfigure step was added to

---
title: 2026-08-09 - Rename Executable to DiabloOrcl
date: 2026-08-09
tags: [dev-report, build-system]
summary: Renamed the Windows executable output from devilutionx.exe to DiabloOrcl.exe without touching the internal CMake target name or other platforms.
---

# Rename Executable to DiabloOrcl

## Context

Project owner requested the shipped Windows executable be renamed from `devilutionx.exe` to `DiabloOrcl.exe`, and asked whether it would break anything.

## What changed

- [`CMakeLists.txt`](../../CMakeLists.txt) — added a `WIN32`-only block right after the executable target is defined:
  ```cmake
  if(WIN32)
    set_target_properties(${BIN_TARGET} PROPERTIES OUTPUT_NAME "DiabloOrcl")
  endif()
  ```
  The internal CMake target name stays `devilutionx` (`BIN_TARGET`) — only the Windows output filename changes. This matters because `BIN_TARGET` is shared with Vita, PS4, 3DS, and Android platform logic elsewhere in the same file; renaming the target itself would have cascaded into all of them.
- [`Packaging/windows/README.txt`](../../Packaging/windows/README.txt) — updated the player-facing "Run devilutionx.exe" instruction to "Run DiabloOrcl.exe".

## Why not a full target rename

Changing `BIN_TARGET` itself (rather than just `OUTPUT_NAME`) would have required touching every other platform's packaging logic in the same `CMakeLists.txt`, plus two GitHub Actions workflows that reference `devilutionx.exe` by name for CI artifact packaging (`Windows_MSVC_x64.yml`, `Windows_MinGW_x64.yml`) — out of scope for a Windows-only rename and not yet updated (see Not done, below).

## Verification

- Confirmed save games and settings are unaffected: `Source/utils/paths.cpp:90` calls `SDL_GetPrefPath("diasurgical", "devilution")` with hardcoded strings, not derived from the executable filename.
- Built the `devilutionx` CMake target for both `build/x64-Debug` and `build/x64-Release` (using `vcvarsall.bat x64` to get a proper MSVC linker environment, since the default shell lacks `LIB`/`INCLUDE`). Both linked successfully, producing `DiabloOrcl.exe`, `DiabloOrcl.lib`, and `DiabloOrcl.pdb`.
- Deleted the stale `devilutionx.exe`/`devilutionx.pdb` left over from before the rename in both build output folders.
- Confirmed `build/` is gitignored, so none of this touched version control.

## Not done / deliberately left alone

- `.github/workflows/Windows_MSVC_x64.yml` and `Windows_MinGW_x64.yml` still reference `devilutionx.exe` by name for CI artifact packaging — only relevant if/when this fork starts using those GitHub Actions builds.
- `Packaging/windows/devilutionx.rc` and `devilutionx.exe.manifest` were left with their original filenames on disk — they're source file names, not the compiled output name, so renaming them wasn't necessary.

## Related

- [[2026-08-09 - Version Bump on Rebuild Policy]] — the rebuild workflow established here (`vcvarsall.bat` + `cmake --build`) is reused by every later report.

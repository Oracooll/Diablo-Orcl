# The Debug tree moved out of OneDrive

**Date:** 2026-09-06
**Requests:** "can we move only the x64Debug folder out of OneDrive? all else including possible Release build stay in OneDrive." then "there is a folder C:\Diablo Orcl. Move the debug builds there."

## What was done

- The Debug tree is now `C:\Diablo Orcl\x64-Debug`. The repository and any Release build under `build/` stay in OneDrive. No source change; only the two MPQ tools' default build folder moved.
- Carried over: `diabdat.mpq`, `hellfire.mpq`, `hfmonk/hfmusic/hfvoice.mpq`, `devilutionx.mpq`, `diablo.ini`, `multi_0.sv`, and `Saved_Games` (39 files: heroes, stash, screenshots, the telemetry CSV), hash-verified twice before the OneDrive copy was deleted. `oracool.mpq` is built by the tree itself.
- Fresh configure: Ninja, Debug, the Visual Studio vcpkg toolchain, `DISABLE_ZERO_TIER=ON`, `BUILD_ASSETS_MPQ=OFF`, `BUILD_TESTING=ON`, and the compiler PINNED to MSVC 14.52. The first configure took MSVC 14.51 from the plain vcvars path and failed on `runes_effects.inc` with C7560 (designated initialisers out of member order), which 14.52 accepts and the old tree had been using all along. The recipe is in the memory note and in `tools\build_oracool_mpq.cmd`'s comment.
- `tools\build_oracool_mpq.cmd` and `tools\build_devilutionx_mpq.cmd` default to the new folder when no in-tree Debug cache exists; an explicit argument still wins.

## Verified

v1.9.297 exe in the new tree; suite there 634/635, the standing dungeon-generation failure only. OneDrive's build folder is empty.

## Note

The ghost-process check now looks for a handle on `C:\Diablo Orcl\x64-Debug\oracool.mpq`.

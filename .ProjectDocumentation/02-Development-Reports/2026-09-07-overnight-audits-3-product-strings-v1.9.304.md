# Overnight audits, batch 3: the last "Oracool Edition" strings; a configure preset (v1.9.304)

**Date:** 2026-09-07

## Product name

The rename to Diablo Orcl (v1.9.143) missed seven user-facing strings: the three incompatible-save messages (loadsave.cpp), the no-multiplayer notice (menu.cpp), the settings category's display name and description and the Auto Save description (options.cpp), and the main menu's version line (diablo.cpp, "Oracool Edition v" -> "Diablo Orcl v"). All say Diablo Orcl now. The ini SECTION key "Oracool Edition" is deliberately unchanged: renaming it would orphan every existing diablo.ini's settings. `oracool::EditionName` (unused) follows.

## Configure preset

`CMakeUserPresets.json` (new, gitignored - it names this machine's paths) carries the Debug tree's recipe: Ninja, `C:/Diablo Orcl/x64-Debug`, the vcpkg toolchain, MSVC 14.52 pinned, ZeroTier off, asset MPQ off, tests on. A reconfigure is `cmake --preset x64-debug-local` under vcvars64; it would have saved the 14.51 detour on 2026-09-06.

Suite 635/636, the standing dungeon-generation failure only.

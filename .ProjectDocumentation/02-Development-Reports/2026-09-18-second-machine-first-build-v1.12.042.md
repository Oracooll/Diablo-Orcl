# The second machine's first build: GemData rows in declaration order (v1.12.042)

**Date:** 2026-09-18 - Debug only - **819 of 819 tests** - MSVC 14.51, the only toolset on this machine.

## What the new computer had

The Debug tree came over on the SSD whole - objects, `vcpkg_installed`, the Blizzard archives, ten heroes - and
with it the old machine's `CMakeCache.txt`, pinned to MSVC 14.52.36520 and to a Windows SDK (10.0.28000) that
this machine does not have. Visual Studio 18 Community here installed **14.51.36231** and SDK 10.0.26100 only. So:
the stale cache and `CMakeFiles` went, `CMakeUserPresets.json` (ignored by git - each machine keeps its own) had
its two compiler lines pointed at 14.51, and `cmake --preset x64-debug-local` configured from nothing. vcpkg
rebuilt its packages for the different compiler in 3.6 minutes; everything else came from the cache.

## C7560, met for real

The reason 14.52 was pinned on 2026-09-06 was that 14.51 rejects out-of-order designated initialisers. It does:
27 errors in `Source/oracool/runes_effects.inc` and `jewels_effects.inc`, the generated `GemData` rows, whose
designators followed each rune's D2 effect order rather than the struct's. C++20 requires declaration order;
14.52 merely lets it through. The fix is at the source, so the toolset no longer matters:

- **`tools/GemDataOrder.ps1`** (new) - `Sort-GemDesignators` reads the member order from `struct GemData` in
  `Source/oracool/gems.cpp` on every run (no second list to keep in step) and sorts a row's designators into it;
  a designator naming a member the struct does not have stops the generator instead of reaching the compiler.
- **`tools/GenRunes.ps1`, `tools/GenJewels.ps1`** dot-source it and sort at the one line each that emits a row.
- **`runes_effects.inc`, `jewels_effects.inc`** regenerated. Checked row by row against the committed files:
  every row keeps the same designators with the same values, only the order differs (28 rune rows, 15 jewel
  rows); no other generated file changed, and the encodings (runes with BOM, jewels without, both CRLF) match.

The four follow-on errors at `gems.cpp:145` were the `Gems[]` array failing to initialise; they went with it.

## What still does not build here, and why

`oracool.mpq` will not pack: the packer reads every file under `Packaging/resources/oracool_assets`, and 49 of
them are OneDrive placeholders this machine has not downloaded (attribute O; hydrating them fails with "the cloud
operation was unsuccessful"). All 49 are **untracked** leftovers of mid-August experiments - `fonts/oracool_yellow*.trn`,
`ui/border2_*`, `ui/panel_frame_*`, `levels/l1data/sunless_1.pal`, two Paladin holy-bolt wavs - and **no code
references any of them**; they were only ever packed because the glob takes everything. Every tracked file is local.
OneDrive is plainly not finished: the old machine's deletions of `build/x64-Debug` (2026-09-06) and `wiki/`
(2026-09-17) have not arrived either (3,173 and 57 ghost placeholders), nor have three untracked `tools/*.ps1`.
The packer leaves the previous archive untouched on failure, so the copied `oracool.mpq` (packed today on the old
machine, from the same assets) is the current one.

`DiabloOrcl.exe` depends on the pack (`add_dependencies`), and so do its own three compile steps, so ninja never
reached them. They were run by hand with ninja's own commands (`ninja -t commands`, under vcvars): `main.cpp`,
the two `.rc` resources, the link and the vcpkg applocal copy. The exe is the real thing - **FileVersion 1.12.042** -
but ninja has no log entry for those outputs, so the next `cmake --build` redoes that minute of work, and until
the 49 files hydrate (or are deleted on the old machine, where they are real) it also reports the pack failure.

Two smaller traps for the record: a batch variable named `RC` is what CMake reads as the resource compiler
(`CMAKE_RC_COMPILER not set` with "compiler 0"), and a `printf` format is no place for `C:\...\18\...\x64` (`\1`,
`\x6`, `\v` are escapes) - write `.cmd` headers with a quoted heredoc.

## Verification

- `ctest`: 819 of 819 passed (55 s), including the shuffled whole-binary lane; one gtest discovery timed out at
  5 s during the fully parallel first build and passed on the rerun (0.07 s alone).
- `C:\Diablo Orcl\x64-Debug\DiabloOrcl.exe` reports 1.12.042 in its version resource.
- Not verified: the game was not launched (the user's rule).

## Not done / left alone

- No OneDrive file was deleted, replaced or hydrated by hand; the 49 leftovers are the user's to delete.
- MSVC 14.52 was not installed; with the rows in order it is not needed.
- The `test-saves-*` folders from the old machine's test runs are still in the Debug tree.

## Related

- [[2026-09-18-necromancer-n11-numbers-roadmap-and-census-v1.12.041]] - the build this one follows

## Follow-up the same night: v1.12.043, the pack unblocked

The user's word on the 49 leftovers: "Delete those files, rebuild." All 49 were checked against `git ls-files`
(none tracked) and removed, with the emptied `fonts/` and `levels/` folders; nothing tracked changed. The glob is
`CONFIGURE_DEPENDS`, so the configure picked up the shorter list on its own. Clean run, no hand steps:
configure, `oracool.mpq` packed, `DiabloOrcl.exe` linked by ninja, **819 of 819 tests**, exe reporting
**1.12.043**. The Debug tree on this machine is now a normal one.

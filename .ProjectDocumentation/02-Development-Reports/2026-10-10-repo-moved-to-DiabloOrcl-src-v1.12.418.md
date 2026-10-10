# v1.12.418 - the repository moves into DiabloOrcl-src (2026-10-10)

At the user's word, `Diablo Orcl` became an umbrella folder; the repository moved one level down and the sibling folders came in beside it:

```
Software\Diablo\Diablo Orcl\
    DiabloOrcl-src\        <- the git repository (was Diablo Orcl\ itself)
    Resources\             <- was Software\Diablo\Resources
    DiabloOrcl RTM\        <- was Software\Diablo\DiabloOrcl RTM
    Diablo Orcl Website\   <- was Software\Diablo\Diablo Orcl Website
    Diablo Orcl Vanilla\   <- was Software\Diablo\Diablo Orcl Vanilla
```

Because Resources and the RTM folder stay siblings of the repository, every tool that reaches them as `..\Resources` / `..\DiabloOrcl RTM` works unchanged. Three tools carried absolute paths and were repointed: `tools/BuildFontColourLegend.pl` (repo + palette), `tools/CutShopGlyphs.ps1` (Resources\Icons), `tools/session-scripts/necromancer/necro_rfa17.js` (RfA-17 output). `tools/OracoolPcxWatcher.cfg` still names the long-gone in-tree `build\x64-Debug` and was left as it was.

The Debug tree (`C:\Diablo Orcl\x64-Debug`, outside OneDrive) recorded the old source path, so `CMakeCache.txt` and `CMakeFiles\` were deleted and it was reconfigured with `cmake --preset x64-debug-local` from DiabloOrcl-src (vcvars 14.51). Saved_Games, diablo.ini and the MPQs in that tree were not touched. Full build clean, ctest 957/957, diablo.ini md5 unchanged.

An empty `uwp-project` folder reappeared in the umbrella root after the move (a OneDrive sync leftover); it was removed (the real one is in DiabloOrcl-src).

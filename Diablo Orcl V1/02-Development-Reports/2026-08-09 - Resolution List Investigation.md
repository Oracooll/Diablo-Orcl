---
title: 2026-08-09 - Resolution List Investigation
date: 2026-08-09
tags: [dev-report, investigation, graphics-options]
summary: The curated resolution list the project owner expected in V1 was already present in source, inherited from before V0 reverted it — the issue was unverified builds, not missing code.
---

# Resolution List Investigation

## Context

Project owner reported that the graphics options resolution list in-game didn't show any change from vanilla DevilutionX, despite expecting a curated resolution list to be part of V1 (per prior discussion in V0's chat history).

## Investigation

- Searched V0's git history (`git log --grep resolution`) and found commit `264c72b`: "Revert Furious Charge and curated resolution list; course-correct to vanilla-faithful (v0.5.0)" — this **removed** a curated/floored resolution list from V0, explicitly to move that work into the new V1 fork instead (see [[V0 to V1 Fork]]).
- Checked that revert commit's parent: it is `f2a347e`, the exact commit V1 forked from.
- Confirmed `f2a347e:Source/options.cpp` (i.e. the state at the fork point) **already contains** the full curated resolution list — `CuratedResolution` struct, `CuratedResolutions[]` array (22 entries across 5 aspect ratios, 960×720 floor), `SnapToNearestCuratedResolution()`, and the `OptionEntryResolution` methods that use them.
- Confirmed the current working tree (`Source/options.cpp` in this repo) still has all of it, and V1's only fork-related commit (`40a1ce0`) touched nothing but `ORACOOL_VERSION`.
- Confirmed the compiled object file (`build/x64-Release/Source/CMakeFiles/libdevilutionx.dir/options.cpp.obj`) was newer than the source, so the existing build should already have reflected the feature.

## Conclusion

The feature was never missing from V1's source — it was inherited automatically from the fork point, before V0 later reverted it. The most likely explanation for the project owner not seeing it in-game was an unverified/stale local build rather than missing code.

## Verification

Built and launched `DiabloOrcl.exe` from `build/x64-Release`, navigated in-game to Settings → Graphics, and screenshotted the resolution option to confirm the curated list was active in the running build.

## Not done / deliberately left alone

This report only establishes that the list *exists* and *renders*. The list's display order and the Fit to Screen labeling behavior were addressed as follow-up requests — see [[2026-08-09 - Resolution List Sort Order]] and [[2026-08-09 - Fit to Screen Duplicate Resolution Cleanup]].

## Related

- [[V0 to V1 Fork]]
- [[2026-08-09 - Resolution List Sort Order]]
- [[2026-08-09 - Fit to Screen Duplicate Resolution Cleanup]]

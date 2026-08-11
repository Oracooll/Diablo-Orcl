---
title: 2026-08-09 - Resolution List Sort Order
date: 2026-08-09
tags: [dev-report, graphics-options]
summary: Changed the curated resolution list's display order from fixed aspect-ratio blocks to sorted by height then width, per the project owner's exact 22-entry specification.
---

# Resolution List Sort Order

## Context

Following [[2026-08-09 - Resolution List Investigation]], the project owner supplied an exact specification for the curated resolution list:

- 4:3 (7): 960×720, 1024×768, 1152×864, 1280×960, 1400×1050, 1600×1200, 1920×1440
- 16:9 (4): 1280×720, 1600×900, 1920×1080, 2560×1440
- 21:9 (3): 1680×720, 2560×1080, 3440×1440
- 16:10 (4): 1280×800, 1440×900, 1680×1050, 1920×1200
- 3:2 (4): 1152×768, 1440×960, 1920×1280, 2160×1440

...sorted by height then ratio, with 960×720 as an unconditional floor (nothing below it selectable or loadable).

Comparing this against `Source/options.cpp`'s existing `CuratedResolutions[]` array showed the **values were already an exact match** — all 22 entries, same numbers, same groupings. What didn't match was the *display order*: the existing code iterated the array in its literal declaration order (grouped in fixed aspect-ratio blocks: all 4:3, then all 3:2, then all 16:10, then all 16:9, then all 21:9), not sorted by height.

## What changed

[`Source/options.cpp`](../../Source/options.cpp), inside `OptionEntryResolution::CheckResolutionsAreInitialized()`:

- Replaced the two parallel `std::vector<Size> sizes` / `std::vector<const char*> ratioLabels` with a single `std::vector<CuratedResolution> filtered`, simplifying the filtering and fallback logic.
- Added a `std::sort` on `filtered`, ordering by `size.height` ascending, then `size.width` ascending as the tiebreak (equivalent to sorting by aspect ratio for entries that share a height, since width alone determines the ratio once height is fixed).
- Updated the doc comment above `CuratedResolutions[]` to note the array itself stays grouped by ratio for source readability, but the *displayed* list order now comes from the sort in `CheckResolutionsAreInitialized()`, not the array's declaration order.

The `CuratedResolutions[]` array's contents were left completely unchanged — no value edits were needed since they already matched the specification exactly.

## Verification

Rebuilt the `devilutionx` CMake target for both `build/x64-Debug` and `build/x64-Release`. Both compiled cleanly — `options.cpp` recompiled with only 3 pre-existing `C4267` warnings, all in unrelated code (lines 1122/1699/1918 at the time, far from this change) that predate this session.

## Not done / deliberately left alone

The project owner's message stated "22 entries" by enumeration but summarized it as "14 total options" — a likely miscount, since the itemized list (7+4+3+4+4) sums to 22, matching what was already in code. Went with the full enumerated 22-entry list rather than guessing at a shorter one; flagged the discrepancy back to the project owner rather than silently picking an interpretation.

## Related

- [[2026-08-09 - Resolution List Investigation]]
- [[2026-08-09 - Fit to Screen Duplicate Resolution Cleanup]]

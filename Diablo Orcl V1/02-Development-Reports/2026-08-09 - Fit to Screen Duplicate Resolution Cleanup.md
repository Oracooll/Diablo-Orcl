---
title: 2026-08-09 - Fit to Screen Duplicate Resolution Cleanup
date: 2026-08-09
tags: [dev-report, graphics-options]
summary: Resolution entries sharing a height showed the same "XXXp" label 2-4 times when Fit to Screen was on; deduplicated since they already resolved to byte-identical final resolutions.
---

# Fit to Screen Duplicate Resolution Cleanup

## Context

Project owner observed that the full `WxH (ratio)` resolution label (e.g. `960x720 (4:3)`) only appears when the Fit to Screen option is OFF. Asked whether this should change. Investigation showed the real problem was more specific than the label format: with Fit to Screen ON, multiple curated entries sharing a height (e.g. `960x720`, `1280x720`, `1680x720` all being height `720`) were each rendered as their own list entry, all showing the identical label `720p` — a meaningless duplication, since the project owner's actual complaint (once clarified) was about redundant entries, not the label format itself.

## Why the duplicates are true duplicates, not just a label collision

In Fit to Screen mode, the code recomputes each entry's width from `size.height * desktopWidth / desktopHeight` — the *original* curated width is discarded entirely. Two curated entries with the same height therefore produce the exact same final `(width, height)` pair once Fit to Screen stretches them, not just the same label. Confirmed this before making the change by checking `OptionEntryResolution::GetActiveListIndex()`, which matches selections by the *stretched* `Size` value, not the original curated one — so deduplicating same-height entries changes nothing about which resolution ends up selected.

## What changed

[`Source/options.cpp`](../../Source/options.cpp), inside `OptionEntryResolution::CheckResolutionsAreInitialized()`'s Fit to Screen branch:

- Added a `lastFitToScreenHeight` tracker (declared once before the loop, `#ifndef USE_SDL1`).
- Since `filtered` is already sorted by height (see [[2026-08-09 - Resolution List Sort Order]]), same-height entries are guaranteed adjacent — the loop now skips an entry if its height matches the immediately preceding one, so only the first (smallest-width) entry per distinct height produces a list entry.

Result: with Fit to Screen ON, the list now shows 11 entries (one per distinct height: 720p, 768p, 800p, 864p, 900p, 960p, 1050p, 1080p, 1200p, 1280p, 1440p) instead of repeating some heights up to 4 times. With Fit to Screen OFF, nothing changed — the full 22-entry `WxH (ratio)` list from [[2026-08-09 - Resolution List Sort Order]] is unaffected.

## Verification

Rebuilt `devilutionx` for both `build/x64-Debug` and `build/x64-Release` — both compiled cleanly with no new warnings.

## Related

- [[2026-08-09 - Resolution List Sort Order]]
- [[2026-08-09 - Resolution List Investigation]]

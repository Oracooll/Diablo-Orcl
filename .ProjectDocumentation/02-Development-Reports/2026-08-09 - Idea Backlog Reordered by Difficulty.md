---
title: 2026-08-09 - Idea Backlog Reordered by Difficulty
date: 2026-08-09
tags: [dev-report, idea-backlog]
summary: Reorganized all 30 Idea-Backlog.md entries into five difficulty tiers (Trivial to Very Hard), fixing positional cross-references that the old thematic ordering had made fragile.
---

# Idea Backlog Reordered by Difficulty

## Context

[[Idea-Backlog]] had grown to 30 entries across two sessions, organized by save-format impact (non-save-breaking first, save-breaking batched at the end, per the user's 2026-08-06 direction). Project owner asked for the whole backlog reordered in ascending difficulty instead — easiest first — since the list is now long enough to need a build-order lens.

## What changed

- Assigned every entry a `Difficulty` tag (Trivial / Easy / Medium / Hard / Very Hard), estimated from each entry's own description of scope, dependencies, and how many new systems/fields it touches.
- Restructured the file into five tier sections (`## Tier 1 — Trivial` through `## Tier 5 — Very Hard`), 3/7/7/8/5 entries respectively.
- Kept the non-save-breaking/save-breaking classification, but demoted it from the primary sort key to an inline `Save impact` tag per entry — the 2026-08-06 "batch save-breaking work together" guidance still applies when scheduling actual implementation, it's just not what orders the file anymore.
- **Fixed a real bug the reorder would otherwise have introduced silently**: several entries referenced each other with positional language ("see Barbarian below," "same as Skills above"). Under the old thematic ordering those were accurate; under difficulty ordering they'd become wrong without any visible error — e.g. "Skills" (rated Very Hard) was cited as being "above" several Easy/Medium entries that would now sort ahead of it. Replaced every positional reference with a named one ("see the Barbarian entry," "same as the Skills entry") so the file no longer breaks silently on a future reorder.
- Updated "How to use this file" to instruct future entries to insert into the matching difficulty tier (not append chronologically) and to always reference other entries by name, not position.

## Why

A silent cross-reference break is worse than an obvious one — someone reading "see Salvaging above" after the reorder would find Salvaging several tiers away, with no indication anything had moved. Fixing this now, while every reference was already being touched for the reorder anyway, was cheaper than leaving it for a future session to discover as a confusing dangling reference.

## Verification

- Counted `### ` headings per tier (`3 + 7 + 7 + 8 + 5 = 30`), confirming no entries were dropped or duplicated during the rewrite.
- Re-ran the same wikilink-resolution check used in prior documentation sessions across the whole vault — no new broken links introduced, only the same four pre-existing false positives (template placeholder text and backtick-quoted literal examples).

## Not done / deliberately left alone

Difficulty estimates are a first-pass judgment call from each entry's existing description, not the output of a real design pass — several entries already say their own scope is unresolved (Item Tiers' open questions, the Randomized bonus dungeon's "most speculative" framing), so tiers may shift once those get worked through properly.

## Related

- [[Idea-Backlog]]
- [[2026-08-09 - Town, Shops, Classes, and Save-Load Backlog Additions]]
- [[2026-08-09 - Diablo 2 and 3 Mechanics Brainstorm]]

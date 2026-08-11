---
title: 2026-08-08 - Fork to Oracool Edition V1
date: 2026-08-08
tags: [dev-report, project-history]
summary: Diablo Ocrl V0 split into a vanilla-faithful V0 line and a new, UI-overhaul V1 line; this repository is that V1 fork.
---

# Fork to Oracool Edition V1

## Context

Diablo Oracool Edition had been developing both quality-of-life features (faithful to vanilla Diablo's presentation) and features that deliberately depart from vanilla's UI (e.g. a non-standard minimum resolution) in the same repository. The project owner decided to split these into two independently versioned lines rather than keep mixing them.

## What changed

- Repository `Diablo Ocrl V0` continues as the vanilla-faithful line, version `0.x.xxx`.
- A new local repository, `Diablo Orcl V1` (this one), was branched from V0 at commit `f2a347e` ("Add CEL decoder and MPQ extractor tools for the UI-overhaul asset pipeline").
- Commit `40a1ce0` ("Start Oracool Edition V1 - the 960x720 UI-overhaul line (v1.0.0)") marks the start of the V1 line. It changed only the `ORACOOL_VERSION` file, from V0's then-current value to `1.0.0`.
- No GitHub remote was configured for V1 at fork time — local-only.

## Why

Per the V0 revert commit `264c72b`'s message: "Splitting the project going forward: this repo (V0, 0.x.xxx) stays a faithful-to-vanilla QoL mod; a new fork ... (V1, 1.x.xxx, local-only for now) is where the deliberately vanilla-departing UI overhaul work will live instead." Full detail in [[V0 to V1 Fork]].

## Verification

Confirmed via `git log` on both repositories: V1's second commit (`f2a347e`) matches V0's history exactly up to that point, and V0's subsequent commit `264c72b` (dated 2026-08-08, after the fork) reverts exactly the features described above from V0 while V1 retains them.

## Related

- [[V0 to V1 Fork]]
- [[2026-08-09 - Resolution List Investigation]]

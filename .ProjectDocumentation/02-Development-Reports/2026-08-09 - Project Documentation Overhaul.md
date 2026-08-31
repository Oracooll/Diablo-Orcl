---
title: 2026-08-09 - Project Documentation Overhaul
date: 2026-08-09
tags: [dev-report, meta, documentation]
summary: Created .ProjectDocumentation as an Obsidian-friendly vault, migrated all of _ProjectLibrary/Documentation into it via git mv, and retired the old folder.
---

# Project Documentation Overhaul

## Context

Project owner asked for a new `.ProjectDocumentation` folder, professionally structured, Obsidian-friendly, with detailed reports on everything done during V1's development, clear to a third party reading it cold. Follow-up decision: migrate the existing `_ProjectLibrary/Documentation/` (inherited from V0 at the fork) into the new structure, and retire the old folder rather than keep both around.

## What changed

- Created `.ProjectDocumentation/` at the repo root with this structure:
  - `00-Meta/` — [[About This Documentation]]
  - `01-Project-Overview/` — [[Project-Scope]], [[Feature-Catalogue]], [[Design-Decisions]], [[V0 to V1 Fork]]
  - `02-Development-Reports/` — this report and all prior V1 session reports
  - `03-Releases/` — the V0-era `v0.x.x` release records
  - `04-Changelog/` — [[CHANGELOG]]
  - `05-Gameplay-and-Features/` — [[Gameplay-Changes]], [[Migration-Plan-1.5.4-to-1.5.5]]
  - `06-Reference/` — [[Build-Instructions]], [[Testing-Guide]], [[Future-Development-Roadmap]]
  - `07-Backlog/` — [[Idea-Backlog]]
  - `08-Implementation-Reports/` — [[Oracool-Edition-v0.1.0-Implementation-Report]]
  - `Templates/` — [[Development Report Template]]
- Migrated all 23 git-tracked files from `_ProjectLibrary/Documentation/` into the new structure using `git mv`, preserving file history. Two `.txt` release-README files were renamed to `.md` in the process (`Release-v0.1.0-README.txt` → `v0.1.0-Player-README.md`, same for `v0.2.0`) for Obsidian consistency; release note files had their redundant `Release-` prefix dropped since the containing folder name already conveys that.
- Removed the now-empty `_ProjectLibrary/Documentation/` directory. Follow-up request: removed the rest of `_ProjectLibrary/` too (`Builds/`, `Research/`, `Screenshots/`) — checked first and confirmed each contained nothing but a placeholder `.gitkeep`, no real content, via `git rm -r`. `_ProjectLibrary/` no longer exists in this repo.
- Wrote [[Home]] as the vault's map-of-content entry point, [[About This Documentation]] explaining structure and conventions for future contributors, [[V0 to V1 Fork]] synthesizing the fork's history from git log across both repositories, and backfilled dev reports for every unit of work done this session: the fork itself, the executable rename, the resolution list investigation, the resolution sort-order change, the Fit to Screen dedup, and the version-bump policy.

## Why

The project owner's stated goal was a documentation system a third party could read cold and understand what happened and why. Obsidian's wikilink/backlink/graph-view model works best with small, atomically-scoped notes that link to each other rather than one large document, which shaped both the new-note format (see [[Development Report Template]]) and the decision to write one report per unit of work rather than a single running log.

## Verification

- `git status` after the migration confirms every moved file shows as a rename (`R`), not a delete+add, meaning `git log --follow` will still show history through the move.
- Cross-checked every `[[wikilink]]` written in the new notes against the actual filenames on disk (`find .ProjectDocumentation -iname "*.md"`) and fixed three mismatches where a link used a spaced title (`[[Project Scope]]`) against an actually-hyphenated filename (`Project-Scope.md`).
- Confirmed the two `.md`-mention references inside the migrated `Gameplay-Changes.md` and `Testing-Guide.md` are plain prose filename mentions, not real markdown links, so moving the files didn't break anything there.

## Not done / deliberately left alone

- The migrated legacy files (`CHANGELOG.md`, `Gameplay-Changes.md`, release notes, etc.) were moved as-is, not rewritten or given frontmatter — they're large, and their content wasn't in question, only their location.
- No `.obsidian/` vault-config folder was created; the vault works from plain folder structure and wikilinks alone, so opening `.ProjectDocumentation/` in Obsidian works without any pre-existing config.

## Related

- [[About This Documentation]]
- [[Home]]

---
title: About This Documentation
tags: [moc, meta]
---

# About This Documentation

This vault exists to make the development history of Diablo Orcl V1 legible to anyone who wasn't in the room when the work happened — including a future version of the people who were.

## Structure

| Folder | Contents |
|---|---|
| `00-Meta/` | This file and other notes about the vault itself |
| `01-Project-Overview/` | What the project is, its scope, its standing design decisions |
| `02-Development-Reports/` | Dated, atomic reports — one per unit of work, written as it happens |
| `03-Releases/` | Player-facing release records (inherited from V0 up to the fork point) |
| `04-Changelog/` | The running feature changelog (inherited from V0 up to the fork point) |
| `05-Gameplay-and-Features/` | Detailed gameplay change documentation and the engine migration plan |
| `06-Reference/` | Build instructions, testing guide, roadmap |
| `07-Backlog/` | Ideas not yet scheduled |
| `08-Implementation-Reports/` | Point-in-time full-project implementation reports |
| `Templates/` | Reusable note templates |

## Where this came from

Everything outside `02-Development-Reports/` and this file was migrated on 2026-08-09 from `_ProjectLibrary/Documentation/`, which V1 inherited when it forked from V0 at commit `f2a347e`. That folder has been retired — this vault is now the single documentation location for V1. See [[2026-08-09 - Project Documentation Overhaul]] for the full migration report.

V0's own documentation continues to live in V0's repository and is not duplicated here; V1's copy is a point-in-time snapshot as of the fork plus everything written since.

## Conventions for future reports

- **One report per unit of work**, filed in `02-Development-Reports/`, named `YYYY-MM-DD - Short Title.md`.
- **Every report gets frontmatter**: `date`, `tags`, and a one-line `summary`.
- **Link liberally** with `[[wikilinks]]` — to related reports, to the overview notes, to anything relevant. Obsidian's backlink panel and graph view are how a reader navigates this vault, so under-linking makes notes harder to find.
- **Write for a third party**: state what changed, why, how it was verified, and what wasn't done or was deliberately left alone. Don't assume the reader watched the work happen.
- Start from [[Development Report Template]] rather than an empty file.
- Update [[Home]]'s Development Reports list when a new report is added.

---
title: 2026-08-09 - Town, Shops, Classes, and Save-Load Backlog Additions
date: 2026-08-09
tags: [dev-report, brainstorm, idea-backlog]
summary: Recorded a 10-item feature list (town rework, shop UI, stash, salvaging, crafting cube, new UI, skills, class rename/addition, save-load removal) to Idea-Backlog.md, reconciling three items against work already done or already floated.
---

# Town, Shops, Classes, and Save-Load Backlog Additions

## Context

Project owner supplied a 10-item list of feature requests, ending with "More - later..." signaling this is an ongoing backlog dump rather than an instruction to start building. Per [[Idea-Backlog]]'s own stated process, ideas are recorded as they're floated without waiting for a request, and only get a real design pass (open questions answered) when picked up for actual work — nothing in this batch was implemented.

## What changed

Recorded all 10 items in [[Idea-Backlog]], but three needed reconciliation against existing content rather than a fresh entry:

- **Item 4 (Salvaging)** and **item 7 (Skills for each character)** were already backlogged (floated 2026-08-05) — marked "re-confirmed 2026-08-09" instead of duplicating. Skills also gained a note: introducing skills "for each character" implies per-class trees, relevant now that a new class (Barbarian, below) is also on the table.
- **Item 5 ("Levski's Cube")** was recognized as the same idea as "Horadric Cube-style fixed recipes," added to the backlog in the previous session's D2/D3 brainstorm — renamed to Levski's Cube (the project's own branding) and rewritten to explicitly position it as the interaction point tying Salvaging (material source) and Crafting (transformation) together, rather than a fourth unrelated mechanic.
- **Item 6 (new UI for the 960x720 canvas)** overlaps the already-backlogged "HUD/UI rearrangement" entry (floated 2026-08-08) — but that entry proposed raising the resolution floor to **1024x768**, while this session's actual shipped work (see [[2026-08-09 - Resolution List Investigation]]) floors the curated resolution list at **960x720**, matching what the project owner just wrote. Corrected the entry's target to 960x720 rather than silently keeping the stale number or silently picking one without flagging the discrepancy.

New entries added: Tristram NPC repositioning (item 1), Diablo 2-style shop interface (item 2), Stash relocation near the well (item 3), Warrior→Paladin rename (item 8), Barbarian class cloned from the Warrior sprite (item 9).

Item 10 (remove save/load entirely) was folded into the existing "Autosave-only play" entry rather than added as new, since they're the same idea — but expanded with an explicit fork in interpretation, because the two readings have very different risk profiles:
- Hiding the manual save/load UI while keeping autosave (matches what Diablo 3 and Belzebub actually do from the player's perspective, and is non-save-breaking).
- Removing persistence entirely (a much larger architecture change with real data-loss risk, and not actually what "Diablo 3 has no save/load" describes — D3 still remembers your character, it just has no save-slot screen).

Flagged this explicitly in the entry rather than assuming either interpretation.

## Why

Two corrections (items 4/7 duplication, the 960x720 vs. 1024x768 conflict) and one high-stakes ambiguity (save/load removal) were worth surfacing now, in writing, rather than silently picking an interpretation during a future implementation pass — consistent with the backlog's own stated rule: "if it's genuinely unclear until a real design pass happens, say so in the entry rather than guessing."

## Verification

N/A — planning artifact only, no code or build changes. Nothing was implemented.

## Not done / deliberately left alone

No design pass was performed on any of these items — no open questions were answered, no implementation started. The project owner signaled more items are coming ("More - later..."), so this remains an open, growing list rather than a finished one.

## Related

- [[Idea-Backlog]]
- [[2026-08-09 - Diablo 2 and 3 Mechanics Brainstorm]]
- [[2026-08-09 - Resolution List Investigation]]

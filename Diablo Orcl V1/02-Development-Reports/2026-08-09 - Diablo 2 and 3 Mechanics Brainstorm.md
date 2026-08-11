---
title: 2026-08-09 - Diablo 2 and 3 Mechanics Brainstorm
date: 2026-08-09
tags: [dev-report, brainstorm, idea-backlog]
summary: Brainstormed an inexhaustive list of Diablo 2/3 mechanics to modernize the game, cross-checked against existing backlog and vanilla features, and appended 15 new ideas to Idea-Backlog.md.
---

# Diablo 2 and 3 Mechanics Brainstorm

## Context

Project owner asked for an inexhaustive list of Diablo 2 and Diablo 3 game mechanics that could be added to make the game feel more modern.

## What changed

Before drafting new ideas, checked what was already accounted for:

- [[Idea-Backlog]] already had several D2/D3-inspired ideas floated: skill trees/synergies, Set items, Item Tiers (Normal/Exceptional/Elite), Unique Legendary Powers, Crafting, Salvaging, and a movement-speed system.
- Grepped the actual codebase to confirm vanilla D1/Oracool already has resistances (`IPL_FIRERES`/`IPL_LIGHTRES`/`IPL_MAGICRES`/`IPL_ALLRES` in `itemdat.h`), three difficulty tiers (`DIFF_NORMAL`/`DIFF_NIGHTMARE`/`DIFF_HELL`), and an existing `Auto Gold Pickup` option (`options.cpp:1249`) — excluded these from the new list since they're not missing mechanics. Confirmed no existing socket concept in `items.h`, so Sockets/Runewords/Jewels are genuinely new ground.

Appended 15 new ideas to [[Idea-Backlog]], following its existing format and non-save-breaking/save-breaking classification framework:

**Non-save-breaking:** Resistance penetration/soft cap, Enchanting (single-affix reroll), Elective Mode (free skill-to-hotkey binding), Elite monster affixes, Health Globes, Waypoints/fast travel, Gambling, Paragon-style post-cap progression.

**Save-breaking:** Socketed items/gems/jewels, Runewords (depends on sockets), Charms, Horadric Cube-style fixed recipes, Transmogrification, Mercenaries/Followers, Randomized bonus dungeon (light Rift-style).

## Why

Each entry follows the backlog's established reasoning: player-scoped new data (Paragon points, unlocked waypoints) can live in its own absent-tolerant save file and stays non-breaking; item-intrinsic new data (a socket, a transmog target, a follower's own gear) needs an `OracoolItemFormatVersion` bump. Where the save impact was genuinely unclear (Charms, Horadric Cube recipes), defaulted to the save-breaking bucket per the backlog's own stated rule rather than assuming a shortcut.

## Verification

N/A — this is a planning artifact, not a code or build change. Nothing was implemented.

## Not done / deliberately left alone

Nothing was scoped for implementation. Per [[Idea-Backlog]]'s own process, each idea gets its own design pass (open questions listed and answered) only when the project owner chooses to pick it up.

## Related

- [[Idea-Backlog]]

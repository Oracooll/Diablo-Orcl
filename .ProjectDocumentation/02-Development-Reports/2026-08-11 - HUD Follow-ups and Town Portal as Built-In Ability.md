---
title: 2026-08-11 - HUD Follow-ups and Town Portal as Built-In Ability
date: 2026-08-11
tags: [dev-report]
summary: Seven play-test follow-ups - edge-based rect scaling (fixing the click-highlight strip), Furious Charge on by default, Town Portal demoted to a button-only built-in ability, the LOG button folded into the burger menu, the clock moved top-left, and the XP counter relocated above the belt with percentage readouts.
---

# HUD Follow-ups and Town Portal as Built-In Ability

## Context

A batch of remarks from the user after play-testing v1.0.67. Item 8 on their list (the gargoyle and angel reading as too large relative to their spheres) is not addressed here - the user is producing replacement orb art.

## Rect scaling: the black strip, and the icon that looked off-centre

`ScalePlate` was applied to a rect's position and size *separately*, truncating twice, so a rect could land a pixel short on the right and bottom. That showed as a thin black strip down the right edge of the Menu/Portal click highlight, and left the skill wells a pixel narrower than the art's actual opening.

Replaced with `ScalePlateRect()`, which scales the source rect's **edges** and derives the size from them. Rects now sit flush with the art and adjacent cells tile without seams.

Measuring the RMB icon against the screenshot showed it was in fact centred already (6px above, 7px below in a 51px well) - the apparent offset was this one-pixel rect error plus the icon's own artwork not being centred within its sprite. Worth re-checking in play now the rect is exact.

## Furious Charge

The option defaulted to **off**, so the Paladin's free slot was still rendering as vanilla Item Repair (anvil icon, "Item Repair Skill" tooltip). The Heal Other icon substitution and the "Furious Charge" name were already wired - they were simply never active. Default flipped to on.

## Town Portal demoted to a built-in ability

Per the user: Town Portal "should only live through the portal button... more of a built-in ability rather than a spell in the classical sense". New shared predicate `oracool::IsBuiltInPortalAbility()` (`oracool/oracool.h`), single-player only to match the always-memorized zero-mana behaviour it rides on. Applied at three points:

- **Spell book pages** (`panels/spell_book.cpp`) - the row is skipped, leaving a gap rather than reflowing, since pages are a fixed spell-per-slot layout.
- **SpeedBook ring** (`GetSpellListItems`, `panels/spell_list.cpp`) - which also removes it from F5-F8 hotkey assignment, as that selects from the same list.
- **Book generation** (`GetBookSpell`, `items.cpp`) - filtered by predicate rather than by jumping the enum index (the technique the existing multiplayer-only skips use), so it does not depend on enum adjacency. Covers both loot and Adria's stock.

**Left open:** Scroll of Town Portal and Staff of Town Portal still generate. Their entries are now hidden from the SpeedBook by the same filter, though scrolls remain usable directly from the belt or inventory. Whether to strip them from loot tables too is a decision for the user - it is a larger change to item generation than was asked for, and they are merely redundant rather than broken.

## Layout moves

- **LOG button** removed; an "Event Log" entry added to the belt's Menu popup (now 11 entries). The log window keeps its position - the row under the mini-map that the button used to occupy is now simply the window's own top edge.
- **Game clock** moved from under the mini-map to the screen's top-left corner.
- **XP counter** moved from under the mini-map to the strip directly above the belt's numbered cells, spanning cells 1-4 and centred over them, clearing the plate's baked-in "1".."4" labels by 2px. The press-and-hold click target moved with it.

## XP percentages

Both XP readouts now carry a percentage against the experience gap between the current level and the next:

- Normal: remaining-to-next-level and what share of the level that is - `2,000 / 100%` on a freshly-levelled character.
- Held: the experience still alive on the floor, and what share of a full level it represents - so a glance tells you whether clearing the level will level you up.

## Verification

Debug build clean, `ORACOOL_VERSION` 1.0.68. Needs a play-test pass: the click highlight's right edge, RMB icon centring, the Paladin's slot showing Furious Charge with the Heal Other icon, Town Portal's absence from book and SpeedBook while the belt button still casts, and the relocated clock / XP counter.

## Related

- [[2026-08-10 - Compact Middle HUD Redesign]]
- [[2026-08-11 - Fixed Mini-Map Frame Size]]

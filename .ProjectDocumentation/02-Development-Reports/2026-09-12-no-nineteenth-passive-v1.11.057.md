# v1.11.057 - no nineteenth passive: two unbuilt passives leave, two pages close up

2026-09-12. The user: "i dont want 19th (lvl 36) skill. Move all lvl36 skill to lvl30 tier." Asked how to make room, since both level-30 rows were full, they chose "Drop an unbuilt one".

## What moved

**Barbarian.** Boon of Bul-Kathos (unbuilt) left the page. The three passives after it moved up one cell:

| Passive | Cell | Level |
|---|---|---|
| Earthen Might | (5,0) | 32 |
| Sword and Board | (5,1) | 34 |
| Rampage | (5,2) | 36 (was 38) |

**Rogue.** Ballistics (unbuilt) left the page. The four passives after it moved up one cell:

| Passive | Cell | Level |
|---|---|---|
| Leech | (4,2) | 30 |
| Ambush | (5,0) | 32 |
| Awareness | (5,1) | 34 |
| Single Out | (5,2) | 36 (was 38) |

Every page of every class now fits the 3x6 grid, and the most passives any class has is 18, the last arriving at 36.

## How a skill leaves a page

`RetiredFromTreePage = -1` in class_tree.h, not deletion. A row's POSITION is its identity: the icon strip, saved skill points and passive slots all index by it, so a deleted row would have shifted every row after it. What a retired row still affects:
- `BuildClassTreePage` never asks for page -1.
- `IsPassiveSkillRow` is false for it.
- `PassiveIndexOnPage` counts only page-3 rows in table order, so the passives after it arrive a cell sooner. Table order is already the new reading order.
- `PassiveInSlot` validates on the way out, so a save with one of them slotted drops it.
- `GetClassTreePageName` does not index an array.
- The `--skill-facts` dump skips spell-less rows.

## Tests

- `EveryPageFitsTheThreeBySixGrid` now expects nothing outside the grid. It also checks: both retired rows are off the passive page, Rampage and Single Out arrive at 36, and Leech arrives at 30.
- `EveryPageIsPopulatedAndGridPositionsAreUnique` counts the rows on no page (exactly 2), and pages plus off-page must add up to all 272 rows.

## Verification

Debug and Release built, ctest **714/714**. **Not seen in play.**

The **RTM exe was not refreshed**: the user was playing from the RTM folder. Copy it once the game is closed.

**To check:** the Barbarian and Rogue Passive Skills pages hold 18 cells with no seventh row, and Rampage / Single Out show level 36.

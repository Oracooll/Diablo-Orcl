---
version: v1.12.115
date: 2026-09-21
area: UI / towners
tests: 832/832
---

# The eight permanent townspeople hover as their own names

## The ask

> The 8 permanent town NPCs - shorten their hover pop-up name to just their pure name.

## Changed

`towner.name` is what the hover pop-up reads, so the eight are edited there and nowhere else:

| Was | Now |
| --- | --- |
| Griswold the Blacksmith | Griswold |
| Ogden the Tavern owner | Ogden |
| Adria the Witch | Adria |
| Gillian the Barmaid | Gillian |
| Wirt the Peg-legged boy | Wirt |
| Pepin the Healer | Pepin |
| Cain the Elder | Cain |
| Farnham the Drunk | Farnham |

## Left alone, deliberately

**The conditional townspeople.** The ask was the eight PERMANENT ones, and the rest of the table are not: the
Wounded Townsman stands only while the Butcher quest is live, Lester and the Complete Nut are the two mutually
exclusive Hellfire farmers, Celia needs the Theo quest and a visit to level 17. They keep their names, as do the
three cows.

**Two store HEADINGS**, which are not hover text: `stores.cpp` prints "Wirt the Peg-legged boy" and "Farnham the
Drunk" as the titles of their dialog screens. Only those two vendors still show a heading at all - Griswold's was
removed with his redesign - so they are now the only long names left on screen. Flagged to the user rather than
changed, because the ask named the hover.

## Consequence worth recording

These eight strings are translated in roughly twenty `.po` files against their OLD text. Changing the msgid means
those translations no longer match, so a non-English build falls back to the English name until the template is
regenerated. No effect on the English build this project runs.

## Build

Debug, clean. 832/832.

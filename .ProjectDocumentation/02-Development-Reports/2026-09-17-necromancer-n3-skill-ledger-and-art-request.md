# The Necromancer, phase N3: the skill ledger and art request A

**Date:** 2026-09-17 - no code, no build (the game stays at v1.12.033). N2 was confirmed in play ("it is all good").

## The skill ledger

A new section of *The Road to Necromancy* (https://claude.ai/artifact/85RWSQWqGn9CK2EhR1gsWq, version 3):
his four pages laid out as the in-game tree lays them out (six rows by three columns, level labels), one tab a
page with a "looked at" count. Opening a skill edits its name, description, kind, what it is paid in (mana /
Essence / nothing) and its Essence cost, its cell (moving onto a taken cell SWAPS the two - the occupant is moved
first, so a failed second write leaves a doubled cell, never a lost skill), a verdict (keep / change / replace)
and notes.

Data: db collection `skills`, 72 documents keyed by the ClassTreeSkill enum name, seeded from scratchpad
`necro_rows.js` via `necro_seed.js`. The "Not yet built: awaits ..." sentence is held apart in `awaits`, and
"Paid in Essence." became `pays` + `cost` with the proposed prices (Corpse / Poison Explosion 10, curses 25,
Revive 35).

**Reading it back:** before building a page, read `skills` with ArtifactData and apply name / description /
cell / kind changes to `class_tree.cpp` (+ `class_tree.h` only if a row is REPLACED - an enum rename; positions
in the enum never move). `pays`/`cost` feed `EssenceCost`.

## Art request A

`Resources/ChatGPT RfA/RfA-17 - The Necromancer (draft).md`, written by scratchpad `necro_rfa17.js`:

- batch 37 - portrait `ui_art/hero6.png` (180 x 76, the override loop already looks for it) and
  `ui/silhouette_necromancer.png` (needs one line in `SilhouetteForClass` on intake);
- batch 38 - twelve missile / effect sheets in RfA-16's format and a 14-cell curse-marker strip. Poison is
  yellow-brown like the delivered acid sheets, because the palette has no green;
- batch 39 - 72 glyphs in RfA-13's strict format, ON HOLD until the ledger is settled; the script regenerates
  the table from the final rows.

Not sent anywhere - the user sends RfAs.

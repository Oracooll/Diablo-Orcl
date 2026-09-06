# Overnight audits, 2026-09-07: summary (v1.9.302 - v1.9.304)

The user: "do a bunch of audits. i am going to bed. you can do a few hours of audits and fixes all by yourself."

| Audit | Result | Fix |
|---|---|---|
| Assets in the MPQ nothing loads | 44 files, 5.5 MB: waypoint-tool intermediates, the panel-frame kit, the spellbook bezel parts, the sygil input, a stale points strip | Removed; four tools redirected so they cannot re-ship them; MPQ 39.1 -> 33.7 MB (v1.9.302) |
| Delivered stat tokens vs SaveItemPower | every IPL_ token handled | none needed |
| File-local per-game state | the August sweeps hold; one harmless tick-phase counter | none needed |
| Header-declared functions unused outside their unit | none of 666 | none needed |
| Mixed line endings | three vanilla files | normalised (v1.9.302) |
| Compiler warnings in Source/oracool | five size_t->int in paladin_ranged.cpp | cast like every other caller; Oracool sources warning-free (v1.9.303) |
| Docs naming the old build tree | Testing Guide | updated (v1.9.303) |
| Skill reference after Holy Bolt | stale row, 49 frames | row dropped, frames renumbered to 48 (v1.9.302) |
| Product name leftovers | seven "Oracool Edition" strings | Diablo Orcl (v1.9.304); ini key kept |
| Reconfigure recipe | manual, and it bit once (MSVC 14.51) | CMakeUserPresets.json, gitignored (v1.9.304) |
| Disabled tests, TODO/FIXME | none disabled; one explanatory TODO comment | none needed |

Not touched, for the user to decide: the standing `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` failure is a fixture mismatch at tile 1,0 (22 vs 4) in vanilla dungeon generation - it has failed every run this month and predates this work. The `stores.cpp` product list still colours a whole requirement line, not the unmet stat, because the shop draws its own lines outside the tooltip's run mechanism.

Every batch built in `C:\Diablo Orcl\x64-Debug`, passed the suite at 635/636, and is committed.

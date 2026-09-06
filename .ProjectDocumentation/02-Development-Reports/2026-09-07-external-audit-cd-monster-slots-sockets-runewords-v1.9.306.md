# External audit (ChatGPT, 2026-09-06), patches C and D: monster slots, socket counts, runeword hosts (v1.9.306)

**Date:** 2026-09-07. Each finding verified against the code before the change.

## WCR-01 (P2, confirmed)

The warcry debuff table is keyed by monster index and the engine reuses an index the moment its monster is deleted (skeletons, golems, doppelgangers), so a monster raised into a dead one's slot inherited its remaining Battle Cry, and an expiring Conversion cleared BERSERK/GOLEM on whoever stood there. `ClearWarcryStateForMonster` is new and is called from `InitMonster` (every creation path) and `DeleteMonster`. Test through the public `DeleteMonsterList` sweep, with a neighbouring slot as the control. `ActiveMonsters` is exported for tests.

## SAV-01 (P2 hardening, confirmed)

The loader clamped the socket COUNT and validated the six socket IDs but never emptied entries at or past the count, and every reader scans all six: a record with count 1 and six stones granted six effects, completed words from data outside the declared range, and gave up six stones on extraction. `Item::normalizeSockets()` clamps the count and empties the tail; the loader calls it after validation. Test.

## RW-01 (P3 contract, confirmed)

`GetActiveRuneword` checked host, count, fill and order but not the documented plain-normal, untiered host rule. It does now; the BASE tier (Normal/Nightmare/Hell/Torment bases) stays eligible, as documented. Test: the same Tir+El completes Steel on a plain sword and not on a magic, unique or quality-tiered one.

Suite 641/642, the standing dungeon-generation failure only.

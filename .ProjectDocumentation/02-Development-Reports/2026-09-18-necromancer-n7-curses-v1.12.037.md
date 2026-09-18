# The Necromancer, phase N7: Curses (v1.12.037)

**Date:** 2026-09-18 - Debug only - **815 of 815 tests**. N4-N6 still await the user's look in play; the ledger had no
verdicts on this page, so the rows were built as written (seven descriptions tightened to what was built, and
written back to the ledger).

## The system: `oracool/curses`

ONE curse per monster - kind, rank, owner, clock - in a per-slot table. Cast as an area at the cursor (radius 2, +1
a point of Wide Malice to 5), or on the one monster under the cursor for Attract and Death Mark. Duration 8 s + 1 s
a rank, +20% a point of Curse Mastery. A new curse replaces the old, except that **Doom** cannot be displaced by a
weaker one. Every curse is priced at **25 Essence**; Soul Harvest is mana and GIVES Essence.

Nothing is pushed into the monster; the curse is asked at the engine's seams:

| Seam | Curses |
|---|---|
| `ApplyMonsterDamage` - damage taken | Amplify Damage (+50%+5r physical, 100 max), Lower Resist (+25%+3r fire/lightning/magic/acid), Decrepify (+20% all), Doom (+15%+2r all); **Frailty**'s floor after the subtraction (below 10%+r of life, it dies; not uniques) |
| `oracool/warcries DebuffMonster` - damage dealt | Weaken (-33%), Decrepify (-25% and -20% armour, plus a chill for the slow) - the warcries' own debuff, reused |
| `MonsterAttackPlayer` / `MonsterAttackMonster` - a blow landed | Iron Maiden: the striker takes (100+25r)% of it back |
| `player.cpp` / `missiles.cpp` beside `OnRfa12Hit`; `OnMinionBlow`'s site | Life Tap: the striker - hero or minion - heals 20%+2r of the blow (50 max) |
| `rfa12_effects MonsterMayNotice` | Dim Vision: it notices only what stands beside it |
| `ProcessMonsters` dispatch | Terror: a step away from its owner is the monster's whole turn (`MonsterStepAwayFrom`); it stops at 9 tiles and shivers; not uniques |
| `UpdateEnemy` (after the companion taunt) | Attract: neighbours within 8 go for the cursed one |
| `MonsterDeath` | Death Mark: a Corpse Explosion of the curse's rank at the body; Essence Tap: 2+2 points of Essence to the owner |
| `ProcessCursesTick` | the clocks, Bane's acid (2+r a second), Confuse's release |

**Confuse** turns the monster the way the Barbarian's cry does - `MFLAG_BERSERK | MFLAG_GOLEM` for the duration,
taken off with the curse (only if the curse put them on). Not uniques.

**The marker**: a lettered chip over the head (`DrawCurseMarker` from `DrawMonsterHelper`), the curse's first letter
in its hue on a dark plate - until RfA-17's 14 sigils arrive, which slot in at the same call.

State is cleared with the slot (`DeleteMonster`), the level, and the game.

## Format

`MAX_SPELLS` 275 -> 290 (15 actives); the writehero hash moved (tag 18); `SpellBand`, `SpellITbl`, `SpellsData` grew.

## For the user's look in play

Points in Amplify Damage: cast on a pack (Essence -25, letters over heads), hit one and compare the numbers. Terror
on a pack: they walk away and stop at nine tiles. Confuse: they fight each other. Attract on one: the others turn
on it. Dim Vision: walk up to them unnoticed. Iron Maiden: let one hit you. Life Tap: hit one and watch your life.
Death Mark then kill it. Soul Harvest with several cursed nearby (Essence up). A screenshot is the only verification.

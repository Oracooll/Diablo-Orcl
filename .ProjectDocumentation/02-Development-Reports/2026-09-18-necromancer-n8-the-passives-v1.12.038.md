# The Necromancer, phase N8: the passives (v1.12.038)

**Date:** 2026-09-18 - Debug only - **815 of 815 tests**. N4-N7 still await the user's look in play; the ledger had no
verdicts on this page, so the rows were built as written - with the four that had no system behind their sentence
rewritten to what was built (and written back to the ledger).

## Seventeen built, one waiting

His passives join the other heroes' in `oracool/passives.cpp`, at the seams that already exist for D3-style passives -
no new hook sites, only new branches:

| Passive | Where | What |
|---|---|---|
| Life from Death | `OnPassiveMonsterKilled` | a death within 6 heals a 25th of your life (rewritten: there are no health globes in this engine) |
| Fueled by Death | new `OnPassiveCorpseConsumed`, called wherever a corpse is taken (raises, Revive, the two bursts) | +30% move speed for 4 s, through the existing Haste clock |
| Stand Alone | `PassiveDamageTakenPercent` | -15% damage taken with no minions, +3% per minion kept, nothing at five |
| Swift Harvesting | - | **inert**: it needs the wands and scythes (N9); the row says so |
| Commander of the Risen Dead | `PassiveManaCostPercent` | Raise Skeleton / Skeletal Mage -30% mana |
| Extended Servitude | `necro_summoning` Revive | timed minions +25% |
| Rigor Mortis | rfa12 `BoneStrike` | a second's chill on every bone hit |
| Overwhelming Essence | `essence.cpp MaxEssence` | 120 |
| Dark Reaping | `OnPassiveHit` | a point of mana and a point of Essence per landed blow |
| Spreading Malediction | `PassiveDamageDealtPercent` | +5% per cursed monster within 6 (30 max) - `CursedMonstersNear` |
| Eternal Torment | `curses.cpp` at the cast | the curse's clock is a day; it clears with the body |
| Final Service | `PassiveCheatsDeath`, before the shared cooldown | the army is dismissed, you keep a quarter; once a floor (`FloorStamp`: a set level is its own floor) |
| Grisly Tribute | `minions.cpp OnMinionBlow` | a tenth of every minion blow heals the owner |
| Draw Life | `ProcessPassivesTick` | a 200th of your life a second per monster within 4, five at most |
| Serration | rfa12 `BoneStrike` | +5% per tile between you and the target, 50 max |
| Aberrant Animator | `minions.cpp OnMinionStruck` | a fifth of every blow a minion takes goes back (stacks with the Iron Golem's third) |
| Blood is Power | `OnPassiveDamaged` | rewritten: this engine has no skill cooldowns, so losing life feeds Essence - a point per 25th of your life lost |
| Rathma's Shield | `OnPassiveDamaged` sets the clock, `PassiveDamageTakenPercent` returns -100 while it runs | below a fifth of life: 4 s of nothing; once a floor |

The passive sweep test's Necromancer exemption is gone; Swift Harvesting joins its still-inert list.

## For the user's look in play

Slot a few (the passives page, four slots): Stand Alone with and without an army; Draw Life in a crowd; Dark Reaping
and watch both halves of the orb; Rathma's Shield by taking a beating; Final Service with an army. A screenshot
is the only verification.

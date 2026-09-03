# Round 9 — the corpse pass, and the plan closed (v1.9.191)

Round 9 of [[Plan - Developing Every Inert Skill]], the last. Four rows: Find Potion, Find Item,
Grim Ward, Redemption. Inner Sight, Slow Missiles and Taunt from the plan's Round 9 list had
already shipped in Round 6.

## Corpses

The engine already keeps corpses: `dCorpse` on the tile map, set by `AddCorpse` when a monster
dies. Consuming one is clearing that cell — it stops drawing, and nothing can find it again.
`CorpseNear(centre, radius)` finds the nearest; the three cries search within two tiles of the
cursor (they are targeted, through Round 6's aimed missile), the aura searches its own field.

| Row | Effect |
|---|---|
| Find Potion | consume the corpse; 50% (+5/rank, cap 90) a potion, 5% (+2/rank) of those full; heal or mana by coin |
| Find Item | consume the corpse; 25% (+5/rank, cap 60) a random item via `CreateRndItem` |
| Grim Ward | consume the corpse; a totem at its tile, one per player, earshot reach, 20 s (+2/rank); repels all but uniques each tick on Sanctuary's retreat channel |
| Redemption (aura) | once a second, the nearest corpse in the field is consumed for 2% (+1/point) of life and mana |

## Held back, for good

- **Decoy** — a second player-shaped entity that monsters target. Nothing in this fork is that.
- **Ode to Glory** — raises a fallen ally. V1 is single-player; there is no ally to raise.
- **Cleansing** — nothing on a player has a duration to shorten.
- Plus the five inert-by-engine rows: Throwing Mastery, Spear Mastery, Increased Stamina, Poison
  Javelin, Plague Javelin, Double Throw; and the 36 Passive Skills page rows that need fury,
  wrath, cooldowns, songs, traps and the rest.

## The plan, closed

Nine rounds in one day (v1.9.181 → v1.9.191). Of the 114 rows the sweep found inert on
2026-09-03, roughly 100 are live; every one still inert says why in its own sentence, and a test
pins that a row's sentence and its `implemented` flag agree.

| Round | Rows | Mechanism |
|---|---|---|
| 0 | — | the thirteen Cold sheets load |
| 1 | 3 | cold damage type, chill |
| 2 | 10 | the cold page, freeze, cold armours |
| 3 | 10 | the bow latch |
| 4 | 17 | the melee latch |
| 5 | 45 | passives: sheet rows and seven hooks |
| 6 | 21 | cries, songs, held auras |
| 7 | 8 | the javelin page on the melee latch and two spells |
| 8 | 3 | Sacrifice, the tree's own Holy Bolt, timed Conversion |
| 9 | 4 | corpses |

## Numbers

- MAX_SPELLS 123 → 126; the writehero hash is re-baselined with its reason.

## To look at in play

Kill something, then Find Item on the body: it vanishes and sometimes leaves loot. Grim Ward on a
corpse in a doorway: the pack stops coming through. Light Redemption after a fight: the bodies go
one by one and the bulbs fill.

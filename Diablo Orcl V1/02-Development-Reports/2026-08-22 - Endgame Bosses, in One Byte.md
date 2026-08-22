# Endgame Bosses, in One Byte

**Version:** v1.9.13 → v1.9.14
**Date:** 2026-08-22
**Tests:** 499/501 (the two standing baseline failures)

The treasure classes gave every zone something worth farming for. This is what stands in front of
it.

## What a boss is

Not a new monster. `PlaceLesserUniqueMonst` already takes a type the floor loaded, scales it,
escorts it, names it and tints it — a boss is that path with a heavier profile, so it needs no
sprite, no animation and no type slot. The backlog row said "built entirely from machinery that
already exists" and that is held to: the moment a boss needs its own art, it becomes blocked on
somebody drawing it.

| | Champion | Boss |
|---|---|---|
| Health | 300% | **800%** |
| Damage | 150% | **200%** |
| Armour | +4 | **+10** |
| Escort | 4 | **6** |
| Size | normal | **always Colossal** |
| Modifiers | one | **two** |
| Treasure multiplier | 2× | **6×** |

Life outruns damage deliberately — 2.7× the champion's health against 1.3× its damage. A boss should
be a long fight, not a fast death; that is the same judgement `PlaceLesserUniqueMonst` already made
for champions, and it is now asserted rather than merely repeated.

At Hell's table, 6× turns an 11% socketable and 5% set-piece chance into 66% and 30% — on one
monster per floor. A destination, still a roll.

## One byte

`LesserUniqueAffix::Dread` — a seventh value on an enum that already lives in a saved `uint8_t`
([loadsave.cpp:1571](Source/loadsave.cpp:1571), a byte that was Unused before champions took it). No
new field on `Monster`, no format bump, nothing to migrate.

It sits **after** `Colossal` and `LAST` still names `Colossal`, so every existing `<= LAST` walk
keeps meaning "the rollable affixes". `RollLesserUniqueAffix` cannot produce it; the only place it
is ever set is `PlaceLesserUniqueMonst`'s boss branch, which makes `lesserAffix == Dread` a safe
test for one anywhere else.

A boss wants **two** things wrong with it and one byte holds one affix. So the second trait is
derived from `lesserNameSeed` — a value the champion path already rolls and already saves — through
a pure `SecondaryTraitOf(seed)`. Same trick as the monster variants: derived, never stored,
identical every time you walk back onto the floor.

The four traits: **Warding** (resists everything a little, plus armour), **Implacable** (takes your
knockback away), **Devouring** (heals from what it deals), **Adamant** (more life again on top of
the profile). Each is a channel an ordinary monster already has — the constraint that kept the
champion affixes honest, and it kills the obvious idea here for the same reason: there can be no
speed trait, because monster movement is paced by the animation and animation timing lives on the
shared `CMonster` rather than on the individual.

The trait is read from a *different part* of the seed than the name and the tint, or every Devouring
boss would also be the same colour with the same name and three independent-looking properties would
turn out to be one.

## Where they appear

`PlaceEndgameBoss()` runs right after `PlaceLesserUniques()`, before the scatter, for the same
pool-accounting reason the champions do.

- **Guaranteed** from area level 49 — written as `MaxAreaLevel / 2 + 1` so a change to the floor
  count moves it — which is the first Hell floor. Deliberately not rarer: a boss you can plan a run
  around is a destination, and that is the whole point of having just built per-zone drop tables. A
  boss you *might* get is a lottery, and the treasure class behind it is unfarmable through one.
- **25%** on floors past the first third otherwise, so the concept is met before the point it
  matters.
- Never in town, never on set levels — the two exclusions `LesserUniqueCountForLevel` already makes.

It runs **after** the champions on purpose: they take the floor's distinct types first, so the boss
gets one they did not, and a boss repeating a champion's identity would read as the same fight
twice.

## The bug the test caught

The first draft of the test built its fixtures as `Monster boss {}` and every one of them came back
worth a unique's multiplier. **A value-initialised `Monster` has `uniqueType == 0`, which is
`UniqueMonsterType::Garbud`** — `None` is `-1`, not `0` — so a zeroed Monster answers `isUnique()`
with true. The fixtures now set it explicitly, with the reason written above them.

## Tests

`EndgameBossIsAHeavierChampionAndIsMarkedByOneByte` compares the two profiles as a **relation**, not
as fixed numbers, so tuning either stays legal and only inverting them fails — including the
health-outruns-damage rule. It pins the multiplier ladder (boss > unique > champion > ordinary), that
a boss wearing a unique's shape still gets *six* (the order inside `TreasureBonusFor` is the whole of
that, and moving one test down is a two-line change that quietly pays a boss a champion's double),
that a boss is Colossal and a champion is not, and that 200 draws across every named champion type
never produce `Dread` — if it ever became rollable, ordinary champions would spawn at 800% health
with a 6× multiplier and nothing would report it.

`EveryBossTraitIsReachableAndNamed` sweeps **all 65,536 seed values** — every value the field can
hold, not a sample — to prove each trait is reachable and roughly evenly distributed. A trait no seed
can produce is dead code wearing a name, and nothing else in the game would notice. It also finds the
Devouring seed by searching rather than assuming, so the test cannot disagree with the derivation
about which seed is which trait, and checks the drain fires for Devouring, not for other bosses, and
not for a Vampiric champion through the boss hook.

## What to look at in game

- Take a Hell floor. There should be exactly one Colossal monster with two words in front of its
  name, and it should be a serious fight.
- Kill it and check the drop against the floor's treasure class — Caves runes, Hell jewels.
- Walk a Normal floor around depth 16+ a few times; roughly one in four should have one.

---
date: 2026-08-15
version: 1.6.5
area: Lesser uniques / self-audit
---

# Self-Audit — Four Findings

An unprompted pass over the recent work, concentrated on the lesser uniques and the Paladin skills —
the newest code, and the code that has been wrong most often. Four findings, two of them real bugs,
one of them introduced an hour earlier by the previous fix.

## 1. The tint was repainting every scripted unique — REGRESSION

**Severity: high, and self-inflicted.**

v1.6.4 added a `TintLesserUnique` call to `SyncMonsterAnim`, so a champion's recolour would survive a
save/load. `SyncMonsterAnim` runs for **every unique on the level**, on every load and every level
entry — and `TintLesserUnique` never checked whether the monster it was handed was a lesser unique at
all.

A scripted unique has `lesserNameSeed == 0`, which decodes to a shift of **−2**. So Gharbad, Zhar,
Lazarus, the Butcher and the rest were all being quietly repainted two shades darker on every load.

The guard was unnecessary while the only caller was `PlaceLesserUniqueMonst`, which by construction
only passes lesser uniques. Adding a second caller with a wider audience invalidated an invariant that
was never written down. It is written down now, and enforced in the function rather than at the call
site.

## 2. Four quest bosses were spawnable as lesser uniques

**Severity: high.**

`IsQuestUnique` tested `mtalkmsg != TEXT_NONE` — the game's own marker for a unique that speaks, and a
good test as far as it goes. It does not go far enough: **four quest bosses do not talk.**

| Boss | `mtalkmsg` | What made it dangerous |
|---|---|---|
| Skeleton King | `TEXT_NONE` | quest boss, duplicable |
| The Butcher | `TEXT_NONE` | position read off `SetPiece` — a borrowed one spawns in the real Butcher's room |
| Hork Demon | `TEXT_NONE` | quest boss, duplicable |
| Na-Krul | `TEXT_NONE` | **`GetUniqueMonstPosition` writes `UberDiabloMonsterIndex`** — a borrowed one repoints, or with `UberRow` unset erases, the state the Na-Krul quest runs on |

`LevelHasMonsterType` is not protection: these types *are* loaded on the floor their quest owns, and
`PlaceLesserUniques` runs **after** `PlaceQuestMonsters` — so the boss's own level is exactly where
they were reachable. The repeat-fallback added at 1.6.1 widened this: once `fresh` empties, the pool
that gets drawn from is the one holding the already-placed boss.

The fix adds a second data-driven test: the monster type's `MonsterAvailability::Never`, the game's
marker for "exists only for scripted content". It catches all four, costs no ordinary champion
(anything appearing on a random level must be `Always` or `Retail`), and catches any quest boss added
later — the same reason `mtalkmsg` was chosen over a hand-written name list in the first place.

## 3. The kill log said you had killed someone else

You fought **Warded Malgrith the Unclean**, killed it, and the log read *"Defeated Rotfeast the
Hungry"* — the borrowed champion's name, which is the precise confusion the 1.6.2 rename existed to
remove.

Two places were naming monsters and only one had been updated. There is one now:
`oracool::GetMonsterDisplayName`, which both the health bar and the kill log call. The health bar's
local copy of the composition is gone.

## 4. A cosmetic affix was shifting the loot RNG

`MonsterDeath` sets `SetRndSeed(monster.rndItemSeed)` so a monster's drop is reproducible. The
Thunderous discharge was firing **before** `SpawnLoot`, and it spends 36 `AddMissile` calls, each
drawing a random animation frame — so it advanced the item stream out from under the drop.

Still deterministic, so not a bug in the strict sense. But it made a champion's loot depend on which
modifier it happened to be wearing, which is a coupling with nothing to recommend it. Moved after
`SpawnLoot`. Both happen in the same tick, so the ordering the old comment defended — the discharge
reading as part of the kill — is not something a player can perceive either way.

## Checked and found sound

- **`cursPosition` bounds.** The shift-forced cast path added at 1.6.3 passes `cursPosition` to
  missiles that index `dPiece`/`dMissile`. `CheckCursMove` clamps it to `[0, MAXDUNX-1]`, so there is
  no out-of-bounds path.
- **Thunderous's trap-sourced missile.** Fired with source `-1`; `Missile::IsTrap()` short-circuits
  the unique-palette lookup, and `AddNovaBall` handles `_misource < 0` explicitly.
- **`ProcessBlessedHammer`'s spiral.** Bounds-guarded before every `dPiece` read, and its
  `pixelsX << 16` is well-defined — the project builds as C++20, where signed left shift is specified.
- **All monster indexing in `oracool/`.** Every `Monsters[...]` is either an `ActiveMonsters` walk or
  guarded by a `pcursmonst == -1` test.
- **`AiDelay`, behind Shield Bash's stun.** Overrides `HitRecovery` unconditionally, so the stun
  lands; Lazarus is exempt, which is correct and inherited rather than restated.

## Not changed, deliberately

`CalcRemainingMonsterXp` walks every live monster each frame, and `GetMonsterDisplayName` builds a
`std::string` each frame while hovering a champion. At ~200 monsters and 60fps these are a few
thousand operations and one small allocation a second. Optimising them would be premature and would
cost clarity for no measurable gain.

## State

**354/356** — `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, red
before any of this work began.

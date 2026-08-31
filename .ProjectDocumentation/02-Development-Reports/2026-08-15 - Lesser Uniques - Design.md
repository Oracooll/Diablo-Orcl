---
date: 2026-08-15
version: 1.5.80
area: Monsters / random uniques
status: design, not yet built
---

# Lesser Uniques — Design

> lets introduce underpowered versions of unique bosses along with minions randomly on every level.
> they can be shrunken in size with stats appropriate for uniques and minnions compare to local mobs
> on current dungeon level. we need to introduce diversification and freshness in the dungeons. you
> have diablo 2 3 4 to draw inspiration from. go wild. make diablo 1 world feel new.

## The finding that makes this cheap

`UniqueMonstersData` holds **101 entries**. Every one has a name, a palette file, an AI, hit points,
damage, resistances, and a minion-pack type — fully authored, hand-tuned content.

Almost none of it is ever seen. Each is pinned to a single level by `mlevel == currlevel`, most are
quest-gated on top of that, and a normal playthrough meets perhaps a dozen. **Diablo 1 already
contains a champion system; it just spawns each champion exactly once, on one floor, forever.**

So this is not "build random uniques". It is "stop wasting ninety of them".

## What already exists

| Piece | State |
|---|---|
| `Monster::uniqueMonsterTRN` | A per-monster palette. A recoloured champion is already renderable. |
| `PlaceUniqueMonst(type, minionType, packSize)` | Already places a unique **with** a minion escort — 8 for most, 30 for the Skeleton King. |
| `UniqueMonsterPack::Leashed` / `Independent` | Already distinguishes minions that stay with the boss from ones that wander. |
| Unique stat overrides | HP, AC, to-hit, damage and resistances are already applied per unique. |
| `Oracool.monsterDensityPercent` | Shipped at 1.5.80. The unique-rate dial slots in beside it. |

The user's "along with their minions" is, mechanically, already done.

## What does not exist

**Sprite scaling.** There is no scale parameter anywhere in `clx_render.hpp` — CLX blits are 1:1 by
construction, and monster sprites are pre-rendered per direction and per animation. "Shrunken in
size" would need a new scaled blit path touching the hottest loop in the renderer.

That is a poor trade for the goal, because the goal is **legibility** — the player must be able to
tell a lesser unique from the real thing at a glance — and there are cheaper ways to say it:

- The **palette** already differs per unique (`mTrnName`), and that is the signal Diablo 1 itself uses.
- A **name** in the hover text: "Rotfeast the Hungry, Lesser" or a D3-style suffix.
- A **health bar** treatment, which the fork already draws for monsters.

Recommendation: drop the shrinking, spend the effort on colour and naming. Revisit only if colour
proves not to read.

## The design

### Selection — reuse, do not invent

For each level, roll `N` lesser uniques. A candidate is any `UniqueMonstersData` entry whose `mtype`
is **already in `LevelMonsterTypes`** for this level.

That constraint is doing a lot of work: it means the sprite and its animations are already loaded, so
a lesser unique costs no extra memory and can never appear as a monster that does not belong in that
dungeon. A Cathedral floor draws its champions from the skeletons and zombies actually walking it.

Quest uniques are excluded by type, not by level, so Garbud stays Garbud's — a lesser unique is never
a quest boss with the serial numbers filed off.

### Scaling — the level's own mobs are the yardstick

The user's phrase is exact: "stats appropriate for uniques and minions **compare to local mobs on
current dungeon level**". So the reference is not the unique's designed numbers, it is the floor.

Take the level's ordinary monster stats as the base and apply a champion multiplier:

| Stat | Lesser unique | Minion |
|---|---|---|
| Hit points | ×3 of a local mob | ×1.5 |
| Damage | ×1.5 | ×1.2 |
| Armour / to-hit | +2 levels' worth | +1 |
| Experience | ×4 | ×1.5 |

Deliberately **not** the authored `mmaxhp` from the table — a level-16 unique's 2,000 HP on a level-3
floor is the "underpowered" complaint the user is pre-empting. The table supplies *identity* (name,
palette, AI, resistances); the floor supplies *power*.

### Affixes — where D2/D3/D4 earn their place

One affix per lesser unique, drawn from a small set that the engine can already express:

| Affix | Implementation already present |
|---|---|
| **Fleet** | `_mmode` speed / `MFLAG_*` — the game has fast variants |
| **Vampiric** | heal-on-hit; the Blood Star monsters already drain |
| **Warded** | `mMagicRes` bitflags, already per-unique |
| **Thunderous** | spawn a small lightning on death — `MiniNovaBall` exists as of 1.5.78 |
| **Relentless** | ignores knockback and stun; `MFLAG_KNOCKBACK` is already a flag |

Each affix should tint the palette or add a word to the name — a player must be able to learn what
they are looking at, which is the whole point of the system.

### Rate — where the user's modifier finally attaches

`Oracool.uniqueMonsterDensityPercent`, same 100–300 steps as the density dial, defaulting to 100.
Base rate: **one lesser unique per level at 100%**, rising to three at 300%. Their minions come out
of the scattered budget rather than on top of it, so a dense level does not become a slideshow.

### Loot

D2 and D3 both make champions the reason to fight them. A lesser unique should roll its drop one
item-level higher than the floor and take one extra roll at the Rare/Buffed-Unique/Primal ladder the
fork already has in `Oracool` options. No new item code — just a better ticket in the existing lottery.

## Build order

1. Candidate selection + placement of one lesser unique per level, no affixes, authored palette.
   Proves the sprite/level constraint and the minion reuse.
2. Stat scaling against local mobs, with the multiplier table above in one place.
3. The rate option, wired to the same INI section as monster density.
4. Affixes, one at a time, cheapest first (Warded, Relentless, Fleet, then the two that need code).
5. Loot.
6. Naming and hover text.

## Risks worth stating now

- **`MaxMonsters`.** Minion packs plus a 3× density dial plus lesser uniques all draw on the same
  pool. The clamp in `InitMonsters` is the backstop and must stay the last word, as it is today.
- **Determinism.** Level generation is seeded, and `PackTest`'s fixtures encode seeded outcomes.
  Anything drawing from `GenerateRnd` during level init shifts those. Roll lesser uniques from a
  dedicated stream or after all seeded generation, and expect the timedemo to need re-baselining.
- **Save format.** Monster state is per-level and written by `loadsave.cpp`'s monster block, not by
  `PlayerPack` — so an affix field is probably free, but it must be checked against that block before
  it is added, not after.

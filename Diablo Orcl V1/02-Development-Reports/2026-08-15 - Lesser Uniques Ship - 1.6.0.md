---
date: 2026-08-15
version: 1.6.0
area: Monsters / lesser uniques
---

# Lesser Uniques Ship — 1.6.0

Six steps, six commits, one version bump. Diablo 1's dungeons now host champions.

## What it does

Every generated level rolls one or more **lesser uniques**: a named champion from the game's own
roster, escorted by minions, carrying a modifier, scaled to the floor it stands on rather than to the
level it was written for.

## The finding the whole thing rests on

`UniqueMonstersData` holds **101 hand-authored champions** — name, palette, AI, hit points, damage,
resistances, minion pack. A normal playthrough meets perhaps a dozen, because each is pinned to a
single level and most are quest-gated on top.

**Diablo 1 already had a champion system. It spawned each champion once, on one floor, forever.**
This is not new content; it is ninety unused champions put to work.

## The six steps

| Step | Version | What it added |
|---|---|---|
| 1 | 1.5.81 | Selection and placement |
| 2 | 1.5.82 | Scaling against the floor |
| 3 | 1.5.83 | The INI density dial |
| 4 | 1.5.84 | Five champion modifiers |
| 5–6 | **1.6.0** | Loot, naming, ship |

## Decisions that carried weight

**Candidates are restricted to champions whose monster type is already on the level.** That makes
them free — the sprite is loaded either way — and coherent: a Cathedral floor draws its champions
from the skeletons actually walking it, never from something that does not belong.

**Quest content is excluded by reading `mtalkmsg`, not by a hand-written list.** The uniques that
speak are the ones a quest is about. A unique added later classifies correctly with nobody
remembering to update anything.

**Scaling captures rather than recomputes.** `PlaceMonster` runs `InitMonster`, which sets the
ordinary difficulty-scaled stats for this floor; `PrepareUniqueMonst` then overwrites them. So the
ordinary values are captured in between and a multiple is put back. The Nightmare/Hell/Torment ladder
is inherited for free instead of this keeping a second copy of that arithmetic to drift out of step.

**Placement runs before the scatter.** Champions and their escorts draw on the same `MaxMonsters`
pool, and the scatter's clamp is what enforces it — so they must already be counted when that clamp
runs, or a dense level overruns the pool and loses monsters at random.

**No save-format change**, and this was checked before the field was added rather than after.
`loadsave.cpp`'s monster block has always written four bytes as `Unused`, symmetric on both sides;
the affix takes the first. A pre-1.6.0 save reads zero there, which is `None` — exactly right.

## Two things that did not survive contact with the code

**Fleet.** It was in the design and in the first draft. Monster movement is paced by the
**animation**, and animation timing lives on the shared `CMonster` rather than on the individual — so
making one champion fast would have made *every monster of its type* fast with it. Replaced by
**Fortified** (armour), which is per-monster and reads just as clearly in a fight.

**Shrinking.** The user asked for champions "shrunken in size". There is no scale parameter anywhere
in `clx_render.hpp`; CLX blits are 1:1 by construction and monster sprites are pre-rendered per
direction and animation. The goal behind the request was legibility, and that is answered by the
**palette** (already per-unique) plus the **modifier word in front of the name** on the health bar —
which is the only place a monster's name reaches the player at all.

## The affixes

| Affix | Effect | Why it was cheap |
|---|---|---|
| Warded | Resistant to magic, fire, lightning | Existing per-monster bitfield |
| Relentless | Cannot be knocked back | `MFLAG_KNOCKBACK` already honoured by the combat code |
| Fortified | +20 armour | Per-monster field |
| Vampiric | Heals a third of damage dealt | Applied after reflect, so it drains what it landed |
| Thunderous | Discharges a mini-Nova on death | The ring built for Fist of the Heavens at 1.5.78 |

Warded grants resistance and never immunity, deliberately: an immune champion on a floor where the
player has one damage type is not a harder fight, it is an unwinnable one.

## Loot

A champion rolls the drop table **twice** rather than once at a better rarity. No new item code — a
second ticket in the lottery the fork already runs (Unique → Primal → Buffed Unique → Rare), which is
a materially better drop without inventing a tier nobody has balanced.

## Left undone, deliberately

**Experience.** `Monster::exp` reads per-*type* data rather than per-monster state, so scaling one
champion's reward needs a field rather than a multiplier. It was not worth a second save-format
conversation in the same feature.

## State

354/356 — `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, both failing
before any of this work began. Being precise about what that does and does not prove: the timedemo
was already red, so it cannot confirm whether consuming RNG during level init shifted the scatter for
a given seed. It certainly did — that is inherent to adding a roll there — and the design recorded
that expectation up front.

Not tested in-game — the user runs the game. Worth watching first: whether one pack per level is the
right density at 100%, whether the affix word fits the health bar for the longer champion names, and
whether a Thunderous discharge on a crowded floor is a surprise or an ambush.

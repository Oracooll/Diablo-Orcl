# Companions: the Golem rebuilt as an engine feature

2026-09-14 — v1.12.018

## Why

> "i want you to keep this version of golem we are building as a separate engine feature called Companion."
>
> "i like all suggestions. lets also make her have health instead of invulnerable, so level ups increase dmg and hp
> and resistance and duration ... 30 sec on level 1 and up 5 sec every level ... up to 1000hp at lvl 20 and more
> onwards. she should reach res 90% rather soon as well. lets built this landmark feature"

## The module

`oracool/companion.{h,cpp}` replaces `oracool/decoy.{h,cpp}`. Everything a summon is lives in one table of
**definitions**, and every summon in play is an **instance**.

- **A definition** holds:
  - **Look:** hero sheets (class, armour, weapon, or the caster's own gear), a palette-ramp tint, and whether the
    shadow stays black.
  - **Behaviour:** attack (bow or melee), role (fighter, guard or bait), and an ability with its cooldown.
  - **Growth lines** from the skill level: life, elemental and physical resistance (each capped), damage as a percent
    of the owner's blow, and duration.
- **An instance** holds its owner, rank, time left, life carried across levels, its golem slot, its formation place,
  its cooldowns and its current target.

### The four companion skills

| Skill | Companions | Attack | Role | Ability | Life (lv 1 → 20) | Elem. res. | Phys. res. | Damage | Duration |
|---|---|---|---|---|---|---|---|---|---|
| Valkyrie | Valkyrie (Rogue, heavy, bow, dark gold) | bow | fighter | Volley (3 arrows, 5 from lv 10), 8 s | 150 → 1000 | 40 +5, **90 at lv 11** | 20 +2, cap 50 | 50% +5 | **30 s +5** |
| Ancestral Call | Korlic, Talic and Madawc (warrior sheets, grey ghosts) | melee | fighters | Leap 10 s, Whirlwind 8 s, Hammer Toss 6 s | 120 → 800 each | 30 +5, cap 80 | 25 +2, cap 55 | 35% +3 each | 20 s +2 |
| Spirit Guardian | Spirit Guardian (Monk, staff, blue) | melee | guard | Taunt, 8 s | 200 → 1200 | 40 +5, cap 85 | 30 +2, cap 60 | 30% +3 | 30 s +5 |
| Decoy | a blue ghost of the caster | none | bait | none | 100 → 900 | 30 +4, cap 75 | 20 +2, cap 50 | 0 | 15 s +1 |

- **The Valkyrie's numbers** are the user's.
- **Life** keeps rising past level 20 on the same line.
- **Level 10 and 20 rewards (rank milestones in place of runes):** at level 10 abilities hit 25% harder and the
  Valkyrie's volley grows to five arrows; at level 20 abilities come round 25% sooner.
- **Everlasting (v1.12.015) is gone.** Every companion now has a timed life, as asked.

## How it lives in the engine

**Slots.**
- A single-player game has one player, so golem slots 1–3 belong to nobody. Companions take those; slot 0 stays the
  Golem spell's.
- At most three companions exist at once. A cast with no room lets go of the companion with the least time left,
  never one the same cast is calling.
- In a multiplayer game a companion may only borrow its owner's own slot.

**Bodies.**
- `SpawnCompanionBody` stands a body up with no network message, and `ReleaseCompanionBody` sends one back to the
  holding cell with no death.
- `MoveCompanionTo` and `PlaceCompanionNear` move them (`monster.cpp`).
- The slot is dressed before the body stands, so its first frame is already the hero sheet's.

**The brain (`CompanionAi`, `monster.cpp`).** It asks `GetCompanionOrders` and `PickCompanionTarget`:
1. **Regroup.** Past the regroup distance, the companion reappears beside its owner in a pillar of light.
2. **Leash.** Past the leash, it walks back to its **formation place**: behind and right, behind and left, or straight
   behind the owner's facing.
3. **Fight.** It picks a target: **focus fire** on what the owner struck in the last 3 seconds (`NoteOwnerStruck`, from
   `M_StartHit`), otherwise the enemy nearest the owner. It uses a ready ability, else its bow or its blade.
4. **Settle.** With nothing to fight, it drifts back to its place.
5. **Hurry.** Left behind mid-step, it advances a frame more each tick, so it moves at twice the pace.

**Stances**, cycled with **J** (keymapper "Companion stance") or a click on the panel's stance line:

| Stance | Leash | Regroup | Melee reach |
|---|---|---|---|
| Follow | 3 | 10 | 4 |
| Hold position | none | 14 | adjacent only |
| Aggressive | 6 | 12 | 8 |
| Passive | 2 | 8 | never attacks |

A Decoy ignores stance: it stays where it was put.

**Damage dealt is the owner's.**
- **Arrows.** `CompanionShot` looses the owner's player-sourced arrows, and copies fire or lightning arrows from the
  owner's gear. The new `Missile::companionPercent` is read in `CheckMissileCol` and applied in `MonsterMHit` to the
  whole blow, with bonuses and passives included.
- **Melee and abilities.** They roll the owner's weapon damage with its bonus damage, at the companion's percent, and
  land through `M_StartKill` and `M_StartHit` as the owner's.
- **Credit.** Kills, experience and on-kill effects are the hero's. Companions are never struck by their own side's
  missiles.

**Damage taken.**
- `ApplyMonsterDamage` asks `CompanionDamageTaken`: physical resistance against physical blows, elemental resistance
  against the rest.
- It does not flinch (`StartMonsterGotHit`).
- At zero life it plays its hero death and is gone for good.

**Guards and decoys.** At the end of `UpdateEnemy`, a monster within reach of one attacks it:
- a Decoy draws everything within 6 tiles;
- a Spirit Guardian holds what is within 3 tiles, and its Taunt widens that to 8 tiles for 4 seconds.

**Walking through.**
- `PosOkPlayer` lets the owner walk onto its own standing companion.
- `HandleWalkMode` then moves the companion to the tile the owner left.

**Levels.**
- `OnCompanionLevelLoad` (from `InitLevelMonsters`) lets bodies and town figures go.
- `ProcessCompanions` (every tick, from the RfA-12 tick) brings each companion back on the new level with the life
  and time it had.
- A revisited level restores stale bodies in slots 1–3; they are released before they can walk about as plain Golems.

**Town.** Companions are drawn beside the players and follow, never fighting, and their time still runs. This is the
v1.12.013/014 town walk, one per instance. A Decoy stays put.

**The panel.** Under the clock's speed band, top left (where Diablo III keeps its companion portraits):
- a header, "Companions: <stance>";
- a row per companion: name, seconds left, a red life bar and a gold time bar.

**Arrival, departure and regroup:** the Resurrect beam and its sound.

## Also changed

- **Old summons replaced.** Ancestral Call and Spirit Guardian were 30-second Golem spirits (`Summon`), and Decoy was
  a disarmed Golem. All three are companions now. `Summon`, `CanSummonHere`, `CallValkyrie`, the spirit clock and the
  everlasting re-summon are gone.
- **Descriptions.** The four skills' in-game descriptions now state their companions' numbers.
- **Safety.** `MonsterAttackMonster` no longer indexes `Players` by a slot beyond the player count. In single player
  that is only ever one player, and slots 1–3 would have read past it.

## Tests

- `OracoolCompanion.TheValkyrieGrowsTheWayTheUserAsked`:
  - 30 s at level 1 and 35 s at level 2;
  - 150 life at level 1, 1000 at level 20, and more past it;
  - resistance under 90 at level 10, 90 at 11, and never past 90;
  - 50% damage; a Decoy deals none; bow and melee attacks.
- `OracoolCompanion.FourSkillsCallCompanionsAndTheStanceCycles`:
  - four companion spells, and the Golem spell is not one;
  - nothing is a companion after a new game, and only a companion resists damage;
  - the four stances cycle round.

## Not verified here

This build was not run in the game. It is the largest behaviour change in the session. Worth a look:
- **Movement.** The Ancients and the Guardian close in on melee targets and come back; check the formation, the
  swap when walking through a companion, and the hurry.
- **Attack timing.** Hits land at the hero sheets' action frames, which the Golem's data never had.
- **The panel.** Its place under the clock is computed, not seen. A screenshot decides it.
- **Balance.** All the numbers besides the Valkyrie's are first guesses.
- **Quest levels.** A set level still has no slot, so no companion appears there.

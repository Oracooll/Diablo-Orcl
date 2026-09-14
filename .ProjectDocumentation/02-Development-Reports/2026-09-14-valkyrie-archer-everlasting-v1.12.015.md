# The Valkyrie: an everlasting, invulnerable archer who hits as hard as the hero

2026-09-14 — v1.12.015

## Why

> "make her shot bow and make her invulnerable and everlasting once cast. the dmg she does equals the dmg my hero is
> making."

## She shoots

- **Sheets.** She wears the Rogue's heavy-armour **bow** sheets now, in the dungeon and in town, still in the dark
  gold with the black shadow.
- **AI.** `GolumAi` asks `IsValkyrie`. Out of reach she closes in like the Golem. Within **8 walking tiles** with a
  clear line (`LineClearMissile`), she stands and starts a ranged attack. Like the melee attack, a ranged attack in
  progress is not interrupted by the AI.
- **The shot.** `MonsterRangedAttack` hands her to `ValkyrieShoot`, which looses an arrow at the Rogue bow sheet's
  own release frame (`bowActionFrame`) with the bow sound.

## Her damage is the hero's

- **Whose arrow.** The arrow is **her owner's**: `AddMissile(... MissileID::Arrow, TARGET_MONSTERS, playerId ...)`
  from her tile. The Golem slot's id is the player's id.
- **Damage and to-hit.** `ProcessArrow` gives a player-sourced arrow the hero's `_pIMinDam`-`_pIMaxDam`.
  `MonsterMHit` adds the hero's bonus damage, damage modifier and passives, and rolls the hero's ranged to-hit with
  the usual distance penalty.
- **Kills.** Kills, experience and on-kill effects are the hero's.

It is a plain physical arrow. Fire and lightning arrow items and the Rogue's arrow skills are not copied onto her
shots.

## Invulnerable

- **No damage.** `ApplyMonsterDamage`, where every blow on a monster lands, returns at once for her: no damage, no
  floating number, no death.
- **No stagger.** `StartMonsterGotHit` returns for her too. For a Golem it plays no animation, but it snapped a
  walking monster back to the tile it left.

## Everlasting once cast

- **Remembered.** A cast sets `SetValkyrieCalled`, a per-player flag.
- **In a dungeon.** `ProcessRfa12ActivesTick` calls her back whenever her slot has stood empty for a second: a new
  level, or a Decoy or spirit that borrowed the slot and has ended. It is silent where there is no slot (a quest's set
  level). It forgets her if the skill has no points left.
- **In town.** `ProcessTownValkyries` brings the companion back on every return, checking once a second.
- **Another summon.** It still takes the slot while it lasts, and she returns after.
- **A new game.** `ClearRfa12ActiveBuffs` forgets her; it runs only at a new game's `InitPlayer`, and the flag is a
  static that would otherwise carry to the next character.
- `CallValkyrie` (rfa12_actives.cpp) is the one summon path for the cast and both returns.

## Tests

- `OracoolCensusNotes.TheValkyrieIsRememberedUntilANewGame`:
  - nothing is a Valkyrie until one is dressed;
  - a call is remembered per player, and out-of-range ids never answer;
  - a new game forgets it;
  - the release frame is valid.

## Not verified here

This build was not run in the game. Worth a look:
- whether the arrow leaves on the right frame of her draw;
- whether 8 tiles is the right reach;
- whether enemies still crowd her. Monsters attack her uselessly, which also keeps them off the hero.

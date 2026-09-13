# Infravision, Etherealize and Search survive a level change

2026-09-13 — v1.11.129

## Why

> "also check the code behind infravision and find out why it runs out every time i change dungeon level and fix
> its countdown timer to survive level changes."

## Cause

Infravision keeps its clock on the **missile** that carries it: `_mirange` counts down each tick, and
`ProcessInfravision` re-asserts `_pInfraFlag` until it reaches zero. The spell countdown column beside the minimap
reads the same field.

`InitMissiles` runs on every level entry, dungeon and town alike, and ends with `Missiles.clear()`. That deleted
the Infravision missile, so the effect and its timer ended on every stair, portal or waypoint. Two other spells
keep their clock the same way and had the same fault:

- **Etherealize.** The comment in `ProcessEtherealize` claimed "the missile survives that". It did not.
- **Search.** Its item view (`AutoMapShowItems`) was cleared with it.

## Fix

`InitMissiles(bool keepHeroTimedSpells)`:

- Before the clear, the local hero's own Infravision, Etherealize and Search missiles are **copied**, if not
  deleted and with time left. After the clear they are **put back** with their remaining time.
- Their effects are restored immediately (`_pInfraFlag`, the Etherealize flag, `AutoMapShowItems`), so the first
  frame of the new level is already drawn with them on.
- **Single-player only.**
- All three are invisible effects on the caster, with no tile, light or `dFlags` mark, so nothing about the old
  level travels with them.

`LoadGameLevel` passes `!firstflag && lvldir != ENTRY_LOAD` at all four call sites. Only a level change inside a
running game carries them; a new game or a loaded save starts clean. That matters because statics outlive the
game: a new character must not inherit the last one's infravision.

## Tests

`OracoolAudit.HeroTimedSpellsSurviveALevelChange` checks four things:

- Infravision (900 ticks) and Search carry across with their time.
- An ordinary missile (Firebolt) does not.
- The infravision flag and the item view come back on.
- `InitMissiles(false)` clears everything.

## For the user to look at

Cast Infravision, note the seconds in the countdown column, and take the stairs. It should still be counting down
on the new level with infravision on. The same applies to Etherealize and Search.

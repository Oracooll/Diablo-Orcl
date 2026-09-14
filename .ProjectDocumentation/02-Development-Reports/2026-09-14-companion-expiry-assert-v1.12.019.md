# A companion's time running out stopped the game

2026-09-14 — v1.12.019

## Why

> "an error occured when timer ran out on the valkyrie"

The screenshot showed an assertion failure at `monster.cpp:4663`: `monster.enemy >= 0 && monster.enemy < MAX_PLRS`.

## Cause

- **The assert.** `ProcessMonsters` reads a monster's `enemy` two ways. With `MFLAG_TARGETS_MONSTER` set it is a
  monster id; without the flag it is a player index, and that is what the assert checks.
- **The mismatch.** `ReleaseCompanionBody` (v1.12.018) cleared `MFLAG_TARGETS_MONSTER` when a companion left, but left
  `enemy` holding the monster it had been shooting (any id up to 199). On the next tick `ProcessMonsters` reached the
  released slot and asserted.
- **Every exit.** Expiry, a cast letting the oldest companion go, and the stale bodies a revisited level restores all
  release through that function, so all of them could stop the game.

## Fix

When `ReleaseCompanionBody` clears the flag, it now also sets `enemy` to player 0, sets `MFLAG_NO_ENEMY`, and clears
`enemyPosition`. The flag and the index always agree.

## Tests

- `OracoolCompanion.ALetGoBodyTargetsNoMonster`: a slot targeting monster 57, once released, targets no monster, and
  its enemy is a player index.

## Not verified here

This build was not run in the game.

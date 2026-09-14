# The Valkyrie stands still

2026-09-14 — v1.12.017

## Why

> "when she is standing in one place she is still looping walking animation"

## Cause

The Golem has no stand sheet, so vanilla stands it on its **walk**. `M_StartStand` and `MonsterIdle` both pick
`MonsterGraphic::Walk` for `MT_GOLEM`. The Valkyrie and the Decoy are the Golem's slot dressed in the Rogue's sheets,
which have a real stand. They inherited the walk-in-place anyway.

## Fix

Both sites now use the walk-for-stand only for an undressed Golem: `MT_GOLEM && !oracool::IsDecoy(monster)`.
`IsDecoy` is true for any slot wearing hero sheets, so the Decoy is fixed too. A plain Golem is unchanged.

## Not verified here

This build was not run in the game.

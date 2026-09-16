# Rage carries down the stairs

2026-09-16 — v1.12.020

## Why

> "barb should carry his rage over dungeon levels"

## What it was

`InitPlayer` emptied the Rage pool on **every** call, and `LoadGameLevel` calls it on every level
entry. So a Barbarian who took a staircase mid-fight - or stepped into town to sell - arrived with
nothing, however hard he had been swinging a second earlier.

## What it is now

The reset moved inside `InitPlayer`'s `firstTime` branch, which runs once, when a character is
created. Rage now survives:

- a staircase, up or down;
- a town portal and the trip back;
- a waypoint.

The only thing that drains the pool is still the calm clock in `oracool/rage.h`: 5 seconds after the
last blow at a monster, then 1 Rage a second. Nothing about generation, cost or the cap changed.

Loading screens are not game ticks, so the clock does not run while a level builds - a fast trip
costs nothing, and a long look at the shops drains at the ordinary rate once play resumes.

## Not verified here

This build was not run in the game. Worth a look: take a staircase mid-fight and watch the orb keep
its Rage, and check that a brand-new Barbarian still starts at zero.

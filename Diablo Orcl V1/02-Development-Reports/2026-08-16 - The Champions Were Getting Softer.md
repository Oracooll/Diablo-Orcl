---
date: 2026-08-16
version: 1.7.23
area: Megaplan Phase 3.3 - resistances and immunities that answer to the difficulty
---

# The Champions Were Getting Softer

Phase 3.3 of the megaplan reads: *"Per-difficulty immunities/resistances (data exists, make it
difficulty-aware)."* Looking at what the data actually says turned up a bug rather than a feature
request.

## One column where there are two

`MonsterData` carries **two** resistance sets — `resistance` and `resistanceHell` — and ordinary
monsters switch to the hard one when Hell arrives. `UniqueMonsterData` carries **one**, `mMagicRes`,
and `PrepareUniqueMonst` assigned it flat on every difficulty.

So on Hell, a rank-and-file Skeleton could be fire-immune while the champion leading it was merely
fire-resistant — or nothing at all. **Champions got relatively softer as the difficulty rose**,
which is exactly backwards, and it lands hardest on this fork: lesser uniques put up to six
champions on every floor, each borrowing a unique's identity and therefore its single resistance
column.

The fix is a union, not a new column. A champion's set is now its own hand-authored bits **plus**
whatever an ordinary monster of its type would carry on this difficulty. Every champion keeps the
identity it was written with, and gains a guarantee it never had: it is never less resistant than
the monsters it leads. Authoring a second `mMagicResHell` column across 101 uniques would have been
101 guesses; this is derived from data that already exists, which is what the phase asked for.

## Nightmare was Normal with fatter monsters

The second half. Vanilla steps hit points, damage and armour on Nightmare, but leaves resistances
alone — the whole second column only arrives at Hell. So Nightmare's monsters were tougher without
being *different*.

Nightmare now gets a real middle rung: **Hell's set with its immunities demoted to plain
resistances**. Nothing becomes unkillable a difficulty early, but the elements Hell will eventually
wall off start pushing back, which is what a middle difficulty is for. `IMMUNE_ACID` has no
`RESIST_ACID` to demote to, so an acid immunity simply waits for Hell.

One subtlety worth stating, because it is the kind of thing that looks like a simplification and is
not: the ladder **unions** rather than replaces. `resistanceHell` is authored as a replacement set,
not a superset, so a straight swap could take away something the Normal column had granted. A
monster must never lose a resistance by the difficulty going up.

## A simplification came free

The Hell and Torment blocks in `InitMonster` each ended with their own
`monster.resistance = monster.data().resistanceHell;`. With the whole ladder in one call at the top,
both lines are gone — the difficulty blocks now do only what they are named for.

## Verified

**415 tests, the same two pre-existing failures.** Two new ones:

- The champion invariant, checked over **every combination of the seven resistance bits in both
  columns on all four difficulties** — 16,384 pairings per difficulty — rather than a walk over
  `MonstersData`, which is not exported to the test binary and which only contains the pairings the
  game happens to ship. This covers the ones it does not.
- The Nightmare rung: immunities demoted rather than granted early, plain Hell resistances carried
  across rather than dropped, and nothing the Normal column granted taken away.

## Where Phase 3 stands

3.1 (TRN recolor variants wired into zone rosters) is **blocked, not skipped** — there is no zone to
wire a roster into until Phase 4 creates one, and `zone_registry.h` explicitly refuses speculative
columns. It also has a prerequisite nobody had noticed: `DrawMonster` only consults
`uniqueMonsterTRN` for uniques, so ordinary-monster recolours need that condition widened first.

3.2 shipped at v1.7.20 (Colossal). 3.3 is this. **3.4** — aura-carrying champion packs — is the
last one, and it wants the monster-facing aura pass that four classes' inert tree rows are also
waiting on. That pass is now the highest-leverage single piece of work left in the plan.

# Signets and Milestones

**Version:** v1.9.19 -> v1.9.20
**Date:** 2026-08-22
**Phase:** D2MXL-to-ORCL Phase 2

## What shipped

Permanent character progression that survives your gear, with a **lifetime cap of 20**, paid by
eight one-time milestones:

| Milestone | |
|---|---|
| Reach level 20 / 40 / 60 / 80 | depth |
| Slay a Dread boss | combat |
| Complete a runeword | system |
| Fill an item with Mystic Orbs | system |
| Wear a set bonus | system |

Each pays one signet point, the first time it is met and never again. The point goes into the
**unspent pool**, so the player decides where it lands — a signet is a point, not a prescription,
and it inherits every rule the ordinary pool already has.

The mix is deliberate. A list that was all levels would just be the levelling curve again; one that
was all systems would be unreachable for a player who has not met those systems yet.

## The cap is the design

Without it a signet is a slower level-up. With it, the pool is a finite resource you can exhaust and
then must live with — twenty points is roughly a fifth of what levelling to 99 hands out, enough to
matter and far too few to replace levelling.

## The plan said PlayerPack. The plan was wrong, and better than it knew

Phase 2 was planned as "no format change, it goes in `PlayerPack`". Checking that before building
turned up two things:

1. **`PlayerPack` cannot grow for free.** It is a fixed struct, its reserved bytes were all
   repurposed years ago, and its own header comment records that growing it at 1.5.0 broke every
   character then existing — "affordable precisely then and probably never again".
2. **It does not have to.** `pfile.cpp`'s `ReadHero` accepts a **chunked tail** past the struct, and
   `oracool/hero_chunks.h` is a tagged, length-prefixed, forward-compatible list built exactly so
   that break never has to happen again. Its contract is one sentence: add a tag, write it, apply
   it, nothing else to touch.

So both values ride the tail as `HeroChunkMilestones` (u32) and `HeroChunkSignets` (u8). **This
phase breaks no save and bumps no version** — which is what the plan claimed, arrived at by a
different route than the plan described.

The `PlayerPack` header comment is now out of date about `ReadHero` requiring an exact size match;
that is worth correcting, but not in this unit.

## Decisions worth keeping

- **A milestone is claimed before its reward is awarded.** A milestone met with the pool already
  spent still counts as done. One that stayed claimable would pay out later, out of order, for
  something the player did hours ago — and would read as a bug.
- **The boss milestone is claimed from the KILL, not the loot.** A boss that dropped nothing still
  counts; tying it to the drop would make a milestone depend on a roll.
- **The runeword milestone is claimed at completion**, because that is the only moment it can be
  noticed — afterwards the item is just an item with a name.
- **The orb-cap milestone goes through the passive walk** rather than testing the item at the paste
  site, so "an item at its cap" has exactly one definition.
- **Unknown milestone bits are masked away on read.** A save from a later build with more
  milestones must not mark one this build cannot name as claimed — that would silently withhold a
  reward it is supposed to pay.

## The trap I nearly repeated

The per-player side tables were first written as `uint32_t ClaimedMask[Players.max_size()]` —
`Players` is a vector, so that is not a compile-time size and the build refused it. Sized to
`MAX_PLRS` with the index **clamped**, because that is the exact bug found in the treasure-class
test an hour earlier: an index that is almost always in range, writing off the end of a stack array
on the day it is not.

## Still open, deliberately

The **Signet as a droppable item** is not here. The mechanism — cap, permanent points, the
milestones that pay them — is what makes the phase worth having, and it needed no art, no icon
strip rebuild and no MPQ repack. The drop is a clean follow-on: a generator, one item, a drop hook
and a use path.

## What to look at in game

Level to 20 and check the event log says a milestone was met and the stat point arrived. Then level
to 40 and confirm it happens once more, not again for level 20.

# Audit of the MXL Programme

**Version:** v1.9.23 -> v1.9.24
**Date:** 2026-08-22
**Scope:** v1.9.19–1.9.23 — Mystic Orbs, Signets and milestones, growing charms, named encounters
**Tests:** 511/513 (the two standing baseline failures)

## The finding that matters

**"Rework Charms" would have destroyed the best items in the game.**

Recipe 2 takes two charms and returns one random *basic* charm. Its input predicate was:

```cpp
bool IsCharm(int idx) { return IsOracoolCharmIdx(idx) && !IsOracoolSalvageCharmIdx(idx); }
```

That was correct when there were exactly two charm families. It stopped being correct the moment the
charm space grew — **silently**, because the predicate kept compiling and kept saying yes to
everything new.

By v1.9.23 that meant a **Chapel Reliquary** — a guaranteed named-encounter reward, which cannot be
farmed for, costs a Sealed Map to earn, and is worth 40,000 gold — could be fed into the recipe and
come back as a Charm of Vigor. The three growing charms the same. A recipe meant for surplus was
quietly a recipe for annihilating the rarest items in the fork.

Fixed by replacing the exclusion with an **explicit list of the six basic charms**. Every
"everything except the ones I have thought of" predicate in this codebase has eventually been wrong;
a list can only go wrong when someone adds a basic charm and does not add it here — which is a
change that makes them look at the line.

This is the third time in a fortnight that a *widening* predicate has silently absorbed a new
family: the socketable `candidates[]` array, the treasure-class `counts[4]`, and now this.

## Also fixed

**An inert guard in `PlaceNamedEncounter`.** It read:

```cpp
const size_t typeIndex = AddMonsterType(...);
if (typeIndex == LevelMonsterTypeCount) return;
```

The `index == LevelMonsterTypeCount` idiom belongs to `GetMonsterTypeIndex`, where that value means
*not found*. `AddMonsterType` increments the count as it adds, so the comparison after it can never
be true. Removed rather than corrected — an inert guard is worse than no guard, because it reads
like protection.

## Checked and found correct, so recorded rather than re-derived

- **`AddMonsterType` loads its own sprites and sounds** (`InitMonsterGFX` / `InitMonsterSND`), which
  is exactly what makes the encounter boss work in an arena that loads no monster types at all. This
  was the largest open risk in the Phase 4 plan.
- **The encounter reward charms are excluded from the seeded droppable pool** via
  `IsOracoolCharmIdx`; the **Sealed Maps** were not, and that was fixed in v1.9.23 when writing the
  test caught it.
- **Salvage declines every new family** — all are `ICLASS_MISC` / `ILOC_UNEQUIPABLE`.
- **A Sealed Map cannot be used outside town**, and is not consumed when refused.
- **`EnterNamedEncounter` requires town specifically**, not "town or another encounter" as the
  `/arena` command allows — so it is impossible to be inside one with no way back.

## Wiki

The wiki had every new **item** (the tables are generated from `AllItemsList`, so 433 items included
all of them) and almost no **prose** about what any of them do. Data-complete and
explanation-empty — which is the failure mode that looks fine from a distance.

Added, all parsed from source rather than typed, because every number in them is a tuning value the
telemetry is expected to correct:

- **mechanics.html** — "Progression that outlives your gear" (the signet cap, the drop rate, the
  eight milestones read out of `MilestoneName`) and "Named encounters" (the three, with place,
  tileset, map and reward read out of `Places[]` and the generated item table).
- **sockets.html** — the three kinds of charm now sharing one cap, and the rule that **only basic
  charms can be reworked** — which is the fix above, stated where a player would look for it.

## What still cannot be verified without playing

Everything that matters most in Phase 4: that an arena enters at all in single-player, that a boss
appears in a room with no authored spawn points, that the exit returns you to town, and that the
reward lands. Plus the recipe book's scroll and select, the orb paste, and the milestone log lines.

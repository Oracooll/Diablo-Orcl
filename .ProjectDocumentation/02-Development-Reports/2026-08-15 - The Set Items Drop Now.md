---
date: 2026-08-15
version: 1.6.22
area: Items / set drops
---

# The Set Items Drop Now

> "for 6 level not a single new tier item dropped. i think they dont drop at all. check your code."

The user was exactly right, and the numbers were stark: **143 of the 143 set item rows shipped
`IDROP_NEVER`.** Six levels of play were run against a drop pool that structurally could not contain
them - the entire eight-tier, thirteen-slot range existed only for the debug spawn commands.

## The first fix was wrong, and the tests caught it in one run

The obvious flip - `IDROP_NEVER` to `IDROP_REGULAR`, joining the ordinary pool - broke eight pack
tests immediately, and the failure named the mechanism: *"Jade Great Helm" went in, "Jade Leggings"
came out.* `UnPackItem` recreates a dungeon item's INDEX by replaying its seed through
`GetItemIndexForDroppableItem`'s exact walk - **the candidate pool is part of the save format.**
Growing it re-routes every seeded recreation, which would have silently transformed items on
existing characters. This is the `MAX_ITEM_SPELLS` lesson wearing a new coat, and it is why the
regression suite exists: one run, one precise answer.

## The shipped design

- **`IDROP_REGULAR` stays** - it is what makes a set item legal on the loopback wire
  (`IsDungeonItemValid` rejects NEVER-rate dungeon items as invalid packets).
- **The seeded pool explicitly excludes the set range** (`IsOracoolItemIdx` guard in
  `GetItemIndexForDroppableItem`), so every seeded recreation - saves, character preview, the pack
  tests - is byte-identical to before.
- **`TrySpawnOracoolSetItem` is their own drop path**: called from `SpawnLoot` AFTER the vanilla
  rolls (so the `rndItemSeed`-driven stream is untouched), roughly one monster in twelve, choosing
  among every set item whose `iMinMLvl` the monster's level has earned. The tier ladder is already
  in that data - leather at monster-level 1-2 rising to spectral at 50 - so deeper floors drop
  better tiers with no table here. Construction goes through the same `SetupAllItems` call the
  debug commands use, magic rolls and the fork's tier ladder included, with the same level-30 clamp
  that keeps the loopback validator satisfied.
- **Single-player only**, like the tier system itself: the compact multiplayer pack cannot recreate
  an item that is not in the seeded pool, and V1 does not play multiplayer.

## What the player should see

Set items on the ground with their new green names, about one kill in twelve, tier-matched to the
floor. The drop rate (8%) is the number to tune by feel - it is one constant.

## State

**368/370** - the usual two. The eight pack tests that failed against the naive fix all pass against
the shipped one, which is precisely the difference between the two designs.

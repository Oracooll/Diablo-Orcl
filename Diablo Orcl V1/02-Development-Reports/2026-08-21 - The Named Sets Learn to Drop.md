# The named sets learn to drop

**Version:** 1.8.99
**Date:** 2026-08-21

Asked to build the set bonus system. It was already built - so this is the work that was actually
missing underneath it.

## The set bonus system had shipped

`ApplySetBonuses` is a registered provider in `stat_sheet.cpp`, `ApplySetBonusesToTotals` walks every
rung **cumulatively**, all 73 rungs carry real powers, and two tests pin it. It even handles the
subtle case where `IPL_ACP` on a rung has no item to be a percentage *of* - read as flat armour,
because otherwise "Deep Foundation"'s +12 armour was worth exactly 1 point.

The Pipeline row was stale, the third such row found this week.

## What was missing: the sets could not be earned

Two findings, and the second is the serious one.

### 1. Twenty-one of ninety-four pieces could not be built

`BaseItemForSetSlot` returned -1 for amulet (11 pieces), ring (8), relic (1) and cloak (1). The
recorded reason for the first two was that naming a droppable ring/amulet row means inserting into
`_item_indexes`, and item indices are positional save format.

**`ItemMiscIdIdx` answers the same question without touching the enum** - it finds the first
droppable row carrying a misc id. It is also bounded as of v1.8.94 and returns `IDI_NONE` on a miss,
which is exactly the -1 this function already means by "no base", so the failure path needed nothing
new.

**73 -> 92 buildable.** Only the relic and the cloak remain, and those are slots this fork has
genuinely not built.

### 2. The named sets had NO drop path at all

`TrySpawnOracoolSetItem` is misleadingly named. It drops **tier items** from the worn-type range -
"set" only in the sense of a matching suit. The 94 pieces of the fifteen designed sets were reachable
**only** through `giveitemset`. Fifteen sets of artwork, stats, requirements and a whole cumulative
bonus ladder, unreachable in play.

`TrySpawnNamedSetPiece` is their hook: 3% per monster, gated by each piece's own `requiredLevel`
against monster depth, and filtered to pieces that can actually be built so the two unbuilt slots are
never rolled and silently dropped.

**3%, not the tier hook's 8%**, deliberately: a named piece is a step toward a COMPLETE set rather
than a self-contained reward. The ladder is the payoff, and a set finished in an afternoon has no
ladder worth climbing.

It sits beside the other hooks in `monster.cpp`, after the vanilla spawns, for the reason recorded
there: the `rndItemSeed`-driven stream above must stay byte-identical, and none of these can ride the
ordinary droppable pool because that pool is replayed from item seeds on unpack.

## Verification

**490/492**, the two standing baseline failures. One new test:
`EveryPieceButRelicAndCloakHasASpawnableBase` pins the count at exactly 92 spawnable and 2 not,
asserts every resolved base is a real row, and names the only two slots allowed to fail.

The count is pinned exactly rather than loosely because the failure it guards is silent: an
unspawnable piece is simply never rolled, so a set quietly becomes uncompletable and nothing says so.

## Still open on sets

The third gap from the Pipeline row - **no gold mechanic tied to sets** - is untouched, and sets
still drop piece by piece rather than as sets. Whether they should drop as sets is a design question:
"drops as a set" collapses the chase the 3% rate exists to create.

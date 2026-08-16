---
date: 2026-08-16
version: 1.7.7
area: Megaplan Phase 1 - the item endgame, closed
---

# Phase One, Complete

The item endgame is whole: all seven units shipped (v1.7.0-1.7.7), each audited as it closed.

## The two closing units

**Gambling at Wirt (v1.7.6).** Wirt is the gambler, in his own existing shop: the item on his
table is UNIDENTIFIED - you buy the base type and the roll reveals itself in your pack. The magic
roll still happens at stock time (seeded, CF_BOY-validation-safe); only the release of the
knowledge moved to the purchase, which is exactly how Gheed sold it. Every purchase immediately
restocks, so it is a repeatable gold sink. Audited: PrintStoreItem gates its affix lines on
_iIdentified, so the peek does not leak the roll.

**The crafting window (v1.7.7).** The Horadric Cube's recipes, no cube: a window on the burger
menu's reserved slot. Transmute Gems (3 of a kind -> random rune), Ascend Runes (2 identical ->
next rung), Rework Charms (2 any -> random charm) - the loop that turns surplus drops into the
Ral that Ancient's Pledge needs. No craft half-executes: output room is checked before anything
is consumed, and consumption walks InvList top-down because RemoveInvItem compacts from the end.

## What Phase 1 adds up to

Plain "basic items" carry sockets; gems and runes fill them with host-dependent effects; exact
rune sequences transform items into named runewords whose recipes the runes themselves teach;
charms work from the backpack under an active cap; ethereal drops trade lifespan for power; Magic
and Gold Find (charm-fed) bend the drops; Wirt gambles; the crafting window recycles the surplus.
Every piece rides the Phase 0 seams - the provider walk, the unseeded drop tail, the pool
exclusions, the versioned item record - and the pack golden tests never went red once.

## The audit trail

Every unit closed with its checks; the standalone audits (v1.7.5 and the in-unit checks here)
found and fixed: the invisible runeword rename, stale telemetry TTK clocks, the ExpLvlsTbl OOB
read at the cap, and the headless loadgfx crash in crafting. All pinned by tests.

## Next (Phase 2, next session)

Skill points feeding the existing ladders, the tree UI, respec at Adria, the Paladin auras per
their existing plan doc, and the run toggle. The SkillPoints hero chunk has been round-tripping
through every save since 1.6.27, waiting.

**State: 386/388** - the usual two. Twenty-one versions this run, all committed and pushed.

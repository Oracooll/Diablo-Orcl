---
date: 2026-08-20
version: 1.8.74
tags: [debug, items, sets, ethereal]
---

# givesset and giveeset

Two commands, per the user: *"give as full as possible set of set and etherial items so i can test
them."*

## givesset does not go through the roller

Its five siblings are one line each - `DebugSpawnEquipmentSet(OracoolItemTier::Rare, ...)` and so on
- and this one deliberately is not.

Those ask the affix roller for a tier. **Set is the one tier that is not a roll.** A set piece is a
specific named object with a fixed stat list, which is exactly why set items are kept out of the
roller (see `OracoolItemTier::Set`'s own comment). Asking the roller for a Set-tier item would
produce a Set-coloured object belonging to no set - worse than useless for testing sets, and the
kind of thing that looks like it works.

So it walks the real table and takes the first piece that fits each slot. The result is a **mongrel**
- up to thirteen pieces from thirteen different sets - which is exactly right for testing that every
slot renders, colours and equips, and exactly wrong for testing set BONUSES. The reply says so and
points at `giveitemset N` for one coherent set.

## giveeset stamps rather than rolls

Ethereal is not a quality; it is a stamp applied on top of a finished item. So `giveeset` reuses the
per-slot spawner with a new flag and applies `MakeItemEthereal` **last** - the same order the drop
path uses, where `TryMakeDroppedItemEthereal` runs after `SetupAllItems`.

That helper carries the whole bargain (+35% AC or max damage, max durability halved) and declines
the items that cannot be ethereal, so the spawner needs to know nothing about what ethereal means.

Magic bases rather than plain: an ethereal white has almost nothing to show, and the point is to see
the stamp against real numbers.

## One correction

`SetItemDefinition::slot` is the design's string - "helm", "main_hand" - not an engine enum, so the
slot-coverage set is keyed on the word. My `static_cast<int>` compiled in my head and not in the
compiler.

## Verification

489 tests, the two standing baseline failures only.

The wiki's generated command table picked both up by itself - `BuildWiki.ps1` parses `debug.cpp` -
and the hand-written "worth knowing" cards gained one each. Artifact republished; MPQ repacked.

Neither command is testable headlessly (both need `MyPlayer` and a live level), so they want a
`givesset` and a `giveeset` in town: thirteen green names for the first, and for the second, the
ethereal line on every piece.

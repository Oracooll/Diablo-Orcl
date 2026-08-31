---
date: 2026-08-15
version: 1.6.12
area: Self-audit / item system, equip slots, gold flow
---

# Audit Round Ten — Items, Equip Slots, Gold

The item system was the last large fork-touched surface without a dedicated pass. Third consecutive
clean round: nothing needed changing.

## Audited and found sound

**The affix save extension's load path** — the model answer to round six's question ("does the
file's shape prove its references?"). Tier clamped to the enum's range, both affix counts clamped to
`MaxOracoolAffixesPerSlot` before any loop trusts them, every affix type validated through
`IsOracoolAffixTypeValid` or dropped to `IPL_INVALID`, and all six array slots read unconditionally
so the record's stride stays fixed for the size checks upstream. `RepairOracoolAffixesIfCorrupted`
runs at the same choke point for every container in the game, and its loops can only see counts the
loader already clamped.

**The six new equip slots** — `CheckInvSwap` needs no per-slot handling beyond the hands (only the
two-hand rule is positional; everything else is "recalc"), the msg-layer caller bounds `bLoc` against
the widened `NUM_INVLOC`, and the ILOC→INVLOC mapping is a closed switch. The grid-sync overload of
`CheckInvSwap` takes an unbounded `invGridIndex` from the network — noted, not fixed: it runs only
for remote players, which V1's single-player loopback never produces, and multiplayer is explicitly
out of scope.

**The gold flow's inverse side** — `GoldAutoPlace`'s stash deposit is overflow-guarded with the
ceiling case falling through to the inventory (and the comment documents exactly that case);
`CalculateGold` sums only inventory stacks (≤ 200k, no overflow); `RoomForGold` tops out at 350k.
All three consistent with round seven's saturated `TotalPlayerGold`.

## Standing

Ten rounds, twenty-one fixes, three clean rounds in a row (8, 9, 10). Every fork-touched gameplay
system now has a dedicated audit pass on record. What remains unswept is multiplayer-only code
(out of scope by project rule), vanilla systems the fork never modified, and art/geometry that
self-evidences on screen.

**354/356** — the usual two. No code changed this round; the build stands at v1.6.12.

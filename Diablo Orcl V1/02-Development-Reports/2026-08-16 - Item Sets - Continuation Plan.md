---
date: 2026-08-16
version: 1.7.46
tags: [item-sets, plan, handoff, phase-1]
status: OPEN — paused deliberately, resume when the user says so
---

# Item Sets — Continuation Plan

The fifteen sets are **built in** as of v1.7.45: 94 items, 73 bonus rungs, icons, the Set tier, and
the bonus ladder all work. This is the list of what would make them *finished*, written down so it
can be picked up cold.

**Paused on the user's call (2026-08-16): "we have to make all sets work, but not now."** Nothing
here is urgent and nothing here is broken — every gap below is a boundary the code states plainly
rather than a defect it hides.

## Where things stand

| | |
|---|---|
| Sets | 15 |
| Items | 94 — **73 spawn today**, 21 cannot |
| Bonus rungs | 73 (2/3/4/5/6-piece ladders) |
| Stat lines | 540 declared, **390 live** (72%) |
| Keywords | 107 — 36 live, 71 inert |
| Only way to obtain one | `giveitemset {1-15}` |

Read first: `oracool/item_sets.h`, then `oracool/item_set_stats.h`. The tables are **generated** —
`tools/GenItemSets.ps1` — and hand-editing them will be overwritten.

---

## Task 1 — the 21 unspawnable items

`BaseItemForSetSlot` (`oracool/item_sets.cpp`) returns -1 for four slots:

| Slot | Items | Why |
|---|---|---|
| amulet | 11 | no base item with a named `IDI_` constant |
| ring | 8 | same |
| relic | 1 | slot not built in this fork |
| cloak | 1 | slot not built in this fork |

**Amulet and ring are the whole job** — 19 of the 21. Vanilla *has* droppable ring and amulet rows in
`AllItemsList`, but they are anonymous entries (`/* */` comment, no `IDI_` name) so nothing can
reference them. Two options:

- **(a)** Add `IDI_ORACOOL_RING` and `IDI_ORACOOL_AMULET` base rows, the way the other Oracool bases
  were added. Small, self-contained, matches existing practice. **Recommended.**
- **(b)** Give the anonymous vanilla rows names. Touches vanilla data; every index after them must be
  re-checked. Not worth it.

Relic and cloak are two items and need two new equip slots — leave them until the slots exist for
their own reasons, or retire those two items.

How it lands unevenly, worst first:

```
Leoric's Fallen Court            8 of 13   (5 blocked)
Choir of Silence                 3 of 5    (2)
Dawnwarden's Reliquary           4 of 6    (2)
Steps of the Empty Hand          5 of 7    (2)
The Rat King's Tithe             3 of 4    (1)
The Lost Cartographer            3 of 4    (1)
...every other set               short by 1
Vestments of the Ashen Saint     6 of 6    — complete today
```

Doing task 1(a) alone takes 73 → 92 of 94, and makes 14 of the 15 sets completable.

## Task 2 — make them drop

Nothing is in a loot table. Decisions needed before any code:

- **What drops a set piece?** Champions and lesser uniques already exist (`oracool/lesser_uniques`)
  and are the obvious carrier.
- **At what rate**, and does Magic Find apply? `_pMagicFind` exists and feeds the unseeded drop tail
  (`ApplyMagicAndGoldFindToDrop`).
- **Does the set's `requiredLevel` gate the drop, the equip, or both?** The data carries a level per
  item *and* per set; only the item's is used today.

Whatever the answer, the roll belongs in the **unseeded** drop tail beside Magic Find, not in seeded
setup — see the note in `stat_sheet.h` about why that boundary matters.

## Task 3 — the 71 inert keywords

Each has a note in `oracool/item_set_stats.cpp` saying what building it would take. They cluster,
which is the useful part — implementing one counter usually lights several keywords at once:

| Cluster | Keywords | What it needs |
|---|---|---|
| Crimson Compact petitions | 6 | a per-player counter + spend hook |
| Choir of Silence condemned/hush | 8 | a monster silence state |
| Wyrmhide temper/shed | 7 | a state machine on the wearer |
| Clockwork Penitent engine | 4 | a triggered-effect scheduler |
| Lost Cartographer route | 4 | a tile-visit tracker |
| Leoric's court rank | 3 | a rank counter |
| Confluence | 4 | a sequence tracker |
| `proc:` (30 distinct) | 30 | one implementation each; the name is the whole spec |

Cheap wins that are **not** clusters, and could be done any time:

- `gold_from_monsters` — `Player::_pGoldFind` already exists and charms already feed it. It only
  needs an `IPL_` that writes it. **One new power, five keywords' worth of honesty.**
- `walk_speed` — the Vigor aura already makes a player run. Needs the same effect reachable as an
  item power rather than only an aura.
- `damage_vs_undead` — no channel; `IPL_ACUNDEAD` is *armour* against undead, not damage to them.
  Would need a new flag beside `TripleDemonDamage`.

## Task 4 — presentation

Not started, and worth doing once sets actually drop:

- The character sheet does not show **which set you are wearing** or **which rung you have reached**.
  `WornSetPieces` and `ActiveSetBonus` already answer both.
- A set item's description does not list its **set's other pieces**, the way Diablo II's does.
- An **inert rung shows nothing**. "Cinderbrand" is earned at four pieces and grants nothing; the
  player is currently told neither. It should be named and visibly inert — the class tree's rule.

## Don'ts

- **Do not hand-edit** `item_sets_data.inc` or the four `item_sets_curs*.inc` fragments. Re-run
  `tools/GenItemSets.ps1`. The CEL's frame order, the `ICURS_` ids and the two width/height tables
  are a four-way contract and a CEL cannot detect a mismatch.
- **Do not add a save field** for set membership. `_iCurs` is the identity — every one of the 94 has
  its own icon, and `_iCurs` is already packed and synced.
- **Do not write a second `IPL_` switch.** Set bonuses go through a scratch item and
  `ItemBonusTotals::AddItem` precisely so there is only ever one.
- **Do not silently approximate an inert keyword.** The generator refuses unknown words on purpose;
  that refusal already caught one wrong mapping (`skill` → `IPL_SPELL`) before it shipped.

## Suggested order

1. **Task 1(a)** — two base rows, 19 items unblocked, 14 of 15 sets completable. Smallest change,
   largest effect.
2. **Task 4** — presentation, so the sets are legible before they are common.
3. **Task 2** — drops, once the above make a found set worth finding.
4. **Task 3** — the inert clusters, one at a time, cheapest first (`gold_from_monsters`).

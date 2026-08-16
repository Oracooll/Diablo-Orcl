---
date: 2026-08-16
version: 1.7.45
tags: [item-sets, content, generator, phase-1]
---

# Fifteen Sets, Ninety-Four Items, One Generator

Fifteen named item sets landed in the drop zone. 94 items, 73 tiered bonuses, full stat lists,
28px icons already cut. This is what it took to get them into the game, in five versions.

## The shape of the problem

The sets are written in a vocabulary of their own — **107 stat keywords**, extracted from the fifteen
`set-data.json` files. About a third name something this engine has had since 1996. The rest name
bespoke per-set machinery: petition counters, shed charges, confluence sequences, court ranks.

So the first question was not "how do I add 94 items" but "what do these words mean here". Everything
else followed from answering that once, in one table.

## Step 1 — the vocabulary (1.7.42)

`oracool/item_set_stats.cpp`: every keyword, with a verdict.

- **Power** — the engine does what the word says. 36 keywords.
- **Approx** — it does something adjacent, and the note says what was traded.
- **Inert** — no channel; the note says what building it would take. 71 keywords.

The IPL semantics were read out of `SaveItemPower`, not inferred from the names, which mattered in
three places that would each have been a silent bug:

- `IPL_ACP` writes `_iPLAC`; **`IPL_TARGAC`** is the one that writes `_iPLEnAc`.
- `IPL_GETHIT` **subtracts** its parameter, so a positive value reduces damage taken.
- `IPL_FASTATTACK` / `IPL_FASTRECOVER` take a discrete **tier**, not a percentage.

71 inert out of 107 sounds like defeat and is not: the *head* of the distribution is ordinary RPG
stats on nearly every item, and the tail is one-off mechanics appearing once or twice.
**390 of the 540 declared stat lines — 72% — compile to a real power.**

## Step 2 — the content (1.7.43)

`tools/GenItemSets.ps1` reads the fifteen JSONs and writes `item_sets_data.inc`. It reads the keyword
meanings **out of the C++ table itself** rather than keeping a second copy, and it **refuses** an
unknown keyword rather than defaulting it to zero.

That refusal caught an error in my own step-1 table within minutes: `skill` was mapped to
`IPL_SPELL`, on the reasoning that the engine can grant a spell from an item. It can — but all six
`skill:` values name abilities invented for the sets (`pass_judgment`, `wyrmturn`,
`deploy_penitent_engine`, `invoke_crimson_compact`), and none is a SpellID that exists. `IPL_SPELL`
would have had nothing to grant. Corrected to inert before it shipped.

## Step 3 — the icons (1.7.44)

The strip had 29 frames for 29 Paladin skills and needed 94 more, at 412..505.

A CEL stores **no names and no sizes** — a frame's position in the file is the only thing tying it to
an `ICURS_` id. So four things have to agree exactly: the CEL's frame order, the enum, and the two
width/height tables in `cursor.cpp`. The generator now emits all four in one walk, in one order.
Ninety-four hand-written spec lines would have been ninety-four chances to put them out of step, and
nothing would have complained — every icon after the first mismatch would simply have been wrong.

`ItemIconCel.cs` gained an **`asis`** mode. The delivered sprites are already exactly grid×28 with
real transparency; the normal path tightens art to its content box and scales that back up to fill
the cell, which would have re-centred and resampled work that was already correct.

Two failures worth recording, both diagnosed from symptoms that named neither cause:

- `set-NN-*/` inside a `/** */` block comment **closed the comment early**. The compiler reported a
  syntax error four lines later and complained about backticks.
- `Set-Content -Encoding utf8` writes a **BOM**, and the spec list is appended to another file with
  `type`. The BOM landed mid-file and became the first characters of a path, surfacing as
  `System.NotSupportedException` from deep inside `System.Drawing`.

## Steps 4-5 — the tier and the bonuses (1.7.45)

**`OracoolItemTier::Set`**, green — the ramp reserved for it when Primal was moved off green
("Green is for future Set Items", 2026-08-16).

**A set item needs no new save field.** Every one of the 94 has its own icon, so `_iCurs` *is* the
identity — and `_iCurs` is already packed, already synced, already survives a round trip. Recognising
set items therefore costs no save-format change at all.

**Set bonuses ride the Phase 0.4 provider seam**, whose header named "three pieces worn?" as the
reason its condition hook exists. It finally has its caller.

A rung's stats go onto a **scratch item**, which then goes through the same
`ItemBonusTotals::AddItem` every worn item goes through. The alternative — a switch mapping each
`IPL_` onto a totals field — would be a second copy of `SaveItemPower`'s switch, and the two would
drift the first time either gained a case. This way a set bonus is worth exactly what the same stats
would be worth on a piece of equipment, by construction.

Rungs **replace** rather than stack: the delivered ladders already restate the lower rungs' stats in
the upper ones. A rung can be entirely inert — "Cinderbrand", the Ashen Saint's four-piece, is a
single `proc:` — and it is still **named**. You are told what you earned.

## What is NOT done

**21 of the 94 items cannot be spawned** — amulet 11, ring 8, relic 1, cloak 1. Their slots have no
base item this code can name. The vanilla ring and amulet rows are anonymous entries in
`AllItemsList` with no `IDI_` constant to reference; relic and cloak are slots this fork has not
built. `BaseItemForSetSlot` returns -1 for them and `giveitemset` reports the shortfall rather than
quietly handing over four pieces of a six-piece set.

(Corrected after publication: this said 30, which was an estimate I never checked. Counted from the
generated table it is 21, so 73 of 94 spawn today. It also lands unevenly — only Leoric's Fallen
Court is badly hit at 5 of 13; every other set is short by one or two.)

**Nothing drops.** The sets are not in any loot table. `giveitemset {1-15}` is currently the only way
to see one.

**The 71 inert keywords are still inert.** Each names a mechanic that would need building.

## Shipped

`ORACOOL_VERSION` 1.7.45. MPQ repacked; `oracool_items.cel` is 277 frames (183 + 94). 428 tests,
13 of them new, the two long-standing failures unchanged.

## Worth noting

The generator caught a mistake in the table the generator was written against. That is the whole
argument for making content pass through a program that can refuse it: a human reading 107 rows would
have nodded at `skill → IPL_SPELL` too.

---
date: 2026-08-19
version: 1.8.14
area: Wiki - a Pipeline page, and the backlog file it replaces
---

# The Pipeline Page

The user asked for a Pipeline section holding every idea floated and not expressly discarded, to
pick the next build from.

## The old backlog could not be used as-is

`07-Backlog/Idea-Backlog.md` has been the holding pen since August 5th and its own preamble says
"nothing gets removed except by moving it to built". That did not happen. **A dozen entries marked
"not started" had shipped** - charms, set items, crafting, sockets and gems, runewords, skill
trees, item tiers, the HUD rebuild, gambling, the run toggle. Publishing it would have handed the
user a menu where a third of the dishes were already on the table.

So the pipeline was rebuilt from scratch against what is actually in the source, drawing from four
places: the megaplan's Phases 3-6, the standing deferrals, this directive's own unfinished points,
and the backlog entries that survive verification.

## 37 open entries

Grouped by what kind of decision each needs:

- **Directive (5)** - Levski's Roar, socket extraction, moving crafting into it, salvaging with its
  seven named materials, and the blank point 10.
- **Content (4)** - the 107 remaining uniques, named set drops, the 97 unbuilt tree rows, skill
  sounds. More of something that already works.
- **Art (2)** - the MPQ drop zone's two unconsumed units.
- **Phases 3-6 (14)** - the megaplan's own stages in its own order.
- **Balance (5)** - resistance soft cap, health globes, movement-speed affixes, skill synergies,
  and the telemetry pass. The CSV has been collecting since Phase 0.9 and has never been read back.
- **Systems (7)** - enchanting, paragon, perfect-roll indicator, transmog, legendary powers, the D2
  shop, the randomised dungeon.

**28 can start today. 9 wait on something, and four of those nine wait on the same thing** -
Levski's Roar, which makes it the single biggest unblocker on the list. The page says so in prose
rather than leaving it to be inferred from a table.

## Generated, not typed

The source is `07-Backlog/Pipeline.md` - a pipe table the user edits in Obsidian, parsed by
`BuildWiki.ps1`. Given the last two audits both found hand-typed wiki content that had drifted, a
hand-written HTML list of three dozen entries would have been stale within a week. It deliberately
does not read the old backlog file, whose staleness is the reason this one exists.

Each entry carries a size (Small = a session, Medium = a few units, Large = a phase) and a save
impact, colour-coded - because save impact is what decides whether something can be slotted in
casually or has to be batched with other breaking work, and that has been a standing rule since
August 6th.

## Verified

37 rows parsed, 28 ready and 9 blocked, checked in the browser. No engine code touched, so tests
stand at **452, the usual two**.

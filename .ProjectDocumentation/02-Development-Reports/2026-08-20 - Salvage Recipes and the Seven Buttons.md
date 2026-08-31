---
date: 2026-08-20
version: 1.8.72
tags: [salvage, levski, items, ui]
---

# Salvage Recipes and the Seven Buttons

## The buckets partition the item space

Every salvageable item falls in exactly one of the seven, so the buttons between them consume each
item once and no item twice.

That is not the only design available - an ethereal rare could plausibly yield both Rare Fibres and
an Ethereal Imbueity. It is the one the **buttons** demand: with overlap, "Salvage all rares" and
"Salvage all ethereal" would each claim the same object, and which one you pressed first would
silently change what you got.

Priority runs most specific first - **ethereal beats set beats tier beats plain quality**. An
ethereal item is an ethereal item whatever else it also is.

| Bucket | Yield |
|---|---|
| Whites | 1 White Scale |
| Magic | 1 Magic Powder |
| Rare | 2 Rare Fibres |
| Uniques | 3 Unique Encrustments |
| Primal | 4 Primal Vines |
| Set | 3 Set Engravings |
| Ethereal | 2 Ethereal Imbueities |

## What salvage refuses

Gold, quest items, and **every socketable**. A salvage-all that could eat a stack of Zod runes
because the wrong box was clicked is the one bug this system must not have, and the socketables are
exactly what a player is hoarding for the crafting system salvage feeds. Weapons and armour only.

## The column, not a row

Seven buttons wide enough to read would be over 700px in a row - half the screen. Stacked down the
right of the grid they cost 140px of width and reuse height the 3x4 grid already occupies. The
window grew by exactly the column plus its gap, so the grid and its two buttons did not move.

Each box **lights** when the backpack actually holds something that button would consume, so the
column doubles as a readout rather than being seven identical boxes. And every press reports, even
an empty one: a button that silently does nothing because you own no rares is indistinguishable
from a button that is broken, and this fork has shipped that exact ambiguity twice.

## The one loss path, made loud

Materials go back into the space the gear just vacated, and they stack - so overflow needs a single
press to yield more than 99 materials while freeing almost no space. Close to impossible, but if it
happens the loss is real, so it says so in the event log rather than leaving a short count to be
noticed later.

## Verification

488 tests, the two standing baseline failures only.

`OracoolAudit.SalvageBucketsPartitionAndRefuseMaterials` walks every rune, gem and material and
asserts each declines; confirms an ethereal rare lands in Ethereal and not Rare; and checks the
seven tiers name seven distinct materials, so no two buttons produce the same orb.

**The window itself needs eyes** - nothing renders a panel in the suite. Open Levski's Roar and
check the seven boxes sit inside the widened window, that the lit/unlit states track what you are
carrying, and that pressing one reports in the log.

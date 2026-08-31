# The Signet Becomes a Thing You Find

**Version:** v1.9.20 -> v1.9.21
**Date:** 2026-08-22
**Phase:** D2MXL-to-ORCL Phase 2b
**Tests:** 509/511 (the two standing baseline failures)

Phase 2 shipped the mechanism. This is the half that was deliberately left out: the Signet of
Learning as an item that drops.

## Champions and better, only

`TrySpawnSignet` declines outright when `TreasureBonusFor(monster) <= 1`. **An ordinary kill never
yields one.**

That is the whole design of the drop half. Milestones are the *reliable* source — eight of them,
each paying once — and the drop is the bonus. A bonus that fell off ordinary monsters would be a
slow trickle nobody could aim at; tied to champions it is a second reason to cross the room for one,
on top of the treasure class.

3% times what the monster is worth: **6% on a champion, 12% on a unique, 18% on a Dread boss**. Over
a full clear that is roughly six signets, against eight from the milestones — comparable, so neither
half makes the other pointless.

Its own hook rather than a seat in the socketable draw. That draw is one budget shared by five
families, and a sixth would quietly make every one of them rarer. This is additive.

## The refusal is the part that matters

A signet is **capped, permanent and unrecoverable**. One used at the lifetime cap must not be
consumed.

`UseInvItem` eats the item **after** `UseItem` returns — so a check inside `UseItem` would report
the refusal and the item would be gone anyway. The gate therefore sits in `UseInvItem`, immediately
beside the spell-book gate that exists for exactly the same reason and says so in its own comment:
*"refusing any later would eat the book and teach nothing."*

Two lines, in the right function. In the wrong one they would look identical and quietly eat
signets.

The cap is also printed on the item — `n of 20 used this life` — because a player who learns it by
being refused has learned it too late to plan around.

## One item, and still a generator

`tools/GenSignets.ps1` produces a single row, which looks like ceremony. It is not: the id, the
`AllItemsList` row, the ICURS id, the CEL frame size and the icon cut spec all have to agree, and a
CEL stores no names and no sizes — a frame's **position** in the file is the only thing tying it to
an id. Hand-adding one item to five places is how the sixth gets missed, and the failure is silent:
every icon after the mismatch is wrong.

It also makes the next signet-like item free.

The art is a **ring** silhouette, deliberately unlike the orbs' spheres and the jewels' lozenges in
the same 28x28 cell. At that size shape reads before colour.

Icon strip 485 -> 486 frames; `oracool.mpq` repacked.

## Tests

`TheSignetIsAUsableItemThatChampionsDrop` pins that it is genuinely usable (`iUsable` true and its
own `IMISC_ORACOOL_SIGNET`, or `UseItem`'s switch never reaches it), that it is mistaken for no other
family (the pool exclusion is one OR-chain of hand-written ranges), that it can be neither socketed
nor absorbed as a Mystic Orb, that an ordinary monster's bonus excludes it from the drop, and — the
load-bearing one — that a signet used at the cap grants nothing and is not counted.

## What to look at in game

Kill champions until one drops. Use it and check the stat point arrives and the log names the count.
Then spend to twenty and confirm the twenty-first is refused **without vanishing**.

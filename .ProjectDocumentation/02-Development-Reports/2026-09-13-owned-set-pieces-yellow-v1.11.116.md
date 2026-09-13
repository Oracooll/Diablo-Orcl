# Set pieces you own but are not wearing read yellow

2026-09-13 — v1.11.116

## Why

> "we need to add colour to set items which are in my possession, but not necessarily equipped. there
> is no such font colour now being used so it is hard for a person to tell if they own more items from a
> certain set. let's apply yellow font to set items owned by hero, but not equipped."

A set item's tooltip lists every piece of its set. Until now each line was one of two colours: green for a
piece being worn, red for everything else - so a piece waiting in the backpack or the stash read exactly
like one never found.

## What changed

The piece list now has three states, decided by one helper, `oracool::SetPieceListColor`
(`oracool/item_sets.{h,cpp}`):

| Piece | Colour |
|---|---|
| worn (and not broken) | green |
| owned, not worn - backpack, any of the nine inventory tabs, or the stash | **yellow** |
| not owned | red |

"Owned" is `IsSetPieceHeld`, the predicate the named-set drop bias already uses, so the tooltip and the
drop weighting agree about what a player has. Worn is tested first, since a worn piece is also held.

The set header "(n/m)" and the bonus ladder still count WORN pieces only - bonuses are for wearing, so
their green/red meaning is unchanged.

## Tests

`OracoolAudit.SetPieceListReadsGreenWornYellowOwnedRedMissing` — the same piece red when absent, yellow in
the backpack, yellow in the stash, green when worn.

## For the user to look at

Hover a set item while another piece of its set sits in the backpack or stash: that piece's line is yellow.

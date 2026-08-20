---
date: 2026-08-20
version: 1.8.68
tags: [runewords, sockets, ui, items]
---

# The First Runeword, and Seven Things It Exposed

The fork's first assembled runeword - Spirit, in a cloak - and the seven glitches the user found
looking at it.

## 1. The gold name is the title now

`PrintItemDetails` re-sets the panel title to `Runeword: Spirit` in Whitegold, and the separate gold
line lower down is gone.

Done in the panel rather than in `getName()` on purpose: the name is persisted in `_iIName` and also
feeds the ground label and the cursor, where "Runeword: Spirit" would be a mouthful. This is a
framing of the name for one panel, not a change to what the item is called. `SetPanelString` replaces
the title outright, and at that point the panel holds nothing else, so it cannot clobber anything.

## 2. The word's own bonuses

They were **applied and never printed**. `ApplySockets` has called `ApplyRunewordToTotals` for every
worn item since the system shipped, so Spirit's +8 all resists, +24 armor and +16 life were real -
but the panel listed the four runes' individual effects and stopped, so the word read as a name with
nothing behind it.

Now formatted from the definition's own fields, so a word whose numbers are retuned cannot describe
its old ones.

Pinned by `OracoolAudit.ACompletedRunewordAddsItsOwnBonusesOnTopOfItsRunes`, asserted as a **delta**
over the runes alone - the test cannot be satisfied by the runes doing the word's job.

## 3, 4 and 5. Backings and the hover overlay

Sockets now outrank quality in the inventory backing, and deliberately so: a runeword forms on
whatever base was to hand, so the interesting fact about a Spirit cloak is that it is a Spirit, not
that its base rolled Normal.

- **Runeword** - dark grey, opaque, green border (`PAL8_GREEN + 1`, the bright end of the mini-ramp
  this fork injected over `PAL8_ORANGE`).
- **Socketed** - the same grey, white border (`PAL16_GRAY`, the whitest index the shared palette
  has; there is no `PAL16_WHITE`).

Both states are derived - `GetActiveRuneword` reads the sockets, `_iSocketCount` is the item's own -
so the backing cannot disagree with the panel.

## 6 and 7. One overlay, not two

The three hover requests are one feature: **every socket gets a cell, showing either the stone in it
or an empty gold ring.** A completed runeword is simply the case where no ring is left, and a
partly-filled base is the case where both appear. New `oracool/socket_overlay.{h,cpp}`, drawn after
the sprite in both the backpack and the stash.

Rings are the user's numbers - 24px across, centred in each 28x28 cell - drawn by squared-distance
comparison rather than a midpoint walk, so the 2px thickness is exact at every angle instead of
thinning at the diagonals.

Two things worth recording:

**Hover-only, by design.** Painting four rune icons over every socketed item permanently would make
a full stash unreadable, and the count already reads from the backing and the ground label.

**The stones anchor at their BOTTOM-left.** That is the runeword book's own 1.8.60 bug - passing a
cell's top as if it were a centre floated every icon a cell too high. The overlay takes the cell
rect and anchors inside it.

## Verification

486 tests, the two standing baseline failures only.

**All of 1, 3, 4, 5, 6 and 7 are pixels, and nothing in the suite renders a panel or a grid.** They
need eyes: the gold title with no duplicate line beneath it; the two border colours; the rings
sitting centred and not clipped at a panel edge; and a partly-filled item showing stones and rings
together.

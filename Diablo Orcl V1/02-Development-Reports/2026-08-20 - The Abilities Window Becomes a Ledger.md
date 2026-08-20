# The Abilities window becomes a ledger

**Version:** 1.8.79
**Date:** 2026-08-20
**Part B** of [[Plan - The Skill Picker and the Points-Only Abilities Window]].

> "in abilities window we repurpose left/right clicks - left click ADDS point, right click
> SUBTRACTS. We remover the + and - symbols. Skills eligible for bump just lit brighter than the
> rest. We leave the skill level indicator where it is for now."

## The rebind

| | Before | Now |
|---|---|---|
| Left click on a tree cell | ready it on LMB | **invest one point** |
| Right click on a tree cell | ready it on RMB | **refund one point** |
| Left click, icon bottom-right | invest | (gone) |
| Left click, icon bottom-left | refund | (gone) |

`CheckSBook` already carried the button through as `assignToRightButton`, so the rebind is one
ternary, not new plumbing.

## Why the glyphs could go

The `+` and `-` were never really symbols. They were **two separate click targets carved out of the
icon**, and they existed only because the icon itself meant "ready this skill" - the two actions had
to be kept apart by geometry or a reach for one would trigger the other.

Left-invest / right-refund removes that collision, which makes the whole cell one target and leaves
nothing to carve. So `SpendPlusRect`, `SpendMinusRect`, `DrawSpendGlyph`, `SpendBoxHovered` and
`SpendBarThickness` are **deleted**, not merely unused - about 60 lines of hit-testing, hover
framing and bar drawing that only the old gesture needed. This is a consequence of the split, not a
tidy-up layered on top of it.

## Eligibility as a state, not a label

What remains to communicate is not "here is a button" but "this cell will take a point" - which is a
state, and states are drawn. Eligible cells get a one-pixel outline near the **light** end of the
gold ramp (`PAL16_YELLOW + 2`; the window's own frame sits at +9, so it reads as lit rather than as
chrome).

The predicate is unchanged - `CanInvestClassTreePoint`, the same call that used to decide whether to
draw the plus - so eligibility means exactly what it always meant. Only its rendering changed.

The rank counter stays on the icon's bottom edge, as asked.

## Also gone from this window

- **Readying a tree skill.** Selection is moving to the LMB/RMB pickers (Part A).
- **Lighting an aura.** `ToggleClassAura` had no other caller here; it moves to the picker with the
  actives. **Until Part A lands, auras cannot be lit.**
- **The Spells sheet's spend controls**, which had nothing to spend since Part C.

## Interim state - do not test this build alone

Part B and Part A are meant to ship together, and this commit is the half without the picker:

- Tree skills **cannot be readied**, and auras **cannot be lit**.
- Spells can still be readied - the Spells sheet's ready tail is deliberately left standing as a
  bridge, and Part A removes it.

Test after Part A, not now.

## Verification

Build clean. **489/491**, the two standing baseline failures. No asset touched, so no MPQ repack.
The visible half - the eligibility outline, the missing glyphs - is drawn over the world, so a
screenshot is the only real verification, and that comes after Part A.

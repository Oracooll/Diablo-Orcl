# Staff spells get their own category (v1.9.196)

User request, 2026-09-03: "lmb/rmb pop-ups should introduce STAFF SPELL category and display that
category when a staff with spell is equipped. staff spells to use legacy orange backing. staff
spells and learned spells should not overlap in one icon as they do now. they need to coexist with
respective backing color."

## What was wrong

The picker asked one question — "does the player know this spell?" — by OR-ing all three stores
together (`_pMemSpells | _pISpells | _pAblSpells`), produced **one cell** per spell, and then typed
that cell by precedence: innate, then charges, then memorised. So a Sorceress who had read Fire
Ball and then picked up a Staff of Fire Ball lost the ability to bind the learned one at all. The
single cell bound to the staff, and when the staff was unequipped the binding went with it.

## What it does now

`EntryKind::Staff` is a fourth kind of cell, and the list has a third section.

| Section | Contents | Plate |
|---|---|---|
| Skills | attacks and class-tree rows | green |
| Spells | `_pMemSpells` and `_pAblSpells`, minus rows already listed above | blue (or the skill ramp) |
| Staff spells | every spell in `_pISpells`, undeduplicated | **orange** — the engine's own charge ramp |

- The staff section is built from the equipped staff's spells alone, so it is **empty and collapses
  to nothing** unless a staff with a spell is equipped. `SectionHeight` already returned zero for an
  empty section, so no layout work was needed for the "display it when equipped" half.
- The staff section is deliberately **not** filtered against the sections above it. A spell held
  both ways is two cells, on purpose.
- Clicking a staff cell binds `SpellType::Charges`; clicking the learned cell binds Spell or Skill.
  That is the whole of "coexist with respective backing color" — the colour is not decoration, it
  is which store the click will spend.

## The well had the same disagreement

`DrawSpellWell` reached the tree-art path before it ever looked at the readied type, so a staff
spell that also has a tree row — which, since the Sorceress's book rows came back yesterday, is
most of her arsenal — was drawn on the green skill plate. The well and the cell that bound it
disagreed about where the cast comes from. A readied `SpellType::Charges` now short-circuits to the
orange plate and the engine's own icon.

## Test

`OracoolSkillPicker.AStaffSpellAndALearnedSpellAreTwoDifferentHoldings` pins the rule the split
rests on: the three stores are separate, and a spell held two ways answers yes twice.

624/625, the standing baseline.

## To look at in play

Equip a staff with a spell and open either pop-up: a third heading, "Staff spells", with orange
plates. Read a book for a spell you also have on a staff and confirm two cells, one blue and one
orange, each bindable. Unequip the staff and the section disappears.

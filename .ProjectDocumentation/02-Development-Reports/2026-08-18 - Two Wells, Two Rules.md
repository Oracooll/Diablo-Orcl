# Two Wells, Two Rules

**Version:** 1.7.95
**Date:** 2026-08-18
**Tests:** 445/447 (the two standing baseline failures)

## "Nothing lands on the LMB - left click lands everything on RMB"

It landed correctly the whole time. `DrawWellIcon` is shared by both wells, and its Paladin-skill
branch asked for `GetRmbSkillWellNetRect()` **unconditionally** - so a skill readied on the LEFT
button was painted into the RIGHT button's hole. Two wells that looked like one well ignoring the
left mouse button.

`DrawWellIcon` now takes its own well's net rect as a parameter; `DrawLmbSkillWell` passes the LMB
rect, `DrawRmbSkillWell` the RMB one. The engine-sheet fallback centres itself in that rect too,
rather than taking a separately-computed origin that could disagree with it.

## "Auras don't land anywhere"

They lit, they logged "burns", they fed the totals - and the HUD said nothing, because an aura has no
SpellID and therefore can never arrive at a well as `_pRSpell`.

A lit aura now takes the **RMB well's face** when nothing is readied on that button, drawn from its
own tree icon through the new `DrawClassTreeSkillInWell`. When a skill *is* readied there, the skill
keeps the face and the aura keeps its corner badge - that order is the lesson of v1.7.91, where the
aura took the well unconditionally and hid every later assignment.

Clicking a burning aura now puts it out. It used to be lit-only, so the same row could not undo what
it had just done.

## "Reg attack and Fist don't use the 46x46px size"

The attack strip is cut at 38px and was centred in the 46px opening, leaving a moat of plate.
`DrawAttackIconScaledTo` scales plate and icon to fill the net rect, same treatment the tree icons
got in 1.7.94.

## Per-sheet click rules, written down as rules

Stated in full at the click handler rather than left to be inferred, and deliberately not unified
(user: "each sheet to have its own set of rules"):

- **Aura rows** - either button lights it; it reports on RMB; clicking it again puts it out.
- **Active rows** - the button that clicked is the button it lands on. Left readies LMB, right
  readies RMB.
- **Passive rows** - never reach the assignment path; the spend corners are all they have.
- **Spells sheet** - unchanged, per-button.

## Files

- `Source/oracool/attack_skills.cpp` - `DrawWellIcon` takes a net rect; the aura takes the empty RMB
  face
- `Source/oracool/hud_art.h` / `.cpp` - `DrawAttackIconScaledTo`, `DrawClassTreeSkillInWell`
- `Source/panels/spell_book.cpp` - the rules, and the aura toggling off

## To look at in game

1. Left-click a combat skill in a tree page - it appears in the LEFT well. Right-click one - the
   right well. They no longer fight over the same hole.
2. Light an aura: it fills the RMB well. Ready a skill on RMB: the skill takes the face, the aura
   drops to the corner badge. Click the burning aura again: it goes out.
3. Regular/Fist Attack fills its well corner to corner without touching the bezel.

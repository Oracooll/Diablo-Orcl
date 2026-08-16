---
date: 2026-08-16
version: 1.7.40
tags: [abilities-window, class-tree, paladin, save-format]
---

# Two Sheets Out, Two Skills Home

> "find a suitable place for Hammer of Faith and Blessed Shield in the 3 dedicated sheets of paladin
> skills and get rid of skills sheet + get rid of class skills. V1 will not need those, they are
> useless."

The Abilities window is four sheets now: **Spells**, and the class's three tree pages.

## Where the two skills went

Both are Combat Skills, so both went on page 0, into gaps their tiers already had:

| | Tier | Level | Column | Was |
|---|---|---|---|---|
| Hammer of Faith | 2 | 12 | 1 (beside Vengeance) | level 10 |
| Blessed Shield | 3 | 18 | 0 (beside Blessed Hammer) | level 20 |

Each moved to the **nearest** tier level, so neither travelled further than it had to — 10→12 rather
than 10→6, and 20→18 rather than 20→24. That matters because the tier now carries the gate:
`IsClassTreeSkillUnlocked` defers to `paladin_skills.cpp`, so a tier and a skill that disagree would
reintroduce yesterday's bug.

All seven Paladin skills are borrowed tree rows now, and the regression test's count guard went 5 → 7.

## Where they could NOT go

Appending them beside Vengeance in the enum — the obvious edit — would have been silently
destructive. A Paladin skill's **position in its class block is three things at once**: its icon
frame in `paladin_tree_icons.png`, its slot in `Player::_pClassTreeInvestment`, and (for the aura
byte) its saved identity. Inserting two rows in the middle would have shifted all twenty auras down
two frames and two save slots, quietly reassigning points a live character had already paid for.

So they were appended at the **end** of the Paladin block instead, out of page order, with the
`page` field doing the work of putting them on the Combat Skills sheet. Their table position is
unusual and has to be.

That still shifts every class after the Paladin by two enum values, which matters for exactly one
field: `_pOracoolActiveAura` stores a raw skill value. Paladin values 0–28 don't move, and the user
has confirmed they have no other hero, so nothing live is affected — but a pre-1.7.40 **non-Paladin**
hero with an aura lit would decode a different aura.

## The icons

The strip had 29 frames for 29 skills — no spares. It's 31 now. The two new frames are the Skills
sheet's own art for these skills, taken from `paladin_skill_icons.png` and desaturated, because that
strip's line art carries a green key-fringe the tree's monochrome icons don't, and the two styles
fight side by side on one page.

They are drawn at their native 38px centred on the 56px cell rather than upscaled: 38→56 is not an
integer factor, and nearest-neighbour on 1px line art breaks the lines. So they read slightly smaller
than their neighbours. **They are placeholders** — 56px versions dropped in the MPQ root will replace
them without a code change.

## What the sheets took with them

**SKILLS** held the two basic attacks and all seven Paladin skills, five of which the Combat Skills
page already carried — same slot, same investment, drawn twice.

It also held something not obvious from looking at it: **its two attack rows were the only way back
to a plain left click.** Clicking either cleared `_pLRSpell`. The right button has had
shift-click-to-clear on its HUD well since the HUD overhaul; the left never did, because it never
needed one. Deleting the sheet without noticing would have made a left-button assignment permanent.

So the LMB well now takes shift-click to clear, exactly as the RMB well does. That is the one piece
of this change that is an addition rather than a removal, and it exists purely to keep a capability
the deletion would otherwise have taken away.

**CLASS SKILLS** listed Item Repair, Trap Disarm, Staff Recharge, Search, Identify and Rage. Only the
sheet is gone — every class still has all six in `_pAblSpells`, and the speedbook still lists them,
because `oracool/class_skills.h` is read by `items.cpp` and `player.cpp`, not by the window. If the
intent was to remove the abilities themselves, that is a separate change and this is not it.

Removed with them: `GetClassSkill`, `BuildClassSkillRows`, `BuildSkillRows`, `AttackRowCount`,
`SkillRowKind`, `SkillRow`, `MaxSkillSheetRows`, `BuildSkillsSheetRows`, `DrawAttackRow`,
`DrawPaladinSkillRow`, and the mixed-height walks in the draw, hover and click paths. `spell_book.cpp`
lost four includes and about 250 lines.

## Save format

`_pClassTreeInvestment` grew 30 → 32 with `MaxSkillsPerClass`, because the Paladin now has 31 skills
and that constant bounds the search that finds them.

The hero blob is 2 bytes longer, and `Writehero.pfile_write_hero`'s golden hash was re-baselined for
it — entry 9 in that test's numbered history. This is a **tail** change, not a `PlayerPack` one: the
fixed struct is byte-identical, so `ReadHero`'s exact-size check on the base is unaffected, and
`ApplyClassTree` clamps to the smaller of the chunk's own count byte and the array. Pre-1.7.40 heroes
load with their 30 entries in the first 30 slots and the rest zeroed.

## Shipped

`ORACOOL_VERSION` 1.7.40. MPQ repacked (79 files) for the extended icon strip. 423 tests, the two
long-standing failures unchanged.

## Worth noting

Points already sunk into a tree row are addressed by position, and position is also art and also
save state. Three meanings on one number is why "just add two rows in the obvious place" was the
wrong move, and why the right one looks wrong in the table.

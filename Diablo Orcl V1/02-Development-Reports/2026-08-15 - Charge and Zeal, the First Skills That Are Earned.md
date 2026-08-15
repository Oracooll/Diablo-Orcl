---
date: 2026-08-15
version: 1.5.47
area: Paladin skills - gameplay gates, mana costs, Abilities window
---

# Charge and Zeal, the First Skills That Are Earned

> Let's reintroduce Furious Charge and Splash damage skills to Paladin. We will rename Furious Charge
> to Charge only. We will rename Splash damage to Zeal. [...] Make Charge require lvl 12. Make Zeal
> require lvl 6. Make Charge require 10 mana per hit. Make Zeal require 2 mana per hit.

Two mechanics that were written months ago and then switched off on 2026-08-11, with a note saying
they would come back as things a player *earns* rather than things an INI toggles. This is that
return, and it is the first time anything in this build unlocks by level and charges mana outside the
vanilla spell system.

## Where the numbers live

Neither mechanic keeps its own copy. `Source/oracool/paladin_skills.h` is a small table - name,
description, minimum level, mana cost - and both the Abilities window's row and the code that spends
the mana read it:

| | Level | Mana | Charged |
|---|---|---|---|
| Charge | 12 | 10 | per launch |
| Zeal | 6 | 2 | per splashing hit |

That split is the design in one line: Zeal is the cheap thing you lean on constantly, Charge the
expensive one you open with. The costs are whole points in the table and shifted into the engine's
1/64 fixed point in one place (`ManaFixedPointShift`), so the table still reads as the numbers the
user gave rather than as 640 and 128.

`SpendPaladinSkillMana` re-checks affordability itself rather than trusting callers, and moves both
`_pMana` and `_pManaBase` exactly as `CastSpell` does - moving only the first would have the
difference reappear the next time anything recalculated the character's stats.

## Charge

The gate that read `return false;` now reads the level table. Mana is spent only when the dash
actually launches, and the `&&` short-circuits so a Charge refused by its cooldown costs nothing -
which preserves the rule the original code already stated: out of cooldown, or now out of mana, the
ability still swings. It just arrives at walking pace instead of rushing.

The name comes from the table too, so the readied-spell caption and the Abilities row cannot disagree
about whether it is "Charge" or "Furious Charge".

## Zeal

Two changes beyond the gate.

**The five-target cap is new.** Range 1 is eight surrounding tiles, so a swing in a crowd could
already carry to eight - and the description the user wrote says *five*. Rather than reword it, the
mechanic now matches: targets are collected ring by ring, nearest first, and stop at five. Collecting
nearest-first matters only if the range ever grows past 1, but that is exactly when it would matter,
so it is worth having now.

**Targets are gathered before any damage is applied.** Two reasons, and the second is a real bug
avoided: the mana must be charged only if the swing actually carries to someone, and killing a
monster mid-scan can reorder `ActiveMonsters` underneath the loop.

Zeal is a passive - it applies itself to every melee swing and there is nothing to ready. That is a
decision worth flagging rather than burying: the alternative reading of "2 mana per hit" is a
toggleable skill you put on a mouse button, which would need a new `SpellID` and everything keyed on
`MAX_SPELLS`. The mana cost self-limits it instead, which is what makes the passive reading work at
all - run dry and the swing is simply a normal swing.

The file is still called `warrior_splash.cpp`. Renaming it to `zeal.cpp` would hide that the class
underneath really is `HeroClass::Warrior`; the header says what it is now.

## The Skills sheet grew a second kind of row

Every sheet until now had uniform rows, so drawing, scrolling and hit-testing could each do
`index * rowHeight` independently. Charge and Zeal need the tall described row - icon, name, wrapped
description, and a "Requires level N" line when locked - beside the compact spell rows that were
already there.

Three copies of that arithmetic would be three chances to disagree about which row the cursor is on,
so the sheet is now enumerated once (`BuildSkillsSheetRows`) and all three walk the same list,
accumulating heights. That also retires the `rowIndex -= AttackRowCount` shift the draw and click
paths each used to do by hand.

Row behaviour:

- **Charge is not a row of its own.** It IS the Paladin's class skill (`SpellID::ItemRepair`), so
  once earned the compact spell row is dropped and the described one stands in its place. Below level
  12 the slot really is Item Repair and keeps its row, with a locked Charge listed underneath.
- Both are listed whether or not the gate has opened - "Requires level 12" is the useful thing to
  know at level 4.
- A locked Charge row is inert. Readying it would arm the slot's Item Repair under a name the player
  has not earned.
- Zeal's row is inert always, the same way Fist Attack's is, and for the same reason: there is no
  separate state to select.

The mana price goes in the tag slot - the right-aligned field the Barbarian sheet established for
"the thing to know at a glance". With two entries a category would say nothing; "10 mana" is what
decides whether you lean on it.

## Art

`tools/CutPaladinSkills.ps1`, following `CutPaladinAuras.ps1`. The two delivered images key by FLOOD
FILL FROM THE BORDER rather than by a colour threshold - both have bright gold centres and Zeal's
sword arc is nearly white, so a threshold wide enough to catch the key's anti-aliased fringe would
punch holes through the artwork. Cut square (774x770 and 773x770, ratio asserted) and scaled to the
38x38 the other three strips use.

Written to `oracool_assets` and the build tree only - deliberately NOT to
`Packaging/resources/assets/ui`, which is what the aura and Barbarian cutters do and what left the
duplicate dead copies removed earlier today. The sources moved from the vault root into
`02-source-art/paladin-skills/`.

## Changed

- `tools/CutPaladinSkills.ps1` - new. `ui\paladin_skill_icons.png` (76x38).
- `Source/oracool/paladin_skills.{h,cpp}` - new. The table, the gates, the mana.
- `Source/oracool/furious_charge.cpp` - gate reopened at level 12; name from the table.
- `Source/oracool/warrior_splash.{h,cpp}` - gate reopened at level 6; 5-target cap; mana per hit;
  targets gathered before damage.
- `Source/oracool/hud_art.{h,cpp}` - the Paladin icon strip.
- `Source/panels/spell_book.cpp` - single row enumeration, mixed row heights, the described rows.
- `Source/player.cpp` - Charge spends its mana at the launch site.
- `Source/CMakeLists.txt` - the new module.

## Verified

- Build clean. **352/354** - the standing baseline
  (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`), no regression.
- oracool.mpq repacked, 73 files, 17.86 MB.
- 1.5.47 confirmed baked into `DiabloOrcl.exe`.
- Icon strip inspected magnified against a checkerboard: both frames intact, background transparent.
- **Not seen in play.** No part of this has been confirmed on screen - not the rows, not the level
  gates, not the mana drain, not the five-target cap.

## Carried forward

The Skills sheet can now carry described rows, which is most of what the auras and Barbarian skills
would need to become real. What they still lack is somewhere to record a choice - `auras.h` still
points at an `ActiveAura` on `Player` that does not exist. Charge and Zeal sidestepped that by not
needing it: one rides a spell slot, the other is passive.

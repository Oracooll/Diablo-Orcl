# 2026-09-29 - Shield skills go red without a shield (v1.12.221)

**Date:** 2026-09-29. Debug only. The user: "Shield requiring skills to have red backing in lmb/rmb slots and menus when a shield is not equipped."

## Before

Three skills need a shield: Smite (Shield Bash) and Blessed Shield (`PaladinSkillData::requiresShield`), and Aegis Slam (`Rfa12MeleeUsable`).
- **LMB and RMB wells:** Smite and Blessed Shield already went red there without a shield, through `CanUsePaladinSkill`. Aegis Slam took the generic path, which asks only about mana and town, and stayed gold.
- **LMB/RMB skill menus** (`skill_picker.cpp`): every cell was gold, or grey where the button cannot take it.

## Change

- **The check:** `LacksShieldFor(player, spell)` (`paladin_skills.h`) answers for all three.
- **LMB well** (`attack_skills.cpp`): marks the skill unusable, so its plate is red.
- **RMB well** (`spell_list.cpp`): sets `SkillPlateTint::Blocked`.
- **Skill menus** (`skill_picker.cpp`): draw such a cell red; the grey "not for this button" still wins.

## Test

`OracoolAudit.ShieldSkillsGoRedWithoutAShield` checks:
- the three answer true with no shield;
- Zeal, Holy Bolt and Votive Strike never answer true;
- the three answer false once a usable shield is in hand.

The test sets `gbIsHellfire`, since `IsValidSpell` admits the fork's spells only then.

Debug build and ctest: 879/879 passed. Not seen in play yet.

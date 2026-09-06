# The Paladin's Holy Bolt row removed (v1.9.301)

**Date:** 2026-09-06
**Request:** "remove paladin Holy Bolt skill. There is a spell like this already in the game."

## What went

- The Combat Skills row (class_tree.cpp, tier 0 column 2 - the column is empty now) and `ClassTreeSkill::HolyBolt`; `ClassTreeSkillCount` 273 -> 272.
- Its two sound rows (skill_sounds_data.inc) and the two wavs under `sfx\skills\paladin\combat-skills\`.
- `SpellID::HolyBoltSkill` stays in spelldat, unreferenced: removing a SpellID shifts the readied-slot values every hero stores, for no gain.

## What moved

`ClassTreeIconIndex` is a skill's position within its class, and is both its frame in the class icon strip and its slot in `_pClassTreeInvestment`. Every Paladin row after Holy Bolt dropped by one - a deliberate save-slot change, accepted because V1 is always New Game. Three guard tests pin those ordinals and were moved with them, with the reason in each. `tools\BuildGlyphStrips.ps1` re-stamped `paladin_tree_icons.png` by name (its report lists the pack's Holy Bolt glyph as matching no row, as expected); MPQ repacked.

Suite 635/636, the standing dungeon-generation failure only.

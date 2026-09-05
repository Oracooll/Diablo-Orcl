# Skill Facts, What the Engine Rolls (v1.9.260)

**Date:** 2026-09-05 · **Request:** "they are missing essential information about a skill's effect like SPEED/RANGE/DMG/ETC.... all the stuff that happen in the back of the engine during triggering - let them be known."

## The rule

Every module that runs a skill now answers "what does it do at rank N" as lines, written BESIDE its own formula, so a fact and the code that rolls it cannot drift. `oracool/skill_facts.cpp` only routes a SpellID to the right module:

| Module | Function | What it quotes |
|---|---|---|
| melee_skills.cpp | `MeleeSkillFactsAt` | damage bonus, strikes in one swing and their share, stun, knockback, sweep share, lightning, charged bolts, leap range, Sacrifice's life cost |
| warcries.cpp | `WarcryFactsAt` | radius, duration, armour / life / resistance magnitudes, enemy debuffs, stun, sleep, damage rolls, find chances, Conversion's reach and hold |
| paladin_skills.cpp (+ paladin_melee, paladin_ranged, furious_charge) | `PaladinSkillFactsAt` | range in tiles, the shield gate, Zeal's strikes and to-hit at that rank, Smite's stun, Hammer of Faith's splash, Fist / Blessed Shield / Blessed Hammer percentages, Charge's dash and cooldown |
| rogue_arrows.cpp | `RogueArrowFactsAt` | elemental bonus, arrows loosed by Multiple Shot and Strafe, Guided's "cannot miss", chill and freeze seconds |
| cold.cpp | `ColdSpellFactsAt` | freeze / chill seconds per spell, the armours' duration and strike-back chill |

The tree block (`ClassTreeEffectLine`) appends the facts under an active's Damage / Mana Cost lines for the current rank and again under Next Level, so the two blocks read line for line. The Spells sheet (`BuildSpellStatBlock`) appends them too, which reaches the cold spells and the arrows when they are hovered as spells.

What is still silent: passives' and auras' facts were already the run effect (DescribeBonusTotals); attack SPEED has no per-rank formula anywhere in the engine except Zeal's per-strike ticks, which depend on the weapon and are not quoted. A spell no module describes prints nothing rather than a guess.

## Verification

Debug build clean. 625/626: the standing `Drlg_l1` failure only. New audit test `TheSkillFactsQuoteWhatTheModulesRoll` pins Zeal's ladder (2 strikes at rank 1, 4 at rank 5), Sacrifice's +150% / +170%, Shout's 40 s / 50 s, Multiple Shot's 2 / 3 arrows, an armour's duration, and that Firebolt says nothing. Hover Zeal, Whirlwind, Shout, Multiple Shot and Frozen Armor in the game.

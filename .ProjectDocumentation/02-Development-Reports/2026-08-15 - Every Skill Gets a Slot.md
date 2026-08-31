---
date: 2026-08-15
version: 1.5.69
area: Paladin skills / spell ids and icons
---

# Every Skill Gets a Slot

> Skills sheet has problems. I cant select last 5 skills, and about skill number 3 - i can select it
> but the white icon does not appear on LMB/RMB. Make sure all skills are selectible and their icons
> appear as they should on LMB/RMB

Two symptoms, one root cause each, and they are not the same cause.

## Why the last five could not be selected

A row is assignable to a mouse button exactly when it carries a `SpellID`. That is not an arbitrary
rule — the readied pair that the HUD's wells draw, that `CheckPlrSpell` casts, and that the save
format now persists **is** a `SpellID`. A skill without one can be listed, described and hovered, but
there is no value to put on the button.

Only Charge had one. The other six now do too: `SpellID::Zeal`, `HammerOfFaith`, `BlessedShield`,
`FistOfTheHeavens`, `ShieldBash`, `BlessedHammer`, appended last so no existing enum value moves.

The slot lives in `PaladinSkillData` beside the level gate and the mana price, so the sheet, the
rings and the click handler all read it from one table and none of them names a skill.

### The save arithmetic, again

Same as Charge's and worth re-stating because it looks like it should cost a save break: `PlayerPack`
persists spell *levels* only for ids 0–46, so these join the runes at 47–51 in not persisting one.
What does persist is `_pMemSpells` / `_pAblSpells`, both `uint64`, and bit 57 is still comfortably
inside. `MAX_ITEM_SPELLS` stays pinned at 52, so item generation draws from exactly the same pool and
the `PackTest` fixtures — which encode a seed's expected book — pass untouched.

## Why Charge showed a blank square

Because it *was* a `SpellID`, and everything that draws a readied spell reached for the engine's
`spelli2` sheet, where it has no frame. `SpellITbl` pointed it at frame 26 — the empty plate. So the
well drew, correctly, nothing.

Its art was never in that sheet. It is in `ui\paladin_skill_icons.png`, drawn through a completely
separate path. `oracool::TryDrawSkillSpellIcon` is the bridge: given a `SpellID`, it asks
`PaladinSkillForSpell` and draws the strip icon if there is one, and returns false otherwise so the
caller falls back to `DrawSmallSpellIcon`. The wells ask it first now.

The `SpellITbl` entries stay at frame 26 rather than borrowing a lookalike frame, deliberately: they
are what shows if a *new* draw site forgets to ask. A bare plate is a visible "nobody wired this up";
another spell's symbol is a lie that survives review. That is the failure mode that retired the old
Heal Other stand-in.

## Casting a skill that has no mechanics

Giving the five a `SpellID` created a hazard that did not exist while they were inert. `CastSpell`
walks `sMissiles`, finds `MissileID::Null` in both slots, spawns nothing — and then calls
`ConsumeSpell`. The button would have charged mana for no effect.

So `CheckPlrSpell` intercepts every Paladin skill, not just Charge. Charge keeps its dash; the other
six swing at the target. That follows the rule Charge's own comment already set two blocks above it:
**the ability never "does nothing", and it never charges for what it did not do.**

## A latent bug found on the way

`oracool/spell_descriptions.cpp` guarded its table with:

```cpp
static_assert(Descriptions.size() == static_cast<size_t>(SpellID::LAST) + 1, ...)
```

The array's size *came from* `SpellID::LAST`. The assert was comparing a value to itself — it could
never fail, no matter what was missing. And something was already missing: Charge had been added to
the enum months of work ago with no line here, and the short initialiser list quietly
value-initialised its slot to `nullptr`. Nothing read it yet, so nothing broke, but
`GetSpellDescription(SpellID::Charge)` was returning a null pointer.

Replaced with a `constexpr` scan for a null entry, which is the check that was intended. The seven
skills are excluded from the array entirely and answered from the Paladin table instead, so their
one sentence is not maintained in two places.

## State

352/354, the standing baseline. `Writehero` and the `Pack` fixtures pass untouched, which is the
evidence that the pool pinning worked.

Not tested in-game — the user runs the game.

# Bands, Rangs and a Retirement

**Version:** 1.7.97
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

## 1 — the six vanilla class skills are retired

A new Paladin spawned with Item Repair on the right button because `playerData.skill` was still being
readied at creation, and `InnateSpellsBitmask` still granted all six vanilla class skills to every
class. Both are gone:

- `InnateSpellsBitmask` grants only the level-gated Paladin skills now.
- `CreatePlayer` readies nothing on either button. The Sorcerer still starts with Firebolt, which is
  a book spell, not a class skill.
- `InitPlayer` clears a button still pointing at a skill the character no longer owns, so an existing
  save loses it on the next load rather than keeping a dead icon.

`AllClassSkillsBitmask` stays exported for the debug "give me everything" command - a different
question from what a character owns. Repair, identify, recharge and disarm are town services.

## 2, 3 — the wells' hover text

The LMB hover reported the basic attack unconditionally, from when the left button could hold nothing
else. It now names what is actually on the button, with the spell level for a spell.

The RMB hover never mentioned a burning aura, because an aura carries no SpellID and every branch
there was keyed on one. It is checked first now, since a lit aura clears any readied spell.

## 4, 5, 6 — the Rule of Rangs

New module `oracool/spell_ranks`, holding two rules that compose:

**Bands.** Every book spell sits at level 6, 12, 18, 24 or 30 - the same shape the class trees use.
Derived from each spell's own `sBookLvl` (the depth its book drops at, which is the game's own
statement of how advanced it is) and then written out explicitly, so retuning one spell is a one-line
edit. The Spells sheet sorts by band, then alphabetically inside it, and an unlearned spell below its
band says "Requires level N" instead of only "Not learned".

**The Rule of Rangs.** Rank N of anything wants `band + (N - 1)` character levels. A level-6 spell
reaches rank 10 at character level 15; a tier-30 tree skill reaches rank 10 at 39. Applied at both
investment gates - `CanInvestSkillPoint` and `CanInvestClassTreePoint` - and to books, where the rank
being bought is the book level it would produce.

**No ceiling.** `MaxSkillInvestment`, `MaxTreeInvestment` and `MaxSpellLevel` all become 98, which is
exactly the pool a character can earn (one point per level from 2 to 99). D2's flat 20 is gone; what
paces depth now is the Rule of Rangs, not a number.

Books refuse to be read past what the reader's level allows, and the refusal happens in `UseInvItem`
BEFORE the book is consumed - `UseItem` runs first and the caller destroys the book afterwards, so a
guard inside `UseItem` would have eaten it and taught nothing. There is a backstop there anyway.

## 7 — F12, the other half

Last version dropped auto-repeat, which was a real double but not this one. The keymapper's
Screenshot action is registered with a **null** `actionPressed` and `CaptureScreen` as its
`actionReleased`, so an ini that still binds it to F12 fired on the key going UP even though
`PressKey` had intercepted the press. `ReleaseKey` now swallows F1-F12 outright - they are reserved,
so nothing else is entitled to either edge of them.

## Files

- `Source/oracool/spell_ranks.h` / `.cpp` (new), `Source/CMakeLists.txt`
- `Source/oracool/class_skills.cpp`, `Source/player.cpp` - the retirement
- `Source/control.cpp` - both hover texts
- `Source/oracool/skill_points.h` / `.cpp`, `Source/oracool/class_tree.h` / `.cpp`, `Source/player.h`
  - the caps and the rank gates
- `Source/inv.cpp`, `Source/items.cpp` - the book gate
- `Source/panels/spell_book.cpp` - band sort, requirement line
- `Source/diablo.cpp` - the release-side screenshot
- `test/oracool_audit_test.cpp`, `test/player_test.cpp`, `test/writehero_test.cpp` - updated to the
  new rules, plus new coverage of the rank gate and the per-rank refund

## Two things left deliberately

- **Mana Shield still caps at spell level 7** (`GetManaShieldDamageReduction`): the formula is
  `24 - level * 3`, so level 8 would be zero damage taken. That cap is a balance decision, not a
  ceiling I removed by accident.
- **Book DROPS are ungated.** A level-5 character can still find an Apocalypse book; they simply
  cannot read it yet. Gating the drop too would mean loot that never appears rather than loot you
  grow into - say the word if the other reading was meant.

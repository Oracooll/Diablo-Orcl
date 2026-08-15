---
date: 2026-08-15
version: 1.5.48
area: Class skills, Abilities window sheets, spell data audit
---

# Six Class Skills for Everyone, and Three Spells That Do Nothing

First slice of a larger batch (class skills, book availability, hover descriptions, hover outline,
common icon backgrounds). This one lands the class-skill restructuring; the rest is still open, and
two findings below change what the rest should be.

## All six, to everyone

Vanilla gives each class exactly one innate skill and the Abilities window listed whichever one you
had. Now every class has all six, on a **Class Skills** sheet of its own.

The change is two lines, because of where the mask is built:

```cpp
player._pAblSpells = oracool::AllClassSkillsBitmask();   // was GetSpellBitmask(playerData.skill)
```

in both `CreatePlayer` and `InitPlayer`. The second is what makes this need **no save migration** -
`InitPlayer` rewrites `_pAblSpells` from scratch on every load, so a character made before today
picks up the other five simply by being loaded once.

The class's own skill is still the one READIED at creation, so a new character still starts holding
the thing that identifies it.

**Search stops being a duplicate.** It was the one class skill that also sat in the book's page
table, so a Monk saw it on two sheets and every other class saw a row they could never learn (it has
no book). It is a Class Skill now like the other five, and its page slot went to Doom Serpents.

**Charge keeps its place.** `BuildClassSkillRows` omits Item Repair while it is rendering as the
Paladin's Charge, because that has its own described row with a level gate and a mana price a compact
row cannot show. Below level 12 the slot really is Item Repair and appears here normally.

## Two findings for the rest of the batch

**Spells have no descriptions. None.** There is no `sDesc` field, no description text, nothing -
the original game never had them and this fork never added them. The requested hover popup therefore
is not a UI job with existing content behind it: somebody has to write ~51 descriptions first. They
will be authored, not recovered.

**Three spells are inert stubs.** Of the four "unobtainable" spells the user asked to make
droppable, three carry `MissileID::Null` in BOTH missile slots:

| Spell | Missiles | Castable? |
|---|---|---|
| Doom Serpents | Null, Null | does nothing |
| Blood Ritual | Null, Null | does nothing |
| Invisibility | Null, Null | does nothing |
| Infravision | Infravision, Null | works |

The other eleven (Infravision, Etherealize, Resurrect, Mana, Magi, Jester, the five runes) all have
real missiles and would work as book spells today. Giving books to the three stubs would put items in
the drop pool that teach a spell which does nothing when cast - worth a decision rather than a silent
implementation, so they are held pending the user's call.

Books themselves are the easy part: only four generic "Book of " item rows exist (drop levels 2, 8,
14, 20) with `SpellID::Null`, and the spell is chosen at generation from anything with
`sBookLvl != -1`. So making a spell droppable is a pure data edit with no new items. One catch found
for later: `items.cpp:694` skips Resurrect and Heal Other in SINGLE-PLAYER, so Resurrect needs that
skip lifted before its book can drop at all.

## Changed

- `Source/oracool/class_skills.{h,cpp}` - new. The six, and their combined bitmask.
- `Source/player.cpp` - both grant sites.
- `Source/panels/spell_book.cpp` - the Class Skills sheet; `BuildSkillRows` emptied; Search out of
  the page table.
- `test/player_test.cpp`, `test/writehero_test.cpp` - the `_pAblSpells` assertion.

## Verified

- Build clean. **352/354** - the standing baseline, back after the two tests this legitimately
  broke were updated. Those two (`Player.CreatePlayer`, `Writehero.pfile_write_hero`) asserted the
  literal single-skill mask `134217728`; they create a ROGUE, which is why the literal was
  TrapDisarm's bit and not the Warrior's. Now `35184609067024`, the six combined.
- **Not seen in play.**

## Still open in this batch

Books for the twelve working bookless spells; ~51 authored spell descriptions; the hover description
popup; the gold hover outline; the common yellow icon background.

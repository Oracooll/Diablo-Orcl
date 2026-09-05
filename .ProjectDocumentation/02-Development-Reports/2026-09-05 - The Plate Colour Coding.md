# The Plate Colour Coding (v1.9.249)

**Date:** 2026-09-05 · **Request:** "lets make some changes: Light Grey - ready and yours (replaces green); vanilla's plate as painted (GOLD) - unlocked but no points spent (replaces RED); Red - locked, not learned, or otherwise off (replaces Dark Grey); vanilla's beige ramp - scroll-based spells, to have their own tier in the picker menu."

## The coding

`SkillPlateTint` is named by meaning now, and `ApplyPlateTint` holds the colour:

| Tint | Colour | Meaning |
|---|---|---|
| Ready | light grey (`SpellType::Invalid`'s pale ramp) | invested, slotted, or usable now |
| Unspent | GOLD - the plate as painted (`Skill`, the identity) | unlocked, nothing spent |
| Locked | red | not earned, not learned, off |
| Blocked | red | cannot be performed right now (the beige it was is a scroll's now) |
| Scroll | beige (`SpellType::Scroll`) | a spell cast from a scroll |
| Yellow | the plate as painted | no state applies |

Green → Ready, Red → Unspent, Grey → Locked, Pink → Blocked at every site (32 references), so the tree pages' legacy rows, the wells, the picker and the Abilities' state logic all read the new colours without a per-site change. The wells and DrawSpell colour an uncastable spell red instead of beige.

## The picker's Scrolls tier

`EntryKind::Scroll`: every spell in `_pScrlSpells`, its own section between Spells and Staff spells, beige, undeduplicated like the staff (a spell known, on a staff and on a scroll is three cells, each a different thing to ready). Binding one readies it as `SpellType::Scroll`. `BuildEntries` and `ContentHeight` grew a `scrolls` count; the click walk has four sections.

## Note for the glyph delivery

The tree pages draw no plate at present ("void of any backing"). When the class strips are rebuilt from the white glyphs on the vanilla plate, the plate returns with them and this coding is what it will wear.

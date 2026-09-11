# Blessed Hammer deals magic damage, and the sheet quotes it (v1.11.043-044)

**Date:** 2026-09-11
**Branch:** renderer-32bit (default), local commit

The user asked:

> in D2 it does magic dmg. Let's make it Magic DMG here as well. Also - change font color in hero stats screen to Magic DMG type (remind me what color that was). Also - make sure hero stats screen shows the DMG amount it does, because right now it shows a simple - (dash). Also, since we are making it do Magic dmg then when it is cast hero should play Magic spell animation, not Fire.

Then:

> let's make Magic GMD font color RGB:104,49,49

## Magic damage

The `BlessedHammer` missile row in `misdat.cpp` is `Magic` rather than `Physical`, and every hit reads the missile's own type (`ProcessBlessedHammer`). So monster magic resistance and immunity now apply to it, as in D2.

The amount is unchanged: 60% of a weapon roll per hit. The cast animation follows: `PaladinCastAnimation(BlessedHammer)` is `MagicType::Magic`.

## The sheet

The three cast skills carry `MissileID::Null` on their spell rows, because they are cast from their own module. That left the character sheet with nothing to ask:
- `GetDamageAmtAtLevel` returned -1, so the row read "-".
- `ReadiedSpellDamageType` fell back to physical white.

`oracool/paladin_ranged` now answers both:
- **`PaladinCastDamageRange(player, skill)`:** the per-hit range at both ends of the weapon. It uses the same arithmetic as the cast (weapon min/max, % bonus, flat bonus, the character's damage modifier, at least 1, then the skill's percentage). Fist of the Heavens quotes its centre blast.
- **`PaladinCastDamageType(skill)`:** the type of the missile doing the damage. That is the hammer's own missile, the shield's throw, and the blast under the fist (ApocalypseBoom, physical). Blessed Shield and Fist of the Heavens stay physical, as they were.

With a 10–20 weapon, Blessed Hammer's row reads **6–12**, in the magic colour.

## The colour

Magic damage was **blue** (`UiFlags::ColorBlue`) on the sheet. It is now **RGB 104,49,49**, the new `UiFlags::ColorMagicDamage` / `text_color::ColorMagicDamage`:
- It is a value colour in `RgbDefinedColors`, with no file. The glyph band's brightest shade is 0x683131 exactly, darkening down the gold ramp's luminance steps the way `DefineTextColorRgb` shades a value.
- `ColorTranslations` grew to 37, and `ColorTranslationsData` is now sized from it rather than a second literal 36.

Blue stays the aura row's colour and the "a bonus is present" colour of the plain value rows.

## Tests

`OracoolCharPanel.DamageFieldsAreColouredByDamageType` now expects `ColorMagicDamage` for Bone Spirit and Blessed Hammer. `EachMouseButtonReportsItsOwnDamageSource` adds Blessed Hammer reading 6–12.

The first run failed both tests: the fork's own SpellIDs sit above `LastDiablo` and are only valid with `gbIsHellfire`, which the game always runs with. The two tests now set it and restore it, as the Zeal tests already did.

`PaladinCastSkillsTakeTheirOwnSpellAnimation` expects magic for the hammer.

## Verification

Debug and Release built, ctest **708/708**, RTM refreshed with exe 1.11.044. **Not seen in play.**

**To check:**
- Blessed Hammer on a mouse button: the hero stats row shows its damage, not a dash, in the new dark red.
- It casts with the magic animation.
- A magic-immune monster takes nothing from it.
- Whether RGB 104,49,49 reads well on the limestone. It is dark on a mid-grey ground.

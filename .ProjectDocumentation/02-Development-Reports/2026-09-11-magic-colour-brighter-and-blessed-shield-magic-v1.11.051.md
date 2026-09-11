# v1.11.051 - magic damage reads brighter; Blessed Shield deals magic damage

2026-09-11. Two requests from the user, in one build.

## The colour

The user: "magic dmg font is way too dark make it brighter."

**Why it was dark.** v1.11.044 put the user's RGB 104,49,49 at the top of the 16-shade glyph ramp and stepped down from there. A glyph draws its body from the middle shades, so most of every letter came out between 0x4C2323 and 0x3E1D1D, near brown on the limestone. That was the risk noted in the v1.11.044 report.

**Now.** The same hue is used at twice the value, **RGB 208,98,98**:
- it is held flat across the top three shades, the way the dialog red holds its own;
- it then steps down the old ramp's ratios, so the letter bodies land around 0xB05353 to 0x984848.

It stays a softer, pinker red than the fire red. The only change is the `ColorMagicDamage` line in `text_render.cpp`'s `RgbDefinedColors`. The comments naming the colour in `ui_flags.hpp`, `text_render.hpp` and `charpanel.cpp` are updated too.

## Blessed Shield

The user: "make blessed shield Magic dmg type as well."

`MissileID::BlessedShieldThrow` is now `Magic` in `misdat.cpp`; it was `Physical`. Everything else reads that one field:
- the hit's resistances, so a magic-immune monster now takes nothing from it;
- the hero sheet's colour (`PaladinCastDamageType`).

It already cast with the magic animation (v1.11.040). The skill's tooltip now reads "Magic damage: 125% per target".

## Tests

`OracoolCharPanel.DamageFieldsAreColouredByDamageType` has a new Blessed Shield row expecting `ColorMagicDamage`, and its Bone Spirit label names the new RGB.

## Verification

Debug and Release built, ctest **710/710**, RTM refreshed with exe 1.11.051. **Not seen in play.**

**To check:**
- The magic damage row on the hero sheet (Blessed Hammer or Blessed Shield on a mouse button) is readable at a glance.
- Blessed Shield's row shows its damage in that colour.
- A magic-immune monster shrugs off Blessed Shield.

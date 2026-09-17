# The 32-bit renderer reaches the heroes

2026-09-17 — v1.12.022

## Why

> "why are you using ramps? We moved to 32bit renderer, right?"

Asked about yesterday's Barbarian dye, and the answer was uncomfortable. The screen has been 32-bit
since v1.11 and UI art, text and the frost tint all draw colour values - but `DrawPlayer` never joined
them. A hero sprite is palette indices; it went through the level palette and the index light tables,
so the dye could only land each garment on one of the palette's existing entries, and
`LoadPngSpriteSheet` squeezed any imported hero sheet into the 128 shared entries. Ramps are still how
a garment is SELECTED (the Warrior's CL2s are indexed; "240-255" is the only thing that says "mail").
They should never have been what limited the RESULT.

## What it is now

**`oracool/sprite_colours.{h,cpp}` - `SpriteColours`:** what each index of a sheet means.

- An index left alone is the level palette's entry through the level's light table - exactly what
  `ClxDrawLight` draws, so an untouched sheet is pixel-identical.
- An index given a colour of its own is that colour in full light, and in shadow is darkened by as much
  as the light table darkens its FALLBACK index (the nearest palette entry), channel by channel.
- Sixteen lit tables and an infravision one, cached per `PaletteRgbGeneration` like the frost tables.
- `DrawSpriteWithColours`: colour values on the 32-bit screen (`ClxDrawRgbMap`), fallback indices
  through the same light table on an 8-bit target - so offscreen work and golden tests still hold.

**The dye is colours, not baked indices** (`HeroColours` in `oracool/hero_look`). The sheet stays the
Warrior's own. The mail's sixteen greys are now sixteen distinct blues blended along the trouser ramp
(the index dye had to land two greys on each of eight entries); boots, belt and gloves are the
trousers' values exactly; the hair is a cool silver (0x8E949C / 0x5C626B) the palette does not have.
`HeroDyeTrn` survives as the fallback table.

**Imports keep their colours** - `LoadPngSpriteSheetColoured`: up to 255 of a sheet's own colours as
its own palette (folded by dropping low bits only if it has more), nearest shared entry kept as each
colour's fallback. `LoadPlrGFX` loads a class's PNG sheets through it; the class TRN, an index
translation, now applies to the CL2 path alone.

**`PlayerAnimationData::colours`**, set by `LoadPlrGFX`, cleared by `ResetPlayerGFX`. `DrawPlayer`
finds the colours of the sprite on screen by address (the sprite is not always `AnimInfo`'s -
`previewCelSprite` stands in) and draws its three cases - own hero unlit, infravision, lit - through
them. Scaling moves indices about and never changes one, so the 120% sheets need nothing.

## Not verified here

Not run in the game. The Barbarian in light armour should look as yesterday's build intended but with
smoother shading on the shirt and visibly silver hair, and should fall into shadow with the room
(walk another player's light, or the Nest/Crypt where the own hero is lit). Infravision shows him red
like everyone else. No class ships PNG hero sheets yet, so the true-colour importer has tests for its
colour tables and draw, not for a real sheet - the shield mix will be its first.

Tests: 794 pass (4 new): an untouched sheet equals the palette through the light table at every
level; an own colour is itself in full light, halves with its fallback, never brightens in deeper
shadow, and rebuilds on a palette change; the mail is sixteen distinct blues with the index dye as
fallback; a draw writes values on 32-bit and fallback indices on 8-bit.

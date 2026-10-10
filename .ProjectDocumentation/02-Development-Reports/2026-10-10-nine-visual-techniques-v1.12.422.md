# v1.12.422 - the nine visual techniques (2026-10-10)

The user approved all nine ("i like all 9"), to be built after the lighting/stutter audit loop came back clean (round 3, no P1/P2) and then audited until no major issues. Each is visual only, behind its own option (on by default), off on 8-bit surfaces (the golden tests) and, unless noted, in town. Every per-pixel loop is hand-written (no calls under /Ob0) in files compiled with `#pragma optimize("gt", on)`.

| # | Technique | Where | Option |
|---|---|---|---|
| 1 | Torch flicker: every flame's light wavers (three incommensurate sines, a phase per flame) through the colour pass | oracool/colour_light.cpp | Torch Flicker |
| 2 | Glowing spells: light-emitting missiles drawn additively with their usual colour table (`ClxDrawRgbMapAdditive`); borrowed sprites, arrows, acid, bone, curses never glow; not in town | engine/render/clx_render, oracool/spell_glow, scrollrt DrawMissilePrivate | Glowing Spells |
| 3 | Wall shadows (ambient occlusion): a half-tile plane of darkening at the foot of solid tiles, bilinear in the floor pass; built once a level | oracool/colour_light.cpp | Wall Shadows |
| 4 | Area mood + vignette: a subtle brightness-kept grade per level type; edges ~25% darker (vignette also in town) | oracool/post_fx | Area Colour Mood, Vignette |
| 5 | Bloom: quarter-res bright pass (soft knee from luma 176), two box blurs, added back at ~0.35; skipped when nothing is bright | oracool/post_fx | Bloom |
| 6 | Particles: embers from flames, sparks on melee/missile hits and explosions, dust motes in the hero's light, ash in Hell; own xorshift, never game RNG; pool of 1024; cleared on level change | oracool/particles (+ one-line hooks in player.cpp PlrHitMonst, monster.cpp MonsterAttackPlayer, missiles.cpp MonsterMHit and AddMissileExplosion) | Particles |
| 7 | Lava haze + water light: floor pieces classified once a level by drawing them into an 8-bit scratch surface and counting colour-cycling indices (lava if red-dominant); haze shifts pixels up to 1.5 px above lava, water brightness ripples +/-8% | oracool/post_fx | Lava Haze and Water Light |
| 8 | Ray-traced shadows: every light (and the baked flames) re-lit per tick with the vanilla linear falloff, a tile counting a light only if the grid ray (Amanatides-Woo) to its centre crosses no BlockLight tile; the floor pass reads the darker of dLight and this. Visual only - dLight untouched | oracool/colour_light.cpp | Ray-Traced Shadows |
| 9 | Sharp zoom filter: Scale2x/EPX (own implementation; xBRZ/hqx not used - GPL/LGPL) at exactly 2x, sharp bilinear between | oracool/post_fx, scrollrt ZoomScale | Sharp Zoom Filter |

Frame order in DrawGame: floor (bright if smooth) -> floor pass (smooth light x shadows x wall shadows) -> floor-pass objects -> ground rings -> tile content (missiles glow here) -> colour + flicker -> particles -> haze/water -> bloom -> mood + vignette -> monster outlines -> zoom (filtered).

Also in this build, the round-3 P3 fixes from the lighting audit: art-name hue lists corrected (unholy, ashen, storm_, the vanilla sheets magblos/miniltng/flare explosions), runes coloured by their kind, a sized unique's body checks the scaled Death sheet before its type's corpse slot.

Tests: oracool_colour_light_test (+ a fourth preview image with pillars: shadows and wall shadows), oracool_post_fx_test (8), oracool_spell_glow_test (additive kernel, glow rules, particle pool). Build Debug, ctest 987/987. Previews checked by eye; the in-game look is the user's to judge after the audit loop.

Next: the audit loop on these nine until no major issues.

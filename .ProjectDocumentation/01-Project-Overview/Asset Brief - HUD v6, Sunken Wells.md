# Asset Brief - HUD v6, sunken wells

**Date:** 2026-09-05 · One ChatGPT prompt. Attach the current HUD (the three shipped pieces reassembled - `orb_liquid_mock.png` from the session scratchpad, or a game screenshot) as the reference image.

What changes against the shipped fifth HUD (design 02 at 0.3168): the orbs and their gargoyle cradles get shorter, and the two skill wells become true openings - transparent, 56x56, with the frame casting a 3px shadow inward on every side so the well reads as sunken. The game will draw the vanilla 56x56 spell plate and its icon UNDER the plate, through the opening, so the frame's inward shadow falls on them; the net area the icon owns is the 50x50 inside that shadow.

On screen the HUD stays 613 wide; the belt and its six 28px cells do not move. The height comes down from 121 to 108.

```text
Redesign the bottom HUD in the attached reference - keep its style exactly: cold grey carved stone,
aged dark gold and blackened iron trim, gold pinstripe rims, the two gargoyles cradling the orbs.
Same layout, same belt; what changes is the height of the orb cradles and the two skill wells.

FORMAT
- One PNG, 1839 x 324 pixels (3x of the 613 x 108 it is shown at), TRUE 32-bit alpha. Transparent
  background, no painted checkerboard, no drop shadow outside the silhouette. Front-on,
  orthographic. Keep detail readable at one third: nothing thinner than 3 px at full size.
- Everything shares one flat bottom edge at y 324; the HUD sits flush on the screen bottom.
- No text or labels anywhere. No icons inside the wells or the belt cells - the game draws those.

LAYOUT (full-size px, left to right, butted with no gaps)
- HEALTH ORB CRADLE, x 0-336, full height. A glass sphere of radius 102 centred at (195, 156), in
  the same gargoyle-and-stone cradle as the reference but SHORTER: the cradle's top is y 0 and the
  sphere's crown must sit at or below y 48. The sphere's interior is EMPTY GLASS - transparent, with
  only a faint rim highlight - because the game pours the liquid in itself.
- PLATE, x 336-1497, y 39-324 (a 1161 x 285 band). The reference's plate: two square wells at its
  ends and the belt bar between them, the bar's top edge at y 201.
- LEFT WELL: a stone frame whose OPENING is exactly 168 x 168 at (366, 156). The opening is fully
  transparent. Around the opening's inside edge, the frame casts a shadow INWARD: a 9 px band on
  all four sides that goes from opaque dark stone-shadow at the frame to fully transparent at 9 px
  in, painted in the PNG's alpha so whatever the game draws beneath it is darkened by it. That
  leaves a clear 150 x 150 in the middle.
- BELT: six identical cells at x 561-1425, each 84 wide and 90 tall, at 126 px pitch (cell openings
  at x 561, 687, 813, 939, 1065, 1191; y 234-324), raised stone floors inside gold pinstripe rims -
  exactly as the reference.
- RIGHT WELL: the mirror of the left, its opening at (1320, 156).
- MANA ORB CRADLE, x 1497-1839, full height, the mirror of the health cradle: sphere radius 102
  centred at (1647, 159), crown at or below y 48, interior empty glass.

WHY THE WELLS ARE OPEN
The game draws a 56 x 56 vanilla spell plate with its icon in each well BEFORE it draws this HUD,
so the plate shows through the opening and the frame's inward shadow lands on it - the well reads
as a recess the plate sits in, and the icon's usable area is the 50 x 50 inside the shadow. So the
opening must be clean transparency and the shadow must be alpha, not painted grey.

DELIVER
1. hud-v6.png - the whole HUD as above.
2. hud-v6-liquid.png - the same canvas, transparent except two opaque discs: the health sphere's
   interior in deep blood red and the mana sphere's in deep blue, radius 102 at the same centres,
   with the reference spheres' glossy shading painted on them. The game clips these to the fill
   level and draws them under the cradles.
3. hud-v6-wells-only.png - optional: the plate alone (x 336-1497), for checking the well openings.
```

## What will happen with the delivery

- `tools/CutHudPlate.ps1` reads the new master: band, cut lines, wells, spheres, belt cells - the wells become the transparent openings found by alpha like the belt holes are today.
- Draw order changes: the wells' plate and icon draw before `DrawMiddleHudArt`, and the plate's alpha (the inward shadow) has to be honoured in the blit - today the plate is quantised to opaque-or-not at alpha 128, so the 9px gradient needs a half-transparent pass of its own. That is the one real code change in this brief.
- The liquid comes from file 2 instead of being lifted from under the cradle's alpha.
- `SkillWellNetSize` goes 46 → 50 and the well rects come from the header as now.

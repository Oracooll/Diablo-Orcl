# ChatGPT brief: belt-menu glyphs and inventory-tab glyphs

Written 2026-09-06 for v1.9.290. Both windows already draw the vanilla gold/grey plate and a
text stand-in; the glyphs replace the stand-ins. Paste the block below into ChatGPT as one message.

## Where they land

- **Burger menu window** (hud_menu.cpp): eight cells of the 37x38 vanilla plate, four across, two
  rows. A glyph is drawn 1:1 on the plate through `IsGlyphFrame`, so the format must be the skill
  glyphs' two-colour format. Entry order is `MenuEntries`: Character, Quests, Runeword Book, Game
  Menu, Inventory, Spellbook, Crafting, Event Log.
- **Inventory tabs** (inv.cpp `DrawInventoryTabs`): ten 28x28 plates, the open one 32x31. One glyph
  per tab, the numeral 1..10, or a small motif per page if the user prefers - the brief asks for
  numerals.

## The prompt

```text
I need two small sets of UI glyphs for a Diablo 1 (DevilutionX fork) HUD, in the same
vanilla-style glyph format as the "oracool-skill-glyphs-vanilla-v1" pack you produced:
flat white silhouettes with a hard offset shadow, no plate, no frame, no colour, no
antialiasing. The game draws its own gold or grey plate underneath.

SET A - belt menu, 8 glyphs, canvas 37x38 (the plate's own size):
  1. character   - a standing figure or a bust; reads as "hero sheet"
  2. quests      - an unrolled scroll with a wax seal, or a quill on a scroll
  3. runewords   - an open book with a rune on the page
  4. game_menu   - a cog, or three dots in a row
  5. inventory   - a backpack or a satchel
  6. spellbook   - a closed tome with a clasp, or an open book with a star
  7. crafting    - an anvil, or crossed hammer and tongs
  8. event_log   - a lined page, or a quill
  Visible bounding box, shadow included, inside x4..32, y4..33 (a 4 px transparent margin).

SET B - inventory tabs, 10 glyphs, canvas 28x28:
  the numerals 1 to 10, bold, in the style of the skill glyphs' strokes (10 is two characters,
  tight). Visible bounding box, shadow included, inside x3..24, y3..24 (a 3 px margin).

FORMAT for both sets, identical to the skill glyphs scaled to the canvas:
- RGBA PNG at the stated canvas size, binary transparency (alpha 0 or 255 only).
- Visible pixels use ONLY white RGB(243,243,243) for the glyph and black RGB(12,7,7) for
  the shadow. No greys, no other colours.
- The shadow is the white silhouette copied and offset (-1, +1), one pixel LEFT and one
  DOWN, drawn BEHIND the white.
- No plate, frame, bevel, text (other than the numerals in set B), gradient or background.
- Do not flatten onto black. The shadow is opaque dark colour, not colour-key black.
- Optically centred in the canvas, not only bounding-box centred.
- Strokes 2 px or thicker so they survive at 1x on a 37x38 or 28x28 plate.

ALSO DELIVER:
- A contact sheet PNG of all 18 at 1x and 4x on a mid-grey background.
- manifest.json listing each file with its set, slot index, name and measured bounding box.
- A verification pass confirming: only the two RGB colours present, alpha binary, bounding
  box within the stated margins, canvas size as stated.

Zip as oracool-hud-glyphs-v1.zip with the PNGs under glyphs/menu/ and glyphs/tabs/.
```

## When it arrives

Drop the zip in the Oracool.MPQ root. The engine already recognises the two-colour format; the
work is two strips (`ui\menu_glyphs.png` 8 x 37x38, `ui\tab_glyphs.png` 10 x 28x28), a cutter in
tools/, and the two stand-in `DrawString` calls replaced by `DrawStripIcon` on the plate.

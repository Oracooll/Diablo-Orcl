# Skill Tooltips in the Diablo II Shape (v1.9.256)

**Date:** 2026-09-05 · **Request:** "look at diablo 2 description theme. we want same theme when hovering over a skill in the abilities windows. the other places can be truncated to Name or Name, Current Level Stats." Then: "Name - in Gold. Current Level: X - in gold. Next Level - in gold." / "Tooltip window to have golden border and to not overlap abilities window. to be adjacent to it - 8px apart." / "tooltip to overlap bottom hud when hovering over lower screen skills/spells."

## The block

`ClassTreeEffectLine` (tree rows) and `BuildSpellStatBlock` (book spells) now emit D2's shape:

```
Current Skill Level: 3          <- gold
Damage: 30 - 40                  <- an active's spell-side numbers at rank 3
Mana Cost: 2

Next Level                       <- gold
Requires level 12
Damage: 35 - 45
Mana Cost: 2
```

An aura or passive prints what its rank grants, one bonus per line, from the same ApplyAura / ApplyPassive the game runs, plus its radius. An unlearned row says "Not learned" and quotes "First Level". A full row says "Fully invested". Passive-page and book-backed rows keep their special text.

Both builders take `withNext`: the Abilities window asks for both blocks, the skill picker asks for the current one only (name plus current stats, per the request). The wells are unchanged: name only.

## The panel

`DrawHoverPanel` draws line by line now. Title and the block headings (Current Skill Level, Current Spell Level, Next Level, First Level) are gold; everything else white; all centred as D2 sets them, shadowed as before. The border is a two-ring gold frame off the yellow ramp inside the theme's tracery. A new overload takes a rect to avoid: the Abilities window passes its own panel rect and the tooltip hangs 8px off its left edge (right, when the left has no room) and never overlaps it. It is still drawn from the frame's above-everything slot, so over the bottom HUD it goes for a low row.

The picker's cursor tooltip colours the same headings gold through `IsHoverHeadingLine`.

## Verification

Debug build clean; 624/625 with the standing `Drlg_l1` failure. The tooltip audit test was retargeted from the old "Now:" / "Next point:" lines to the new headings and a signed-number check for the aura sweep. A ghost DiabloOrcl.exe (36 MB working set, no world loaded) was holding oracool.mpq and was stopped to let the pack through.

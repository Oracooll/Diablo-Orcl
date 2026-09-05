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

## v1.9.257: percentages, not fractions

"use percentages in description of skills when comparing things/stats. dont use Two and a half times. Use +150%." 80 sentences across `class_tree.cpp`, `melee_skills.cpp` and `warcries.cpp` (104 occurrences, the three files repeat some) rewritten from fraction words to signed percentages: "a blow two and a half times as hard, a fifth more a rank" is "a blow at +150% damage, +20% per rank"; "hurts a sixth softer" is "deals -17% damage" (the coded value, `PassiveDamageTakenPercent`); "a tenth harder to harm" for Superstition is "+10 to fire, lightning and magic resistance", which is what the code adds. Durations went to digits as well ("40 seconds, +5 per rank"). Descriptions with no comparative number were left alone.

## v1.9.259: the three skill keys

"left skill picker hot key - A. Right skill picker hot key - S. Abilities window hotkey - D. Settable in game's main menu->options->keymapping." Two new keymapper actions, `LeftSkillPicker` (A) and `RightSkillPicker` (S), each a toggle of its quick list; `DisplaySpells` (the Abilities window) moved from S to D. All three show in Options > Keymapping. The dev ini in build\x64-Debug had `DisplaySpells=S` stored, which would have kept S and collided with the right list, so it was moved to D there too. "hot key of either of the three to be omitted in hover descriptions": the RMB well's "Hotkey: 's'" line is gone; nothing else named a key.

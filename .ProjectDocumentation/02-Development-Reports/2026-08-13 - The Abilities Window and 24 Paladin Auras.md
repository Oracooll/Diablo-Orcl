---
date: 2026-08-13
version: 1.1.81
area: UI / Abilities window, Paladin auras
---

# The Abilities Window, and 24 Paladin Auras

## What was asked

> We need to make Spellbook window a multi-purpose window. It needs to have a few more SHEETS -
> Skills, Auras (auras is active only with Paladin). We need nice solid arrow key left and right of
> title to scroll through sheets. We need to move repair skill in Skills sheet. Clicking on LMB or
> RMB open this, let's call it, Abilities window.
>
> Auras - assortment of icons you need to extract and introduce in the game and in the Auras sheet.
> Auras Descriptions - Attach the descriptions to the auras in Auras sheet. Plan on how to implement
> the Auras abilities in the game.

Two files supplied: `Paladin Auras.png` (a 6x4 icon sheet) and `Paladin Auras Description.docx`.
Done unattended.

## The window

One window, three sheets, cycled by a solid gold triangle at each end of the title band. The title
is the **sheet's** name - SPELLS / SKILLS / AURAS - so the arrows visibly change something.

- **Spells** - as before: every book spell, learned ones in gold, unlearned greyed and inert.
- **Skills** - the class's innate skill (Item Repair for the Paladin, Trap Disarm for the Rogue, and
  so on), plus the built-in Town Portal in single player.
- **Auras** - all 24, Paladin only.

Each sheet keeps **its own scroll offset**, so switching away and back does not lose your place.

The arrows are drawn from primitives rather than art: two solid triangles on a flat band. An asset
would be another file to cut, ship, pack and keep in step with the theme's gold, for no gain.
`CycleAbilitySheet` skips sheets the class does not have, so a non-Paladin cycling right from Skills
lands back on Spells rather than on an empty page.

### Repair moved, and why that is a real change

The class skill used to live in **page 0, slot 0 of `SpellPages`** - patched into the spell grid on
the way past by a switch on the player's class. That made it look like a spell and put it in the
spell list. It is now resolved by `GetClassSkill()` and listed on its own sheet, and `BuildSpellRows`
simply skips that slot.

Town Portal joins it there. It is not a spell in this fork - it is granted at the belt, free and
always available in single player - but it *is* a skill the character has, so the Skills sheet is
where it belongs. Two consequences had to be handled explicitly:

- `IsSpellKnown` reports it known by definition, because it is not in any of the spell bitmasks that
  function normally consults.
- Its icon is forced to `SpellType::Skill`. `GetSBookTrans` runs a spell through `CheckSpell`, which
  reports the portal unusable *because it was never memorised* - so it would have drawn greyed out
  on the Skills sheet despite always being castable.

### The skill buttons open it

Both HUD skill wells now open the window. They were inert before: the RMB well displayed the readied
spell but only the `s` hotkey could change it, and the LMB well was purely decorative.

That made a fourth place that toggles this window, so the duplicated body became one funnel -
`ToggleAbilitiesWindow()` - which the burger menu's entry now calls too. The keybind and the gamepad
action keep their own control-scheme bookkeeping and stay separate, but call the same scroll reset.

## The auras

### Cutting the icons

`tools/CutPaladinAuras.ps1`. The sheet is 6x4 cards on white; each card is a square stone frame with
a name plate hanging below and overlapping its bottom edge. The plate is not wanted - the game draws
the name as text, and baking it in would give two names in two fonts, with the baked one beyond a
translator's reach.

**Two things this got wrong first and the fix for each:**

*Finding the plate.* The first attempt scanned downward for the first row narrower than 90% of the
card, and stopped at the first row the emblem's own glow happened to inset - somewhere up in the
artwork. Every one of the 24 icons shipped with its full name plate attached. Now it measures the
frame's width from the top 60% of the card (where there is certainly no plate), then scans **up**
from the bottom for the last row still at full width. And because these frames are square, the cut's
aspect ratio is now **asserted** - a ratio check would have failed the first version instantly
instead of letting it through to a preview.

*Keying the white.* By flood fill from the card border, not by a brightness threshold. Several
emblems are white or near-white at their centre - Aura Mastery's figure, Cleanse's burst, Holy
Freeze's snowflake - and a threshold would have punched holes straight through them.

Output is one 912x38 strip, 24 cells of 38x38, matching the small spell icon so both sheets keep the
same row rhythm. Packed into `oracool.mpq`, which is searched before every other archive - a loose
PNG alone would have been shadowed by the stale copy in there.

### Locked auras

Auras unlock in four tiers by character level: Initiate 1+, Templar 8+, Crusader 16+, Champion 24+.
Six per tier, which is also exactly one row of the icon sheet.

A locked aura is **blended into the panel at half strength** rather than drawn from a second, greyed
copy. The Spells sheet greys unlearned entries with `SetSpellTrans`, which works because spell icons
are a single palette ramp; these are full-colour paintings with no ramp to remap. Its second line
also states the level and tier that would unlock it, which is the useful information at that point.

### Descriptions

From the supplied document, one sentence each, kept short enough to wrap to two lines beside the
icon. Where the document revised an effect after first proposing it - Defense, Defiance, Resistance
and Aura Mastery were all refined once the one-active-aura rule was settled - the **revised** wording
is what ships.

### Clicking an aura does nothing, on purpose

They are listed and described; they have no gameplay effect yet. Making them selectable would set a
state nothing reads and invite "I picked Might and nothing happened". The gameplay design is
[[Paladin Auras - Gameplay Implementation Plan]].

## One trap worth recording

`HeroClass::Paladin` **does not exist**. Oracool renames the Warrior to "Paladin" in display data
only (`playerdat.cpp`'s className); the enum, the sprite folder ("warrior") and every save field keep
the original name. `ClassHasAuras` therefore tests `HeroClass::Warrior`, with a comment saying so -
written the other way it compiles fine everywhere else in the codebase and silently never matches,
which would have disabled the entire feature with no error.

## Files

- `tools/CutPaladinAuras.ps1` - new cutter.
- `Source/oracool/auras.h/.cpp` - enum, table, tiers, unlock rule, icon indices. Registered in
  `Source/CMakeLists.txt`.
- `Source/oracool/hud_art.cpp/.h` - the icon strip asset, `DrawAuraIcon`, `GetAuraIconSize`;
  `BlitHalfTransparentSkipZero` gained optional source-window parameters so one cell of a strip can
  be blended.
- `Source/panels/spell_book.cpp/.hpp` - the multi-sheet window.
- `Source/diablo.cpp` - skill-button click, arrow release.
- `Source/oracool/hud_menu.cpp` - its spellbook entry folded into the shared toggle.

## Verification

Debug config builds clean at `1.1.81`. Full suite **351/353** - the two failures are
`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2` ("Unable to load
character"), the same two this project has recorded as pre-existing in every build this session, and
neither `pack.cpp` nor dungeon generation is in this change set.

The icon cut was checked by eye at 4x before and after the plate fix.

**Not seen in game.** Nothing here has been run - the game cannot be driven from this session. Per
the new-window rule, every clickable needs confirming:

1. Both arrows cycle sheets; the title changes; a non-Paladin skips Auras entirely.
2. Both HUD skill buttons open the window; the burger menu entry still does.
3. Spells sheet: a learned row readies the spell, an unlearned one does nothing.
4. Skills sheet: Item Repair and Town Portal both listed, both with normal (not greyed) icons.
5. Auras sheet, on a Paladin: 24 icons, correct art per name, the first six lit at level 1 and the
   rest greyed with "Requires level N".
6. Each sheet scrolls independently and remembers its position.
7. Clicking empty space anywhere in the window does not walk the player.
8. Opening the window hides the mini-map and event log.

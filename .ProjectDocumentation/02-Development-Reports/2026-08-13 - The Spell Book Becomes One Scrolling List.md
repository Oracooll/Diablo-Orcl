---
date: 2026-08-13
version: 1.1.72
area: UI / Spell book
---

# The Spell Book Becomes One Scrolling List

## What was asked

> 1. Title - SPELLBOOK
> 2. Separator as in Quests, Waypoints, etc...
> 3. List of all game spells in scrollable screen
> 4. Reuse Spell Icons
> 5. Reuse Spell Icon Frames
> 6. List to include all spells by default, but unlearned ones to remain desaturated ergo inactive.

## The window

340x720, flush top-right, mirroring the inventory - the two share the right-hand slot and are never
open together, so they should occupy exactly the same space. Shared theme throughout: half-
transparent fill under the ornate bevel, outlined FontSize30 "SPELLBOOK" in the title band, the same
separator rule as the waypoint list, quest log, character sheet and inventory. Five windows, one
treatment.

## What "all game spells" means

Not the `SpellID` enum. Enumerating that sweeps up six unimplemented entries (DoomSerpents,
BloodRitual, Invisibility, Mana, Magi, Jester) and the Hellfire rune items, none of which the book
has ever shown and several of which have no icon.

The `SpellPages[6][7]` table stays - but as the **source** of the list rather than as a layout. It
is the game's own curated set of book spells, so flattening it lists exactly what the six tabs used
to reach, in the order they used to appear. `BuildSpellRows` walks four pages in Diablo and five in
Hellfire (page 5 is Invalid padding), resolves page 0 slot 0 to the current class's skill exactly as
the tabbed version did, and drops Town Portal, which is a built-in ability here rather than a book
spell.

## Desaturation was already in the engine

The whole "unlearned" treatment is one line:

```cpp
SetSpellTrans(known ? GetSBookTrans(sn, true) : SpellType::Invalid);
```

`SpellType::Invalid`'s translation table maps the icon's colour ramps onto `PAL16_GRAY`
(spell_icons.cpp) - which is precisely the desaturated, inactive reading asked for, and is already
the grey the game uses for a spell you cannot currently cast. No second set of art, no recolour
pass, and it stays correct if the icons are ever replaced.

Unlearned rows also take `ColorUiSilverDark` for both text lines, so the words go quiet with the
picture instead of staying bright beside a grey icon, and their second line reads "Not learned"
rather than a mana cost the player cannot pay. Clicking one is inert - it is listed so the book
shows the whole spell set, not so it can be readied.

Icons and their frames are the existing `DrawSmallSpellIcon` / `DrawSmallSpellIconBorder`, unchanged.

## Scrolling

34 rows (Diablo) at 44px each against a 595px window. Mouse wheel at three rows per notch, gated on
the cursor being over the book so the wheel still zooms the dungeon elsewhere; `ResetSpellBookScroll`
at every open. Rows draw through a clipped subregion, so one straddling the top or bottom edge is
cut there rather than spilling onto the title band - same approach as the character sheet.

The gamepad's `SpellBookMove` used to page between tabs on left/right. It now scrolls, and accepts
up/down as well as left/right - the old binding was horizontal and muscle memory is cheap to honour.

## The right-panel trap, closed before it fired

The previous report
([[2026-08-13 - Three Windows Routing Clicks Through a Rect They Outgrew]]) flagged this exact
change as the thing that would re-arm that bug on the right-hand side: `GetRightPanel()` is 320x352,
and a book drawn at 340x720 but hit-tested against that rect would let clicks below y=352 through to
the ground. Handled as part of this pass rather than discovered afterwards:

- `GetSpellBookPanelRect()` exported, and `IsOverRightPanel` (control.cpp) now asks it instead of
  `RightPanel`. That one function was already the right-hand authority, so most callers inherit the
  fix for free.
- `GetPanelPosition(UiPanels::Spell, ...)` returns the book's own origin.
- The click router and the walk-suppression check (diablo.cpp), hover suppression (cursor.cpp) and
  the touch handler all moved onto the new rect.

`GetRightPanel()` itself is untouched and still correct for what remains of it.

## Removed

- `data\spellbk` (the parchment page) and `data\spellbkb` (the tab buttons) are no longer loaded.
- `sbooktab` - the global that indexed those tabs - is deleted, along with its `extern` and its
  reset in `InitControlPan`. It had exactly three references left and no way to change value.
- `PrintSBookStr`, `SpellBookDescription`, and the two-column absolute-position text layout.

## Files

- `Source/panels/spell_book.cpp` - rewritten. `Source/panels/spell_book.hpp` - four new exports.
- `Source/control.cpp` / `.h` - `IsOverRightPanel`, `GetPanelPosition`, `sbooktab` removal.
- `Source/diablo.cpp`, `Source/cursor.cpp`, `Source/controls/plrctrls.cpp`,
  `Source/controls/touch/event_handlers.cpp`, `Source/oracool/hud_menu.cpp`.

## Verification

Debug config builds clean at `1.1.72`.

Wanted in game:
1. Every spell listed, learned ones in gold with mana/damage, unlearned greyed and reading
   "Not learned".
2. Clicking a learned row readies it; clicking an unlearned row does nothing.
3. Wheel scrolls; wheel off the book still zooms the dungeon.
4. **Clicking empty space anywhere in the book does not walk the player** - especially below
   y=352, which is where the old rect ended.
5. Reopening the book shows the top.

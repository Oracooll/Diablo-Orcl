# One Slot, One Occupant

**Version:** 1.7.96
**Date:** 2026-08-19
**Tests:** 445/447 (the two standing baseline failures)

## 1 — the wells' content nudged 1px right, 3px down

The bezel painted into `middle_hud.png` is not perfectly symmetric about the button rect the code
derives, so geometric centring reads high and left. `NetRectIn` now carries a `{1, 3}` nudge, applied
once where the net rect is built rather than at each drawing site - so plate, tree icon, attack icon
and spell icon all move together and stay aligned with each other.

## 2 — the aura and the right button are ONE slot

"If I put a combat skill on RMB I can't seem to put an aura there. If I put Reg Attack then I AM able
to." That was the rule as written: the aura only took the well when nothing was readied, and wore a
corner letter otherwise. The letter was that compromise made visible - which explains the second half
of the question and is why it is gone.

They are now mutually exclusive, exactly as D2 has it - an aura is what the right button is set to,
not a badge riding beside it:

- lighting an aura clears the readied right-button skill (`ToggleClassAura`);
- readying a right-button skill puts the aura out (`ClearClassAuraForRightButton`, called from the
  tree click, the spells click, the speedbook and the hotkey path).

No third state, nothing invisible, no corner letter. The well simply shows its occupant.

## 3 — spell icons respect the net rect too

The RMB well's spell path computed its own geometry from the button rect plus its own hand nudge, so
a readied *spell* sat a few pixels off from a readied *skill* in the same hole. It now draws through
`GetRmbSkillWellNetRect()` like everything else, scaled to fill via the new
`DrawSmallSpellIconFittedTo(out, cell, spell)` - the fitted draw generalised from plate-only to any
spell frame.

## 4 — the Spells sheet gets the tree's cell

`SheetIconSize = 56` is now one constant for both sheets (the tree already used 56; the list was at
the engine's 37x38, which is why they felt like different windows). The Spells rows draw their icon
scaled to that square, and the spend controls moved onto it: green plus bottom-right, red minus
bottom-left, the invested level between them on the bottom edge. Row height follows to 64.

The right-edge `[+N] [+]` group is retired with them - two spellings of one control on two sheets was
the inconsistency.

**New: a minus for spells.** `CanRefundSkillPoint` / `RefundSkillPoint` mirror the tree's refund - one
rank at a time, free, back to the unspent pool. Previously a point put into a spell by accident cost
a paid respec to recover. Deliberately not gated on `IsSkillInvestable`: a spell can stop being
investable, and points already sunk must not be stranded.

## 5 — F12 took two screenshots

`CaptureScreen` blocks for 300ms inside `SDL_Delay` while it restores the palette, so an ordinary
press is still held when the handler returns - long enough for the OS to queue an auto-repeat, which
captured a second file. Auto-repeat is now dropped for F9-F12 only; F10/F9 had the same silent
double-step on game speed. Movement and hotkey keys still repeat.

## Files

- `Source/oracool/hud_layout.cpp` - the nudge
- `Source/oracool/class_tree.h` / `.cpp` - `ClearClassAuraForRightButton`, and lighting clears `_pRSpell`
- `Source/oracool/attack_skills.cpp`, `Source/panels/spell_list.cpp` / `.hpp`, `Source/control.h`,
  `Source/engine/render/scrollrt.cpp` - the aura owns the well, the badge is gone
- `Source/panels/spell_icons.cpp` / `.hpp` - `DrawSmallSpellIconFittedTo` takes a SpellID
- `Source/oracool/skill_points.h` / `.cpp` - the refund
- `Source/panels/spell_book.cpp` - 56px list cells, spend corners, click handling
- `Source/diablo.cpp` - the repeat filter

## To look at in game

1. Every well's content sits centred in the bezel; a spell and a skill land in the same place.
2. Light an aura with a skill on RMB - the skill goes, the aura arrives. Ready a skill with an aura
   burning - the aura goes out. No letter in the corner either way.
3. Spells sheet: 56px icons, plus/minus on the icon, level on its bottom edge. The minus returns a
   point to the pool.
4. F12 writes exactly one screenshot, held or tapped.

---
date: 2026-08-13
version: 1.1.85
area: UI / Abilities window
---

# Two More Sheets, and the Speedbook Retires

Follow-on to [[2026-08-13 - The Abilities Window and 24 Paladin Auras]]. Five requests, all touching
the same window.

## S opens Abilities; the speedbook ring is gone

> S button to open Abilities window. Forget about this belt of spells/skills above the bottom HUD.

`DoSpeedBook()` was the **only** place in the codebase that ever set `spselflag` true. Replacing its
body with `ToggleAbilitiesWindow()` therefore retires the ring in a single edit, and all three
callers - the S key, the gamepad's quick-spell action, and the RMB well via `DoPanBtn` - inherit the
new behaviour without three separate changes. With the flag permanently false, `DrawSpellList` and
`CheckSpellList` are unreachable and the ring cannot render. They are left in place for a later
cleanup pass rather than deleted in the same edit.

The keybind keeps its action id `"DisplaySpells"` so saved keybindings survive; only its displayed
name and description changed to "Abilities".

**Two bugs found while doing it**, both of which would have shipped:

- `DisplaySpellsKeyPressed` set `sbookflag = false` immediately before toggling it. S could
  therefore only ever *open* the window, never close it. Removed - `ToggleAbilitiesWindow` already
  closes what it needs to.
- The LMB/RMB skill-button handler added in 1.1.80 ran **before** `DoPanBtn` and would have silently
  killed **shift-click-to-clear** the readied spell, which is `DoPanBtn`'s job on the RMB well. It
  now claims only the LMB well and leaves the RMB one alone.

## Spells reserved to the Sorcerer

> Reserve spells ability sheet for sorcerer only.

`IsSheetAvailable(Spells)` now tests for `HeroClass::Sorcerer`.

That broke an assumption planted a version earlier: two guards fell back to `AbilitySheet::Spells`
when the current sheet was unavailable. That was right while Spells was universal and became wrong
the instant it was not - a Paladin would have been sent to a sheet it does not have, by a guard
whose entire job was to prevent exactly that. Both now call `FirstAvailableSheet()`, and Skills is
documented as the always-available one because every class has an innate skill.

The arrows are also drawn and hit-tested only when `AvailableSheetCount() > 1`. With Spells reserved,
several classes are down to Skills alone, and arrows on a window that cannot turn are a control that
lies.

## Sorting

> Arrange Auras by LVL req. / Arrange Spells by LVL req + alphabetically.

Auras sort by tier minimum level then name; Barbarian skills the same; spells by requirement then
name.

**Display order is deliberately not enum order.** The aura and Barbarian enums follow their icon
sheets, which is what makes `GetAuraIconIndex` / `GetBarbSkillIconIndex` the identity and keeps art
and data impossible to desync. The sorted order is a lookup on top
(`GetAuraAtDisplayIndex`), built once into a static table rather than re-sorted per frame.

For spells, "LVL req" can only mean the **Magic** requirement: Diablo puts no character-level gate on
a spell at all. `minInt` is that number. (`sBookLvl` exists but governs which dungeon level drops the
*book* - loot availability, not a requirement.)

## The Barbarian sheet

> Here are Barb's skills icons and descriptions. Make a sheet of his own on the ability window and
> put these there. Arrange by level and alphabetically.

18 skills, Barbarian only, in `oracool/barb_skills.h/.cpp` - the same shape as the aura module.

### The icon cut needed a different technique from the auras

Both sheets are framed icons with a name plate, but the plates differ in a way that defeats the
earlier approach. On the aura sheet the plate hung **below** the frame and was visibly narrower, so
the frame's bottom was found by scanning for where the card's width collapsed. Here the plate is
**inside the tile** and the stone border runs full width straight past it - a width scan finds the
tile's real bottom every time and keeps the name.

So the cut is geometric instead: the artwork panel is square, so a square of the card's own width
taken from the top lands just above the plate, discarding ~36px. An assert requires the card to be
meaningfully taller than wide, so a grid mis-detection fails loudly rather than shipping names.

### The green-scrub assertion fired, and was right to

The cutter asserts no background green survives. It flagged skill 6 keeping 6 green pixels - which
is **Natural Resistance, whose artwork is a green glowing figure**. The check could not tell artwork
green from a keying miss.

Fixed by restricting it to the outer 2px ring rather than deleting it: background green can only
enter the output where the tile meets the backdrop, and there the tile is always grey stone. The
check keeps its value and stops firing on legitimate art.

### The levels are Oracool's

The brief describes all 18 and groups them into Warcries, Combat and Passives, but **never assigns
levels**. They are laid out as three tiers of six at **1 / 8 / 16** - mirroring the Paladin's aura
tiers so both classes progress on the same rhythm, and falling out of the sheet's 6x3 grid the way
the aura sheet's 6x4 gave four tiers. Within a tier the order follows the brief's own signals: its
Fury costs, and its note that Shout is "suitable from early levels onward".

Each row carries a right-aligned **kind** tag (Combat / Warcry / Passive / Utility), because "do I
press this, or is it always on?" is not answerable from the description and is the first thing worth
knowing.

Like the auras, clicking a Barbarian skill does nothing - they are listed and described, and the
gameplay pass does not exist. The brief's **Fury** proposal (built in melee, spent on skills,
decaying out of combat) is the Barbarian's counterpart to the Paladin's auras and belongs in the
same plan document when that work starts.

## Shared, not duplicated

The aura and Barbarian rows are the same shape, so they share one `DrawDescribedRow` in
spell_book.cpp and one `DrawStripIcon` in hud_art.cpp rather than being two near-identical copies.

## Verification

Debug config builds clean at `1.1.85`; full suite 351/353, the two known pre-existing failures. All
18 Barbarian icons checked by eye at 4x after the plate fix.

Not seen in game. Per the new-window rule, the clickables to confirm: both arrows on each class
(a Sorcerer has Spells+Skills, a Paladin Skills+Auras, a Barbarian Skills+Barbarian, a Rogue Skills
alone and therefore **no arrows**), S toggling the window open *and closed*, and shift-clicking the
RMB well still clearing the readied spell.

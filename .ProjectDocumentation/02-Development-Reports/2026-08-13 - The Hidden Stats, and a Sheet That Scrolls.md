---
date: 2026-08-13
version: 1.1.64
area: UI / Character sheet
---

# The Hidden Stats, and a Sheet That Scrolls

## What was asked

> We need to discuss HIDDEN but interesting character stats - I assume there are stats in the game
> which affect the player but are not visible on the current character screen.

There are, in quantity. Asked whether to show the item flags as readable states or as derived
numbers, the answer was **"derived numbers where one exists"**. Asked for a second tab, the answer
then changed to:

> Instead of second tab just list all of these stats below Mana. Make the Char screen scrolable.

Plus two rules that reshaped the layout:

> Heads up: scrolling moves clickable object (PLUS button, RESET button). UI needs to account for
> that and move clickable area as well.

> Heads up: we should avoid constant rearranging of texts based on different DATA's name length.

> Align the divider between DATA name and DATA itself dead center of the screen.

## The twenty new readings

Every one is derived at the site that actually consumes it, and each helper carries the file:line
of that site. A sheet that quietly disagrees with the combat code is worse than no sheet.

| Row | Derivation | Consumed at |
|---|---|---|
| Block chance | `dexterity + class block bonus` | player.cpp:784 |
| Damage taken | `_pIGetHit`, flat, added to incoming damage | player.cpp:691 |
| Trap damage | 50% or 100% | missiles.cpp:1080 |
| Thorns | flat 1-3 per melee hit taken | monster.cpp:1231 |
| Armor pierce | `_pIEnAc` | player.h:592 |
| Spell to hit | `GetMagicToHit()` | player.h:621 |
| Fire damage | `_pIFMinDam`-`_pIFMaxDam` | player.cpp:624 |
| Lightning damage | `_pILMinDam`-`_pILMaxDam` | player.cpp:961 |
| Attack frames | `_pAFrames - skipped` (4/3/2/1 by tier) | player.cpp:222-241 |
| Recovery frames | `_pHFrames - skipped` (3/2/1) | player.cpp:2737 |
| Life steal | 3% or 5%, plus `0-12%` if random-steal | player.cpp:708-745 |
| Mana steal | 3% or 5%, zeroed by NoMana | player.cpp:720 |
| Spell levels | `_pISplLvlAdd` | player.h:661 |
| Light radius | `_pLightRad` | player.cpp:2601 |
| Demon damage | 300% or 100% | itemdat.h:732 |
| AC from armor / magic / dexterity / level | the four terms behind the single Armor class number | player.h:570 |

### Two judgement calls worth recording

**Block chance is quoted against an equal-level attacker.** The roll is
`target.GetBlockChance() - attacker._pLevel * 2`, and `GetBlockChance()` adds the target's *own*
`level * 2`. Against an equal-level attacker those two terms cancel exactly, leaving dexterity plus
the class bonus. A sheet has no attacker to name, so that is the honest number - and it happens to
be the simplest one. It reads 0 without a shield, because `_pBlockFlag` gates the roll entirely and
the potential is unreachable.

**Attack frames quote the fresh-swing branch.** `StartAttack` has two: a fresh swing and a repeated
one, and the repeated branch skips fewer frames - it even suppresses Fastest entirely when Fast or
Faster is also equipped. Quoting the repeated branch would understate every speed item on the
character, so the sheet quotes the first.

Lower is faster for both frame rows, which is why they reuse the Now/Base column pair: Now is what
the items give, Base is the class/weapon animation before any modifier.

## Fixed columns, on the centre line

The first version measured the label column from the longest translated label. That is defensible
in isolation and wrong in practice: it makes the column geometry a function of the row list, so
adding one long-named stat slides every value, every + button and RESET sideways across the whole
sheet. Twenty new rows was exactly the case that exposes it.

The geometry is now compile-time and anchored to the window, not the data:

```
CharColumnDividerX   = 340 / 2 = 170     <- the panel's dead centre
CharLabelColumnWidth = 170 - 3 - 6 = 161
ValueColumnX  173 ("Now")   SecondValueColumnX  223 ("Base")
StatButtonColumnX  275      scrollbar  329..332
```

A `static_assert` holds the divider on the centre line, and `EnsureLayout` asserts that no label
exceeds the fixed column - so a longer label added later fails loudly in the Debug build instead of
silently clipping. English's longest ("Points to distribute") is 139px against 161 available.

## Scrolling

Forty rows at 28px with eleven 7px group gaps is 1197px in a 595px window, so the sheet scrolls
602px. Mouse wheel, three rows per notch, gated on the cursor being over the sheet so the wheel
still zooms the dungeon everywhere else while it is open. `OpenCharPanel` returns it to the top.

**Rows draw through a clipped subregion.** `DrawChr` takes `out.subregion(...)` over the scrolling
area and draws rows and widgets in content-relative coordinates. A row straddling the top or bottom
edge is cut there rather than spilling onto the title band or the bottom bevel - which is what
makes pixel scrolling safe without per-row visibility juggling.

### The moving-widget problem, both halves

The user flagged the first half before it could bite: the + and RESET buttons are positioned
per-row, so scrolling moves them and their hit rects have to move too. `PlaceWidgets()` rewrites
`ChrBtnsRect` and `ResetButtonPosition` from `rowTop - ScrollOffset`, and is called from exactly
two places - `EnsureLayout` and `ScrollCharacterSheet` - which are the only two things that can
move a widget.

The second half is subtler and does not announce itself: a button scrolled *off the top* has a rect
that now sits under the title band, where it would happily accept a click. Every hit-test path is
therefore gated on `GetCharacterContentRect()` - a point outside the scrolling area can never be
inside a rect that is also outside it:

- `CheckChrBtns` and `ReleaseChrBtns` (control.cpp). Release also clears the pressed flags, so a
  button that scrolls out from under the cursor between press and release cannot act on the release.
- `InteractsWithCharButton` (touch/renderers.cpp).
- Gamepad navigation (plrctrls.cpp) does the opposite and better thing: rather than refusing to
  move the cursor to an off-screen button, it scrolls the sheet until that button is visible, with
  a no-progress guard for the ends of the range.

## Files

- `Source/panels/charpanel.cpp` - 20 new rows, six derived-reading helpers, fixed column geometry,
  scroll state, clipped drawing, scrollbar.
- `Source/panels/charpanel.hpp` - `GetCharacterContentRect()`, `ScrollCharacterSheet()`,
  `ResetCharacterSheetScroll()`.
- `Source/control.cpp` - scroll gates on both hit-test paths, scroll reset on open.
- `Source/diablo.cpp` - wheel wiring, both directions.
- `Source/controls/plrctrls.cpp`, `Source/controls/touch/renderers.cpp` - scroll-aware hit-testing.

## Verification

Debug config builds clean at `1.1.64`. Geometry is checked by `static_assert` (divider on centre,
single-value width) and by runtime asserts (label fits its column, buttons clear the scrollbar).

Wanted in game, since none of this can be asserted:
1. Wheel scrolls the sheet; the wheel still zooms the dungeon when the cursor is off the panel.
2. + buttons and RESET work at every scroll position, and do **nothing** when scrolled out of view.
3. No row draws over the title band or below the bottom bevel at any scroll position.
4. Reopening the sheet returns it to the top.
5. Spot-check a derived number against an item: equip something with Fast Attack and confirm the
   attack-frame row drops by 2.

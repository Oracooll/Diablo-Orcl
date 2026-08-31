---
date: 2026-08-14
version: 1.5.14
area: Front end / text layout
---

# Every Label in the Middle of Its Own Box

> make all available menus in the front end have their items/texts/articles be vertically centered
> in their invisible text boxes
>
> i mean Single Player / Settings / Exit game and similar items. They are not in the vertical center
> of their invisible text boxes.

## What was actually wrong

Nothing was mispositioned. Every front-end label was drawn exactly where it was told to go - at the
**top** of its rect. `DrawString` only centres vertically when asked, via `UiFlags::VerticalCenter`,
and the single-player screens never asked. The multiplayer ones (selconn, selgame) did, on every
button and list, which is why the two halves of the same front end had never looked quite alike.

Where the box is the size of its text, top-aligned and centred are the same picture and nobody
notices. The main menu is where they stop being the same picture: its rows are **86px** tall (a
deliberate choice - three entries spaced like six, see `mainmenu.cpp`) and its font's line is
**42px**. All 44px of slack fell below the label, so "Single Player" sat hard against the top of its
row with a blank half-row under it, and the row's two rotating pentagrams - which have always
centred themselves in the row - marked a centre the text was 22px above.

## Fixed once, in the renderer

`UiFlags::VerticalCenter` is now OR-ed in by `Source/DiabloUI/diabloui.cpp`'s five text render
overloads - `UiText`, `UiArtText`, `UiArtTextButton`, `UiList`, `UiEdit` - rather than at the ~40
places a widget is constructed.

Three reasons it belongs there and not at the call sites:

- A screen added later cannot forget it.
- It is idempotent. The multiplayer screens already pass the flag; OR-ing it again changes nothing,
  so there is no "which screens have it" list to keep true.
- It is a **no-op wherever the box already fits the text**. The offset is
  `max(0, (rect.h - lines * lineHeight) / 2)`, so a rect of height 0 (the common "no explicit
  height" case) and a rect exactly one line tall both offset by zero.

### The one thing that had to be checked first

`VerticalCenter` measures text height by counting `\n` in the string - not by re-running the word
wrap. If a paragraph wrapped at draw time, the count would be short, the block would be pushed too
far down, and the bottom lines would clip.

It does not happen here: **every** multi-line body in this front end is already run through
`WordWrapString` before it reaches a widget - `dialogs.cpp:74`, `selok.cpp:87`, `selyesno.cpp:142`,
`settingsmenu.cpp:156,162`, `selgame.cpp:292,452,734`, `selconn.cpp:124`. The breaks are in the
string itself, so the measured height is the real height. That is what makes the central fix safe;
without it this would have had to be a per-widget audit.

## What actually moves

Measured, not estimated - `LineHeights = { 12, 26, 38, 42, 50, 22 }` against each rect's height:

| Screen | Box | Font line | Shift |
|---|---:|---:|---:|
| **Main menu items** | 86 | 42 | **+22px** |
| Hero list rows (characters, classes) | 52 | 38 | +7px |
| Hero name box | 50 (after its 1px inset) | 38 | +6px |
| Settings description, 1 line | 64 | 18 | +23px |
| Settings description, 3 lines | 64 | 54 | +5px |
| Version string, bottom-left | 32 | 16 | +8px |
| Hero captions ("Enter Name") | 42 | 38 | +2px |
| Hero title, action-row buttons | 42 | 42 | 0 |
| Settings lists, settings title | 34 / 26 / 35 | 34 / 26 / 38 | 0 |
| selok / selyesno bodies | measured from the text | - | 0 |

The zeros are not a failure of the change - they are the screens whose geometry was **already**
derived from the font's line height, on purpose, so that `DrawString` would not shave descenders off
(`hero_layout.h` says so at `HeroTitleHeight` and `HeroButtonRowHeight`). Those were centred by
construction and stay exactly where they were.

The hero list rows are the second real win after the main menu: at +7px the label now shares a
centre line with the pentagrams flanking it, same as the main menu.

**One judgement call worth flagging.** The settings description sits in a fixed 64px band under the
list, and centring makes a short description float in the middle of it rather than hang from the
top - so it now shifts a little as you step between options with different-length descriptions. That
is what "articles vertically centered in their invisible text boxes" asks for and it looks more
balanced, but it is the one place the old top-alignment had an argument in its favour. Easy to
exempt if it reads badly in motion.

## Files

- `Source/DiabloUI/diabloui.cpp` - `CenteredInBox` and the five render overloads.
- `Source/DiabloUI/mainmenu.cpp` - the comment that documented the *opposite* behaviour ("text
  renders top-aligned within its row (no VerticalCenter flag below)") rewritten, since it was now
  describing what the code used to do.

## Verification

Debug config builds clean at `1.5.14`; full suite **352/354**, the two known pre-existing failures
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

Worth recording for next time: run `ctest` **serially**. Under `-j 8` two
`LoadSaveOracoolItemExtensionsTest` cases (`RoundTripsExtraTabItems`,
`RoundTripsTieredItemStoredInExtraTab`) also failed - they share a save path with their neighbours
and collide. They pass in isolation and in a serial full run; it is a harness artifact, not a
regression.

Not seen in game. Worth confirming: the three main-menu entries sit in the middle of their rows with
even air above and below, and the pentagrams line up with the text rather than with the row.

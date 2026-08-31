---
date: 2026-08-14
version: 1.5.42
area: Waypoint list / text rendering, font colour translations
---

# The Font That Would Not Take a Colour

> make the wp list gold for locked and white for unlocked wps.

> i see no change. recheck.

> it says 1.5.41 and still no change

Three messages, and the third is the interesting one. The first was a five-minute change. The second
should have been the end of it. The third meant the change was real, shipped, running - and invisible.

## What was ruled out first

The edit was in the file and reached `DrawString`. `waypoint_menu.cpp.obj` and `DiabloOrcl.exe` were
both freshly stamped. `DrawWaypointMenu` has exactly one caller (`scrollrt.cpp:1455`), so there was no
second draw path overwriting the first. `ColorWhite` and `ColorWhitegold` resolve to different entries
in `ColorTranslations`. Every plausible "you didn't actually ship it" explanation was gone.

So the code was correct and the screen disagreed. At that point guessing is worthless; the only useful
thing left is to read the pixels.

## Reading the pixels

From the user's 1280x720 screenshot, brightest pixel per row:

| Element | Flag set | Face | Measured |
|---|---|---|---|
| "Tristram" (unlocked) | `ColorWhite` | dialog | **(255,189,189)** |
| "Cathedral Level 1" (locked) | `ColorWhitegold` | dialog | **(255,189,189)** |
| "Cathedral Level 2" (locked) | `ColorWhitegold` | dialog | **(255,189,189)** |
| "WAYPOINT" title | `ColorWhitegold` | **30** | (221,196,126) |
| XP counter | - | 12 | (204,183,117) |

The title, in the same frame, from the same palette, with the same colour flag, came out gold. The rows
came out identical to each other and identical to nothing in the palette's gold ramp - a pink
(255,189,189) that no colour flag would ever produce.

Two rows with different flags rendering the *same* colour is not a wrong-colour bug. It is a colour
that was never applied at all.

## The cause

Font colours in this engine are not drawn, they are remapped. Font ink lives at palette indices
**192-207**, and each `.trn` file is a 256-byte table that rewrites those indices to somewhere else -
`whitegold.trn` sends them to 193-207 (PAL16_YELLOW), `white.trn` to 242-254 (PAL16_GRAY). A colour
flag does nothing but pick which table to run the glyph through.

The rows used `UiFlags::FontSizeDialog`. And:

```cpp
FontSizes = { 12, 24, 30, 42, 46, 22 };
```

`FontSizeDialog` is index **5** - it loads `fonts\22-XX.clx`, which is a wholly separate stock asset
from the 12/24/30/42/46 family. Its ink does not sit in the 192-207 range. Every `.trn` therefore runs
over it and changes nothing, leaving its own baked-in pink showing through whatever colour was asked
for.

The rows had never been colourable. The comment sitting above them - "Gold for reached, plain white for
not" - described an intention that the screen had never once honoured, and nobody had noticed because
the two states had never been asked to look different before.

## Changing the font, and the 31 pixels that decided the names

> change the font

> just try to keep the wp names not wrapping

Only 12, 24, 30, 42 and 46 respond to colour, and a 43px row rules out everything above 24. So the
face was forced: **FontSize24**. The open question was width.

Estimating from the screenshot suggested ~220px for the longest name, comfortably inside the 238px text
column. That estimate was wrong, and worth recording as wrong: bitmap faces are drawn, not scaled, so
22 -> 24 is not a 9% increase in anything.

The right method was to read `fonts\24-00.clx` directly - CLX frame header is `uint16 headerSize,
uint16 width, uint16 height`, glyph frame index is the codepoint's low byte - and sum the widths the
way `GetLineWidth` does (`Σ(glyph.width + spacing) - spacing`, spacing 1). Cross-checked against the
22px face first: it predicted 204px for a string measured at 202px on the screenshot, a 2px
ink-box difference. The method was sound.

The real numbers at FontSize24:

| | Widest name | vs 238px column |
|---|---|---|
| With ordinal prefix | "6. Catacombs Level 5" - **255px** | 8 of 25 names wrap |
| Without | "Catacombs Level 5" - **224px** | fits, 14px spare |

The first pass widened the panel 340 -> 368 to fit the prefixed names. The user's call overtook it:

> reduce wp names if you have to. remove the number infront of the level name.

Which is the better trade by some distance. The prefix cost 31px to display a number the row's own
position in the list already communicates, while the number that actually matters - the absolute
dungeon level - is in the name's tail and stays. So the panel went back to 340 and the names lost their
prefix, and the whole change is now a font flag and a string table.

Worth noting the prefix was purely a display string: the array *index* carries the destination level
(`StartNewLvl(*MyPlayer, WM_DIABNEXTLVL, entry)`), and nothing anywhere parses the text. Grepped to
confirm - the strings appear in this one source file and in past dev reports, nowhere else.

## The colours, now that they land

Gold for **locked**, white for **unlocked** - the inverse of the usual instinct to highlight what you
can use. It reads better here because of what the states mean on this screen: an unlocked row is
somewhere you can already go, a locked one is somewhere you have not been. The gold marks what is left
to find rather than what is already done.

Hover swaps the pair rather than adding a third colour, so a row visibly reacts whichever state it is
in. The lit or dormant sigil beside the name carries the real state cue; colour is reinforcement.

## Changed

- `Source/oracool/waypoint_menu.cpp` - rows draw `FontSize24` instead of `FontSizeDialog`; gold/white
  by lock state with hover swapping the pair; `WaypointNames` lost the ordinal prefix; `PanelSize`
  unchanged at 340x720 with the measurement recorded against it.
- `Source/oracool/waypoint_menu.h` - doc comment.

## Verified

- Glyph widths measured from `fonts/24-00.clx`, method validated against the 22px face to 2px.
- Build clean. **352/354** tests - the standing baseline
  (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`), no regression.
- 1.5.42 confirmed baked into `DiabloOrcl.exe`.
- **Not yet seen on screen.** The colours are now on a face that can take them and the widest name has
  14px of measured clearance, but neither has been confirmed in play.

## Carried forward

Any future text in this build that needs a colour must avoid `FontSizeDialog`. It is the only face in
`UiFlags` that silently ignores every colour flag, and it fails without a warning, a log line or a
compile error - the text simply arrives pink. The 22px face is fine wherever the colour is not
load-bearing; it is a trap wherever it is.

# Runeword Book Filters (v1.9.275)

**Date:** 2026-09-05 · **Request:** hover states for the item and rune filters, "requires these runes" as the rune filter's meaning, and the gold cross replaced by a yellow mirror of the red X.

## The rune filter had the wrong question

`PassesFilters` accepted a word only when EVERY rune it needs was selected - "buildable from what is selected". That is the right question for the Possible-Runewords toggle, which selects the runes you hold, and the wrong one for a hand-picked rune: clicking Tal alone showed nothing, since no word is made of Tal only. The two questions are separate now. A `PossibleMode` flag records which one the selection answers:

- **Toggle lit**: buildable - every rune of the word is among the held ones.
- **Hand-picked runes**: requires - every SELECTED rune is among the word's, and the word may need more. Two runes list every word that needs both, whatever else it needs. Clicking any rune leaves Possible mode.

## Hover and the toggle

- Item-slot keys draw their label white under the cursor.
- Rune keys draw their sprite through `BrightenTRN`, a palette table that lifts every colour a few steps up its own ramp (the 8-entry mini-ramps at 128..159 included), so a rune reads lit without leaving its palette.
- The Possible toggle is `DrawWindowCloseButtonStyled`, the red X's plate-and-X shape in yellow, at the top-left corner mirrored from the close button's rect; lit yellow while the mode is on or the cursor is on it, the "Possible RW" tip under it. `window_close.cpp`'s drawing is the shared styled function now, with the red one a two-colour call of it.

## Verification

Debug build clean; 625/626 with the standing `Drlg_l1` failure. In the book: click Tal and see every word that needs Tal; add Eth and see the intersection; the yellow X fills the row with your held runes and lists only what you can build.

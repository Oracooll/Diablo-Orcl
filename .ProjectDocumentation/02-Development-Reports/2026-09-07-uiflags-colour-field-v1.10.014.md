# Engine improvement: UiFlags carries colour as a 12-bit field (v1.10.014)

**Date:** 2026-09-07
**Request:** "let's introduce 12-bits numeric fields into this 64bit UiFlag. That will allow us to have 4096 colors/shades/trn files, correct?" (Yes.) "can this be considered Engine improve introduction?" (Yes.)

## What UiFlags is

Every string the game draws is passed one 64-bit word saying how: font size, alignment, outline, shadow, and the colour. Vanilla DevilutionX spent one BIT per colour; this fork had widened the word from 32 to 64 bits on 2026-08-15 and, after the legend's eight new colours, had 16 bits left.

## The change

A colour is now a 12-bit NUMBER in bits 48-59 of the same word (`UiFlagsColorShift`, `UiFlags::ColorMask`): the colour's index, 0 meaning none (drawn as Whitegold, the fallback every unrecognised flag always got). 4096 colours fit; the palette offers 66. The renderer's own `text_color` enum went from 8 to 16 bits so it does not cap the field.

Every `UiFlags::Color*` name is unchanged and still composes with the layout bits, so the 162 call sites that write `ColorWhite | AlignCenter` compile untouched. What changed underneath:

- `GetColorFromFlags` is one field read (a switch on the index) instead of a 28-way chain of bit tests with a precedence order.
- The outline pass (`DrawOutlinedString`) clears the field with `WithoutColor` instead of a hand-kept list of every colour bit, which every new colour had to be added to or the outline came out in colour.
- The one site that tested a colour with `HasAnyOf` (the store's red half-size sprite) uses `HasColor`.
- Helpers in ui_flags.hpp: `UiFlagsColorIndex`, `HasColor`, `WithoutColor`, `WithColor`.

## The one rule that is new

Two colours must never be ORed together: the indices would add up to a third colour, where the bit scheme picked one by precedence. The audit found no site that did so (the nine `|=` sites all write onto a style with no colour yet). `WithColor` is the safe way to change a colour a style already has.

Bits 40-47 and 60-63 are free now; a new colour is a new index, not a bit. Adding a colour is the legend's four steps minus the mask line.

## Tests

Every colour has a distinct non-zero index below 4096, a bare colour carries nothing else, composition with layout bits round-trips, `WithoutColor`/`WithColor`/`HasColor` behave, the field sits at 48-59 clear of every other bit. Suite 692/692, the two colour tests from earlier today still green. Committed locally.

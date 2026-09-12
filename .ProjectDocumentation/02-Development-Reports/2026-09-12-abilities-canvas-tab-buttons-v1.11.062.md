# The Abilities window gets its own canvas, and five buttons instead of two arrows

**Version:** 1.11.062
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "take Canvas Ability Windows.png and use it as canvas for the abilities windows. This canvas has
> prebuilt 5 buttons. You need to implement Hover and Click effect and move the Titles under the
> button row and remove the current nav arrows."

## The canvas

`Canvas Ability Windows.png` (340x720, the same size as every other side panel) is staged as
`ui/abilities_panel.png` in `Resources\03-private-assets\oracool_private_assets` - the private
archive, beside `panel_bg.png`, because it is the same class of derivative art - and packed into
`oracool_private.mpq`.

It gets its **own** `ArtAsset` rather than replacing `panel_bg.png`, because it is the only side
panel with a row of tab plates painted into its header. The other five windows have nothing to put
there and would have worn five dead buttons.

Two things follow from it being a second canvas of the same stone:

- The **Panel Gamma** setting now lives in `ApplyPanelGamma(ArtAsset&)` and both canvases call it.
  It used to be an inline loop in the `SidePanelArt` load; two canvases that disagreed about
  brightness would have been visible the moment the user moved the setting.
- The **dim** starts below the buttons. `AbilitiesCanvasInner` is `{22,54},{296,641}` against the
  shared `{22,25},{296,670}`: dimming the plates would flatten the bevels that the hover and pressed
  states are read against.

## The five buttons

The plates are painted, so the geometry is **measured off the artwork**, not spaced by arithmetic -
the gold bevels run x=22..80, 82..140, 142..201, 203..261 and 263..317 (widths 59, 59, 60, 59, 55,
the last one narrower where the right bezel crowds it), and every plate spans y=23..53.

The canvas letters them **1, 2, 3, P, S**, which is the artwork's order and not the enum's - Spells
is enum 0 and sits last on the canvas - so `TabPlates` pairs each plate with its sheet explicitly.
A `static_assert` ties the table's length to `AbilitySheetCount`: a sheet with no plate could not be
reached now that the arrows are gone.

Three states, all drawn as overlays **inside** the gold bevel (`TabPlateBevel`, 3px), because
painting over the bevel would erase the thing that makes the row read as buttons:

| State | Drawn as |
|---|---|
| Hover | half-transparent blend toward `PAL16_GRAY + 2` - the plate catches light |
| Pressed | half-transparent blend toward `PAL16_GRAY + 13` - the plate sinks |
| Open sheet | a 2px `PAL16_YELLOW + 2` ring on the plate |

The open sheet wears a ring rather than a wash so it still reads as selected while the cursor is
over it, and so the plate's painted glyph is never tinted out from under its own label.

## The title moved down

`oracool::PanelTitleTop` is 28, shared by all six windows - and on this canvas y=28 is *inside* the
painted plates, so the title would have been drawn straight through the five buttons. This window
now has `AbilitiesTitleTop` (57) and `AbilitiesTitleHeight`, and two static_asserts hold the layout:
the title must clear the tab row, and the list must start below the title. The other five windows
keep the shared line; only this one has a header row to clear.

## The arrows are gone

`GetArrowRect`, `DrawArrow`, `ArrowWidth`/`ArrowHeight`/`ArrowHitSize` and `PressedArrow` are
deleted, along with `AvailableSheetCount()` - whose only caller was the guard that hid the arrows on
a window with nowhere to turn. The plates guard themselves, one at a time, through
`IsSheetAvailable`.

Two details worth recording:

- Clicking the **open** sheet's own plate is a no-op rather than a reset, so it cannot throw away the
  scroll position of the page you are already reading.
- Switching sheets by plate does the same two things `CycleAbilitySheet` does on the way out of a
  page: clears `ArmedPassiveSlot` (a gesture does not survive leaving the page it started on) and
  calls `UpdateScrollBounds`. This was missed in the first pass and would have carried an armed
  passive across a sheet change.

`CycleAbilitySheet` itself stays - it is declared in the header and costs nothing.

## The art stays droppable

The first pass gated both drawing *and* hit-testing on `HasAbilitiesPanelArt()`, which would have
left the window with **no way between sheets at all** if the file were ever missing - the arrows
being gone. The file's own comment has promised since 2026-08-16 that the art is "droppable rather
than required", so `DrawTabPlates` now draws the row itself when the canvas is absent, from the same
`DrawLegacyTextBox` primitive the arrows were built from, lettered from `TabPlate::label`.

## Verification

- Debug and Release both build clean; **718/718** tests pass.
- The geometry was checked by compositing the computed rects back onto the canvas and looking at the
  result, since this is a window drawn over the world and constants can be perfectly plausible while
  the thing is visibly wrong. Plates, bevel insets, the title band and the content line all land
  where they should.
- `UnsafeDrawBorder2px` does no clipping, but `BottomDockedTop` clamps at zero and the panel is
  flush right, so the tab row is always fully on screen.
- RTM updated with the Release exe and the repacked `oracool_private.mpq` - the archive matters here,
  or the window falls back to the shared canvas and shows no buttons.

## Still to check in play

The hover and pressed blends, and the gold ring on the open sheet, are tuned against the palette
rather than against a screenshot - worth a look to see whether the hover is too strong over the
painted glyphs.

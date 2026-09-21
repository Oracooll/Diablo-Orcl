# The Salvage plates rearranged, and the four dear tiers ask first (v1.12.102)

**Date:** 2026-09-21 · **Version:** v1.12.102 · **Tests:** 832/832

User: "Row 1 - Salvage an Item, All Basics, All Magic, All Rares. These actions do not require confirmation. Row 2 -
All Uniques, All Sets, All Primals, All Ethereal - These actions require confirmation ... Are you sure you want to
destroy all (item type) items? ... a green one CONFIRM and red one CANCEL. Both in frames. Both sinkable upon click,
and make sure the action and the dismissal of this message happens after click release and only if release is inside
boundary of the buttons. Release of click outside the boundary of any of these buttons is considered as Let Me Think
a Bit More by the user."

## The rows

Same 66 px pitch, same two rows. Row one at y 344: the hammer (x 43), White, Magic, Rare. Row two at y 410: Unique,
Set, Primal, Ethereal. `SalvageTierNeedsConfirm` is the rule - the four of row two.

## The question

A press on a row-two plate sets `PendingConfirmTier` and nothing else happens: the results frame shows "Are you sure
you want to destroy all Unique items?" wrapped and centred, with CONFIRM and CANCEL along its foot - 100x28 each,
20 apart, a 1 px frame in green (0x64A064) or red (0xC04030) round a dark plate, the label in the matching colour,
brighter under the cursor.

Pressing a button only sinks it 2 px. `ReleaseLevskiButtons` - the window's LeftMouseUp hook - decides: if the
release is inside the button that was pressed, CONFIRM runs the salvage and CANCEL dismisses; if it is anywhere else,
neither happens and the question stands. That is the user's "let me think a bit more".

A row-one plate still acts at once, and clears any standing question by doing its own work. The question does not
outlive the window: it is cleared on close and on a new game.

`RunSalvageTier` is the bulk salvage lifted out of the click path so the immediate plates and the confirmed ones run
the same code.

# Unmet requirements in red (v1.9.300)

**Date:** 2026-09-06
**Request:** "if i cant equipt an item due to a stats requrement nt fulfilled, in the description of that item use red font for that stat requirement, so it is easier to spot it."

The requirement line ("Required: 60 Str 20 Mag 25 Dex") is one line, and the unmet stat can be any of the three, so the tooltip's two-colour head/tail mechanism was not enough. `PanelLineRun` (control.h) is the general form: per line, a list of (byte offset, colour) runs parallel to `InfoStringLineColors` and `InfoStringLineTailStart`, kept in step by the same functions, saved and restored by the comparison tooltip's capture. `AddPanelStringRuns` appends one such line; `DrawBlock` (cursor_tooltip.cpp) lays the runs out left to right from the centred origin, like the two-run case.

`PrintItemInfo` (items.cpp) compares each effective requirement with the inspected character's stat - the same comparison as `Player::CanUseItem` - and wraps an unmet one in a red run that returns to white after it. Met stats stay white.

Test `UnmetRequirementIsRedAndMetOnesAreNot`: a short sword needing 60 Str on a 30-Str character produces exactly one red run covering " 60 Str"; raising Str to 60 leaves the line one colour. Suite 635/636, the standing dungeon-generation failure only.

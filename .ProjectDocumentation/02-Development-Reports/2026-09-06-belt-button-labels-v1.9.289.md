# The belt buttons as text: blue TP, gold M (v1.9.289)

**Date:** 2026-09-06
**Request:** "i dont like these icons. [...] replace for now these icon with blue TP text and Gold M letter. Both to have text shadows."

## What was done

`DrawBeltButtonText(out, cell, text, color, state)` in hud_art.cpp draws the label with `DrawString` - FontSize24, centred both ways, `UiFlags::Shadowed` (the game's own solid-black shadow two left and two down, the same cast as the belt items'). `DrawTownPortalIcon` draws "TP" in ColorBlue, `DrawBurgerMenuButton` draws "M" in ColorGold, both over the belt plate. States keep the chosen colour: hover lifts the label one up and right off its shadow, click sets it one down and left onto it.

The painted sheets are still loaded and quantised (with the v1.9.288 blue repaint), and the opaque-bounds centring helpers are still in the file, for when the glyph-style pack the user is commissioning arrives.

## Test

`OracoolAudit.TheBeltButtonLabelsSitCentredWithShadows` replaces the ring/blue test: plate-only vs plate-and-button diff; each label has black shadow pixels and coloured ones, and its box is centred in the cell within two pixels per axis.

Suite 632/633, the standing dungeon-generation failure only.

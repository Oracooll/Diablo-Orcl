# Colours match their .trn names: in-play counterparts for the menu files (v1.10.016)

**Date:** 2026-09-07
**Request:** "change the colors in the attached file to match their trn name. Rule - colors to match their trn file names." and, on the vanilla orange row, "fix this one as well. to match its name."

## The cause

The front-end palette (ui_art\diablo.pal) lays its ramps 16 indices BELOW the level palettes': ui 176-191 is the gold that a level keeps at 192-207, ui 224-239 the gray a level keeps at 240-254. So goldui.trn is gold in a menu and steel blue in a level, grayui.trn gray in a menu and red in a level - which is how the mlvl line under a monster bar came to be red-brown. The three vanilla gamedialog*.trn are identity on the glyph band, so "white", "yellow" and "red" all drew the raw gold.

## The fix

The pattern the engine already had for dialog white - a menu entry and an in-game entry chosen by `gbRunGame` - now covers the four UI colours too. Seven Orcl files:

- `oracool_uigold.trn`, `oracool_uigolddark.trn`, `oracool_uisilver.trn`, `oracool_uisilverdark.trn`: the menu files with their bands shifted up 16, so in a level they say what they say in a menu. New `text_color` entries `ColorInGameUi*`; `GetColorFromFlags` picks them when the game runs. Nothing changes in the menus.
- `oracool_dialogwhite.trn`, `oracool_dialogyellow.trn`, `oracool_dialogred.trn`: the in-game dialog three, now the in-play white, yellow and red bands (from white.trn, yellow.trn, red.trn). The vanilla gamedialog*.trn are on the not-used list.

Vanilla orange.trn, unused since its minis were taken by the green ramp, is drawn on the legend through its in-play twin oracool_orange1.trn, so it reads orange as its name says.

## Visible in play

The mlvl line under a monster bar is silver now, and floating fire damage numbers are gray rather than dark red (both used ColorUiSilver / UiSilverDark). Everything else that named these colours is front-end.

## Counts

Orcl files 19, all used but the dark focus-glow twin and the two dialog colours with no caller; vanilla 16, of which orange.trn and the three gamedialog files are unused. Suite 692/692; oracool.mpq 428 files. Legend and wiki regenerated and republished. Committed locally.

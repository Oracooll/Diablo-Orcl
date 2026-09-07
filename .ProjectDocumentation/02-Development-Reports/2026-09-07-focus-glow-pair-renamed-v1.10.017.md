# The focus-glow pair joins the UI family: ColorUiYellow, with in-play twins (v1.10.017)

**Date:** 2026-09-07
**Request:** "now fix these two. they dont make sense." (the two rows ColorOracoolYellow / ColorOracoolYellowDark, drawn blue and marked "menu palette only")

## The cause

The pair was made on 2026-08-15 for the front-end focus glow against the MENU palette, where 128-135 is bright yellow; in a level those indices are bright blue. Named "Oracool yellow", they were the only colours on the legend whose name said one thing and whose sample said another, and the "menu palette only" note was an apology for it.

## The fix

They are ColorUiYellow and ColorUiYellowDark now, the same family as UiGold and UiSilver, and get the same treatment those received in v1.10.016: the menu files renamed to say what they are (`oracool_menuyellow.trn`, `oracool_menuyellowdark.trn`, moved with `git mv`), in-play twins shifted up 16 onto the level's bright yellow minis (`oracool_uiyellow.trn`, `oracool_uiyellowdark.trn`), `ColorInGameUiYellow` / `ColorInGameUiYellowDark` in the renderer chosen when the game runs. The generator that made the pair (tools/MakeYellowFontTrn.ps1) writes the new names. Every identifier renamed across Source, test and tools; the one caller (the front-end focus glow) is unchanged in behaviour.

## Counts

Orcl files 21; table 36 entries. Suite 692/692; oracool.mpq 430 files. Legend and wiki regenerated and republished. Committed locally.

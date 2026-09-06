# Three colours from the legend: GR-5 for sockets, BE-2 for Primal, YL-3 for Rare (v1.10.007)

**Date:** 2026-09-07
**Requests:** "use GR-5 as font color when socketed item drops on the ground. also use this color for the specs row Sockets X in its description." / "Make BE-2 - primal items font color. Use it game-wide where necessary." / "Make YL-3 default Rare items color."

## The mechanism, first use

The font colour legend (06-Reference/Font-Colour-Legend.html) gives every shade an ID and a 16-number .trn band. Adding one is now the four steps the legend lists: a 256-byte .trn in oracool_assets/fonts (generated from the band by the patch script), a `text_color` enumerator and a `ColorTranslations` row (both arrays grew 22 → 25), a `UiFlags` bit in the widened range (bits 40-42), and a line in `GetColorFromFlags` above the Whitegold fallback. One more site than the legend listed: `DrawOutlinedString`'s colour mask (ornate_border.cpp) strips the caller's colour for its black pass, and had never learned the three earlier Oracool colours either; all six are in it now.

## GR-5, sockets

`ColorGray5`. The floor label of a socketed PLAIN item (normal quality, no tier) is gray through and through, the Diablo II convention; a magic, rare, unique or set item keeps its quality colour for the name, since that is what tells qualities apart, and only its socket count "[n]" goes gray (it was red). The "Sockets: x/y" row in the description is GR-5 too.

## BE-2, Primal

`ColorBeige2` replaces the orange placeholder in `Item::getTextColor`, so every name site follows: floor labels, the cursor tooltip, the held-item line, Levski's grid, the stash, shops. The inventory slot backing for a Primal moves from the orange ramp to the beige ramp so it agrees with the name. The runeword book's rune-effect lines stay orange: that is the runes' plate colour, not the Primal tier.

## YL-3, Rare

`ColorYellow3` replaces `ColorYellow` for the Rare tier in `Item::getTextColor`. `ColorYellow` (YL-1) stays where it means lightning: the sheet's damage colour and the floating damage numbers. The Rare slot backing already rides the bright yellow minis.

## Tests

Three item tests pinned the old tier colours and now pin the new flags. The movement-curse test asserted a step under the walk for any curse; a curse of exactly 10 reads 90% and is the last walk stride, so it now asserts by the threshold (the shuffle lane rolled a 10). Suite 687/687; oracool.mpq repacked with the three .trn files. The legend's Part 1 now lists the three as in use.

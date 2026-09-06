# The slow channel wired, the stash sort by band and size, Blessed Hammer's gold mace (v1.10.004)

**Date:** 2026-09-07
**Requests:** "wire cold spells and curses to the slow channel" / "books get weirdly placed in the stash after SORT click. fix it." / "change the asset you use with Blessed Hammer. Use asset of a mace instead of this fireball you are using. Tint the mace GOLD."

## Cold on a player slows

`PlayerMHit` (missiles.cpp) is the one gate every missile that reaches a player passes through. A `DamageType::Cold` hit that LANDS - past the to-hit roll and past the block - now calls `oracool::SlowPlayer(player, 60, 25)`: a quarter off the stride for three seconds. The sheet's Move speed row turns red for it, and `WalkFrameSkipFor` takes a step off the walk. Cold also gained a resistance case in that switch (magic resistance stands in, as it does for acid; players have no cold resistance of their own). Nothing in the game casts cold at players today; whatever does from now on slows them without another line.

## The curse

`IPL_MOVESPEED_CURSE`, "-X% movement speed", appended after `IPL_MOVESPEED` (the loader's affix-type bound moved with it). Same drop-tail roll as the bonus: one roll in four is the curse, -10..-20%, stored in the item's own affix record with a positive magnitude and the other sign, which is the `IPL_GETHIT_CURSE` idiom. SaveItemPower subtracts, both tooltip printers print the negative, the loader's re-derive subtracts. `MovementSpeedBonusPercent` is signed now (it clamped at 0), so a cursed ring reads 85% on the sheet, in red, and the feet take a step down.

## The stash sort

`SortStash` ranked books as Others with every trinket, ordered by value, and the first-fit placement then dropped each 2x2 book into whichever gap the 1x1s before it had left - the "weird" placement. Two changes in stash.cpp: books are a band of their own (rank 6; Others moved to 7), and within a band the sort goes by footprint (area, then height) before value, so a run of same-size items packs cleanly and the small ones fill the remainder.

## Blessed Hammer

`AddBlessedHammer` now borrows the item-drop tumble of items\mace.cel through `UseItemDropAnimation`, the same call Fist of the Heavens falls with, and overrides the divine recolour with a new `GetGoldTrn()` (divine_trn.cpp): every colour keeps only its luminance and is painted along a gold ramp (red leads, green at four fifths, blue near zero), nearest global-palette index, rebuilt when the palette changes. The tumble loops in ProcessMissiles, so the mace keeps turning for the whole spiral. No asset was added; the mace is diabdat's own, so no MPQ repack.

## Tests

New: the curse rolls within 2000 tries, lands in the record as `IPL_MOVESPEED_CURSE` with the magnitude, the field is -10..-20, a worn cursed ring reads below 100 and one step under the walk. The earlier drop-tail test learned to skip curse rolls while it waits for a bonus. Suite 686/686. Gold tint and the mace's look need a screenshot from the user - the cold slow has no caster to test in play yet.

# v1.11.054 - worn items measured against the sheet's strength; requirements past 127 no longer wrap

2026-09-11. The user sent a screenshot: Strength 112 (68 base), with an equipped Nightmare Great Axe of Slaying needing 112 drawn red. "something is wrong with STR checks. I have hit 112 STR but the axe is RED. Check code."

## Fault 1 - two strength rules disagreed (the red axe)

Two places judge whether the hero meets a requirement.

**`Player::CanUseItem`** decides whether an item may be picked up or equipped. It compares `_pStrength`, the sheet's Now value: base plus every bonus provider (worn items, sockets, charms, the class tree, set bonuses, Rage). It also applies Hel's reduction through `EffectiveRequirement`.

**`CalcSelfItems`** runs first in every `CalcPlrInv` and sets each worn item's `_iStatFlag`, which is what draws an item red and switches its bonuses off.
- It built its own strength: base plus only the worn, identified items' `_iPLStr`.
- It compared that against the raw `_iMinStr`, without Hel.

So the hero's strength from anywhere but worn-item affixes (44 points in the screenshot) let `CanUseItem` equip the axe, and then `CalcSelfItems` measured about 68 against 112 and switched it off.

**Fix.** `CalcSelfItems` now runs the same `oracool::AccumulateBonuses` walk that `CalcPlrItemVals` uses, and compares against `EffectiveRequirement`, exactly as `CanUseItem` does.

It is still a loop, as vanilla's was. An item that fails loses its flag. The providers already skip flag-off items (the equipment and socket walks both test `_iStatFlag`), so its bonuses, and any set bonus it completed, leave the next total, and every remaining item is measured again. A broken item still starts flag-off and never counts.

## Fault 2 - requirements past 127 wrapped (found while checking)

`_iMinStr` and `_iMinDex` were `int8_t`, while `_iMinMag` and the base item table's three columns are `uint8_t`.

The tiers scale requirements to 140% / 180% / 220% (`item_tiers.cpp`), and `ScaleByte` clamps at 255, not 127. So:
- a Hell Great Axe (80 x 180% = 144) stored -112 and had **no** strength requirement;
- its tooltip still printed 144, because the tooltip casts to `uint8_t`.

Every Hell or Torment base with a requirement above 70 was affected, and Nightmare bases above 90. The user's axe, at 112, was just under the line.

**Fix.**
- `_iMinStr` and `_iMinDex` are now `uint8_t`. The worst case, 90 x 220% = 198, fits.
- The save reads and writes the same single byte as unsigned, so a stored -112 reads back as the true 144. No format change.
- The shop's "Required:" line copies them into `uint8_t` too.

## Tests

`OracoolStatSheet.WornItemRequirementsMeetTheSheetsStrength` rebuilds the screenshot: a level-22 Barbarian with Rage (+44, standing in for any non-item source) on 68 base.
- A 112-strength axe stays usable.
- 113 is refused.
- 144 is refused, where the old signed byte made it -112 and allowed it.

The first assertion fails on the old `CalcSelfItems`.

## Verification

Debug and Release built, ctest **712/712**, RTM refreshed with exe 1.11.054. **Not seen in play.**

**To check:**
- Load the character: the Great Axe should draw normally and its damage should count.
- An item your total strength does not meet should still draw red.

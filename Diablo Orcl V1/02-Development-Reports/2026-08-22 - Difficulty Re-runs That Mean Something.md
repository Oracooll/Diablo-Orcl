# Difficulty Re-runs That Mean Something

**Version:** v1.9.15 -> v1.9.16
**Date:** 2026-08-22
**Tests:** 501/503 (the two standing baseline failures)

## Auditing the row before building it

The backlog asked for three things: "new immunities, new lesser-affix pools and new drop tiers per
difficulty, so Nightmare is not just Normal with bigger numbers". Checking each against source
before writing anything found that two were already handled:

- **Immunities** answered to the difficulty from Phase 3.3. `MonsterResistancesFor` gives Nightmare
  Hell's set with its immunities demoted, Hell the real set, and Torment hardens Hell's resistances
  into immunities.
- **Drop tiers** were done by accident but correctly: `TierForItem` keys off item level, item level
  rises with area level, and area level rises with the difficulty. A re-run already produced
  *better* items.
- **The affix pool** was not. All six champion modifiers were on the table from the first floor of
  Normal, so the only thing a re-run changed about a champion was its numbers.

And auditing turned up a fourth thing the row did not name but which is the same complaint. A
treasure class is chosen by DUNGEON TYPE, and a re-run walks the same twenty-four floors - so the
Cathedral in Torment paid exactly what the Cathedral in Normal paid. Better items, no more of them.
The socket economy in particular was no denser on the fourth run than the first.

## What shipped

| Difficulty | Variants | Loot rate | New champion modifiers |
|---|---|---|---|
| Normal | 15% | 100% | Relentless, Fortified, Colossal |
| Nightmare | 19% | 115% | Warded, Thunderous |
| Hell | 23% | 130% | Vampiric |
| Torment | 28% | 150% | - |

**The affix pool grows.** Normal offers only the three a new character can read and answer: it does
not get knocked back, it is armoured, it is large. None needs an item you may not own yet.
Resistances arrive with Nightmare, where a character has a second damage type and something to
resist with. Vampiric waits for Hell, because a champion out-healing a character is a wall rather
than a fight.

Written as a "from here on" test rather than a per-difficulty list, which is what makes it
impossible to author a rung that accidentally drops something the rung below had.

**Loot rate** scales every zone's treasure class as a percentage rather than as a fourth column on
every table - so re-tuning a zone does not mean re-tuning it four times, and the ladder is one list
a reader can take in. Modest on purpose: the item level already rises steeply, so half again by
Torment is enough to feel without turning the fourth run into a different economy.

**Variant rate** climbs so a deeper run is denser in encounters that are not the rank and file. It
stops at 28 for the reason recorded on the constant: past about a third, the recolour becomes the
default and the ordinary monster becomes the surprise.

## The leak the fallback would have had

`RollLesserUniqueAffix` has a fallback for when every modifier is already on the floor - it happens
once a level wants more packs than it has distinct champions. That fallback rolled `1..LAST`
directly.

Left alone, it would have handed out a Vampiric champion in Normal **only on crowded floors** - the
exact shape of a bug that never reproduces on demand. It now draws from the allowed set, and the
test rolls 4,000 times on Normal asserting every result is in Normal's own pool.

## Wiki

Three ladders, all parsed rather than typed: the variant rate from `VariantPercentFor`, the loot
scale from `DifficultyTreasureScale`, and the modifier arrivals from `ChampionAffixAllowedOn`'s own
switch, inverted into per-rung lists.

That last parse failed silently on the first run and produced four empty lists. The regex used
`[^c]*?` to cross the gap between a case group and its return - and every one of those gaps holds a
comment explaining why that rung, and every one of those comments contains the letter c. Replaced
with a negative lookahead on the next case label, which cannot be defeated by prose.

## What to look at in game

- Start a Nightmare run: champions should begin showing Warded and Thunderous, which Normal never
  offers.
- Compare a Torment Cathedral run to a Normal one - socketables should fall noticeably more often,
  not merely land at better grades.

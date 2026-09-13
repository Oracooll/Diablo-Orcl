# Drop rates retuned, and brighter book and rune text

2026-09-13 — v1.11.125

## Why

> "we need to decrease drop chance of uniques and set item 10 fold. also - make charms of salvaging non-dropable,
> only purchcasable. reduce drop chance of runes 10 fold. increase drop chance of rares 5 fold."
>
> "increase font brightness on books and runes."

## Where the factors live, and why not in the options

The unique and rare chances also depend on options: `Unique Drop Chance Percent` and `Rare Item Drop Chance`.
`SaveOptions` writes every option back to `diablo.ini`, so a changed default would never reach an existing
install. Each factor is a constant in code, applied on top of whatever the ini says.

## Changes

| Drop | Was | Now | How |
|---|---|---|---|
| Unique | window of `uper` out of 100 (2% plain, 16% for a high-quality roll), before options | the same window, then **1 in 10** survives | `CheckUnique`: a hash of the item seed, no draw, fresh drops only |
| Named set piece | treasure class `setPercent` (3–5%) × monster bonus, out of 100 | the same number out of **1000** (0.3–0.5% on an ordinary kill) | `TrySpawnNamedSetPiece` |
| Rune | its family share of the socketable draw | the same share, then a **1-in-10** roll | `TrySpawnOracoolGem`, rune branch only |
| Rare | `QualityChancePerMille` = configured × band / 10 | band × **5**, capped at 1000 per mille | `oracool/item_tiers.cpp` |
| Charm of Salvaging | in the charm drop pool | **excluded**; Griswold and Adria still stock them (`StockSalvageCharms`) | charm walk |

- **Runes.** The weight in the treasure-class table was left alone on purpose. Shrinking it would have handed the
  rune share to gems, jewels, charms and orbs, raising all of them. The extra roll lowers runes only.
- **Uniques.** The first attempt drew `GenerateRnd(1000)` in place of `GenerateRnd(100)`. That failed
  `PackTest.UnPackItem_*`: those tests rebuild reference items from their seeds, and a different draw rebuilt
  different items ("Rusted Curse" became "Demonspike Coat"). The shipped version keeps the draw byte-identical and
  lets one ticket in ten through, chosen by a hash of the seed (no random draw), on fresh drops only. This is the
  same trade the Unique Drop Chance Percent option already makes. Single-player loads read full records, so saved
  items are unaffected. Quest uniques (`IMISC_UNIQUE`) do not use the roll.
- **Tier pieces.** The worn-tier pieces from `TrySpawnOracoolSetItem` are plain tiered gear, not set items, and
  are unchanged.

## Books and runes

`ColorGold6` (books) and `ColorOrange7` (runes, and gems, which share it) start **two shades higher** on their own
ramps: gold from `DDC47E`, orange from `E7B37E`. The hue is the same and the text is lighter.

## Tests

`OracoolAudit.RaresRollFiveTimesAsOftenAndCapAtCertainty` pins the factor, shows buffed unique and primal did not
move, and checks the cap. The existing treasure-class and colour tests still hold.

## For the user to look at

- Uniques and set pieces should now be rare events.
- Rares should be common.
- Runes are thin, and no Charm of Salvaging drops.
- Book and rune names on the floor and in tooltips read lighter.

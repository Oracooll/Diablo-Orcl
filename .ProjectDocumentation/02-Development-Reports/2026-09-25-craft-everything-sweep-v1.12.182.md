# Craft every item, check every tooltip — v1.12.178 → v1.12.182

2026-09-25

> imagine you are a player and you have all possible crafting material to craft all possible items and
> recipes in the game. I want you to craft every single possible item in the game and double check if it
> has all the necessary rows of text according to the artefact we just made.
> — "remove any trace of prefix/sufix segregation. all afixes are now one pool."

## The sweep

`test/oracool_tooltip_sweep_test.cpp` (one TEST, written by a subagent, debugged here) crafts through the
game's real paths: every base (vendor route; starting gear through InitializeItem), and for every worn
base a magic gamble roll, an unidentified copy, Rare / Buffed Unique / Primal (`RetierOracoolItem`),
ethereal (`MakeItemEthereal`), Punch Sockets + a stone; every unique (`CreateUniqueVendorItem`), every set
piece (the Set shelf walked dry), every runeword rune by rune, all 28 recipes through
`TransmuteLevskiGridWith`, and Gillian's Imbue / Remove / Reroll. Each tooltip is captured as the
backpack hover builds it and checked against the Orcl Item Tooltips page's rules. Report:
`tooltip_sweep_report.md` (+ `_all.md`) beside the test. Final run: **4,148 items, 0 failures**, and the shuffled three-repeat run clean
(WARNs only: long rows, legitimate "-1" values).

Getting it to run found two crashes in the game (both real, both fixed):
- `GenerateStaffName` / `GenerateStaffNameMagical` / the magic name fallback handed a null `iSName` to the
  translator when a name ran long — the starting staves have no short name.
- `CalculateToHitBonus` app_fatal'd on any value but vanilla's eleven keys; a rerolled "King's" /
  "Warrior's" item replays its ROLLED percent. Brackets now.

(Test side: `ControlMode` exported with `DVL_API_FOR_TEST` — `DetectInputMethod` resets the cursor; the
archives and cursor sprites are mounted; the table's empty end row skipped.)

## Tooltip bugs it found (fixed)

- To-hit-and-damage affixes (and Doppelganger) printed the item's TOTALS — a primal showed the line twice
  and folded other to-hit affixes into it. Per affix now: damage from the roll, to-hit as the residual.
- Spectral Elixir printed nothing: a "+3 to all attributes" row and the use hint.
- Set pieces printed each power from the item's accumulated field: "Mana: +35" twice for +25 and +10.
  Per power now; and IPL_LIGHT prints "{:+d}%" ("+-10% light radius" before).
- The four Crafts appended their guaranteed affix beside a rolled one of the same kind (a Safety Craft
  printed "-1 damage from enemies" twice): merged into one row now.

## Crafting bugs it found (fixed)

- **Unique-shelf items had item level 0** → Reroll Uniques did nothing; Awaken rolled a primal with no
  affixes. Stamped at the unique's own level. `RetierOracoolItem` refuses a zero-affix result and restores
  the item; `UniquesForBaseOf` keeps the item's own uid.
- **Quest-base uniques** (Cleaver, Griswold's Edge, Arkaine's Valor …, `IMISC_UNIQUE` rows) were cleared by
  Reforge/Awaken with the reagents kept: refused now (`IsQuestUniqueBase`), and every in-place recipe is
  all-or-nothing (the target is restored unless the recipe completes).
- **Gillian's reroll dropped every tiered item to magic** (tier set, then cleared by
  `ClearOracoolAffixRecord`), lost the base-tier scaling and reset durability. Fixed in
  `RebuildOracoolItemWithAffixes`.
- **53 high-tier armours were indestructible by accident** (durability 255 = `DUR_INDESTRUCTIBLE`): capped
  at 250.
- **Jewellery** admitted to the tier recipes 9–14 (user's choice).
- **Set pieces from Recast / Consecrate** used the slot word's generic base (main hand → Short Sword), so
  two-handers found no piece and crafted pieces lost their family: `BaseItemForSetPiece` now.
- **Starting gear** gets item level 1 (recipes rolled at 0 and made nothing).

## One affix pool, one affix list (user's rule)

`_iPrePower` / `_iSufPower` are gone from `Item`. Every affix on every item lives in `_iOracoolAffixes`
(type, rolled value, price multiplier) — magic items, staves, tiered items, recreated items. Consumers read
the one list: the tooltip, `OracoolAffixesUsed`, the level requirement (which ignored a magic item's
table affixes whenever it had a pool affix), the text-shop line, `AffixStatesIndestructible`. Gillian
lists and rerolls every affix of a magic item. Save layout unchanged: the two legacy bytes are written as
`IPL_INVALID` and, on load, an old item's pair is migrated into the list (value read back from the
item's own stat field). The two vanilla affix TABLES remain as the pool's data sources; nothing on an item
depends on which one an affix came from.

Doppelganger rows name their clone effect (a perfect primal carried it beside a plain to-hit-and-damage affix and the two read identically). The sweep mounts the archives once per process (the shuffled repeat crashed on a second LoadCoreArchives). Windows Device Guard intermittently blocked freshly built executables (the MPQ packer, test binaries) - retries, not code.

Debug build at v1.12.182: 834 of 835 tests in the full run; the one red, the shuffled sweep, passed its three shuffled repeats after the once-per-process fix (run directly, not yet in a full ctest).

# Auditing the wiki page by page: 60 corrections and two broken generators

**Version:** 1.11.062
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "audit the wiki menu item by menu item, page by page, for inacurate or old information and remove
> what is no longer valid/used in game as of the latest patch."

## Method

Five parallel auditors, one per group of pages, each checking every factual claim on its pages
against current source rather than against the prose's own internal logic. 28 pages, 27 audited (the
bundle is generated). The menu itself was checked first: **28 nav entries, 28 pages, exact 1:1** - no
dead links, no orphans.

## Two generator bugs - the wiki was lying, not merely dated

**Runewords parsed to nothing.** `data.js` carried `"runewords":[]` while `runewords_data.inc` holds
370. The Runewords section published as an empty table and a "0 runewords" stat, while the overview
card advertised 370. The regex's tail was `([-\d,\s]*?)\}` - digits, commas and whitespace only - and
every row carries an `ItemSpecialEffect::X` token after its first eight numbers, so the pattern could
never reach the row's closing brace and **not one row matched**. The `WARNING: PARSED NOTHING` line
had been printing on every build. Now 370 rows parse.

**Every skill claimed a cap of 98.** `BuildWiki.ps1:209` defaulted an undeclared `maxRank` to a
hardcoded 98, and stayed there when the real cap dropped to 30. The default now reads
`MaxSkillInvestment` from source.

Both are the same failure: a number typed into the generator instead of read from the game.

## The pattern behind most of the prose errors

Three changes account for the bulk of the 60 corrections, because each one invalidated claims spread
across many pages:

| Change | What it falsified |
|---|---|
| Per-skill cap 98 → **30** | "No ceiling", "nothing is capped", "the entire pool", "rank 94 at level 99", the Rule of Rangs card |
| The ladder 96 → **64 rungs**, tiers per **12** | "one tier per 24-level block", "the 24-area ladder", "1-60 area scale", vendor ilvls of 78-88 |
| Palette green **removed** at v1.11.023 | the colours lede's ".trn recipes", the palettes green-minis row, an engine row describing live injection |

## Corrections by page

- **index** - "96-level ladder" card, "three pages each", "24-area ladder", "No level ceiling",
  six-slot belt, the `areas: 24` stat relabelled to dungeon floors.
- **start** - Abilities key **S → D**, belt keys **1-6 → 1-4**, "the other three" sheets → four,
  "24-floor block" → 16 rungs, "Nothing is capped" → 30, belt six slots → four plus Menu and Town
  Portal.
- **mechanics** - the "No ceiling" note rewritten (the cap *binds*: 30 per skill against a 98-point
  lifetime), "flat cap of 20" claim reversed, one tier per 12 rungs, points no longer spendable on
  spells, and two live rules that were documented nowhere: **spells never miss** and **bows are
  two-handed**.
- **classes** - four-page tree, the "whole pool" figure was printing `maxInvestment` (30) where it
  meant the 98-point lifetime; now prints both.
- **skills** - four pages, the tier-7 example, "retired from the tree" → listed but unspendable, and
  the **Sorcerer note deleted**: it claimed 13 inert rows and "no cold damage type at all", while the
  data shows 34 of her 48 rows built and Holy Freeze dealing cold damage.
- **spells** - "rank 94 at level 99" and "no cap" → ranks cap at 30.
- **items** - "1-60 area scale" → 1-48, the hook-only family list completed, the save-format claim
  corrected (it is the multiplayer wire format), rings and amulets **do** take a tier.
- **tiers** - vendor ilvls 30-40/54-64/**78-88** were impossible on a 64-rung ladder → 22-32/38-48/
  54-64, and the highest-tier column rebuilt for the 12-rung bands.
- **affixes** - uniques 143 → **250**; basic is not the only quality that sockets (jewellery excepted).
- **sets** - drop rate 3% → 3-5% scaled by champion/unique; two of the three "known gaps" are closed.
- **uniques** - unique-monster chance 15% → **~8%** effective; "minimum level" is the item's affix
  base level, not a character requirement.
- **sockets** - "Sixty-one" D2 runewords → fifty-nine; rune span 1-48 → 3-48; "Seven" effects → eight.
- **areas** - book bands completed to all six, named-encounter arenas, the "#" column renamed so it
  cannot be read as a rung.
- **monsters** - endgame boss "first Hell floor" → **area level 33**, and "first third" → area level
  10 (two thirds through a block).
- **world** - vendor level is deepest floor **+2**; added the gold "!" quest marker (v1.11.059).
- **salvage** - a full pack is now all-or-nothing, not "if nothing fits".
- **ui** - Abilities key → D, four class-tree pages, belt slots, plate colours rewritten for
  v1.11.056 (gold / grey / green / red), "pink" → red.
- **controls** - Abilities **D** with A/S as the skill lists, belt 1-4, the reserved-letter list
  gained A and D, Screenshot is bound by default (Print Screen), and the Abilities rows now describe
  spending points rather than readying skills.
- **colours / palettes / engine** - the ".trn recipe" model replaced with 16 RGB values in
  `RgbDefinedColors`; the green-minis row corrected to vanilla orange; the injected-ramp engine row
  marked as removed at v1.11.023.
- **debug** - duplicate `hideui / clearui` card removed.

## Verification

Every page's inline script passes `node --check`; the generator reports 370 runewords and zero
`maxRank:98`. **The rendered page was not verified by eye** - the in-app browser cannot open
`file://` and is not signed in for the private artifact - so the user should look.

## Still open

- **sets.html** groups by each piece's own enum, so `WIKI.setItems` yields 94 one-piece "sets"
  instead of the real families; the heading still says fifteen. This is a generator fix, not a prose
  fix, and was left rather than half-done.
- **colours.html** lists 23 colours where `ui_flags.hpp` defines ~31, and `ColorMagicDamage` is
  missing entirely - regenerate with `tools/BuildFontColourLegend.pl`.
- **assets.html** describes runtime palette quantisation that is now only the 8-bit fallback path.
- Two **source comments** are themselves stale: `oracool/area_level.h:44-45` ("the deepest base needs
  alvl 51") and `item_tiers.cpp:47` ("At ilvl 1-24 only Normal is available").

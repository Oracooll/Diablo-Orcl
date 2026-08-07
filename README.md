# Diablo Oracool Edition

A single-player-focused overhaul of Diablo built on top of [DevilutionX](https://github.com/diasurgical/devilutionX) 1.5.5. Oracool Edition keeps the original game's feel intact while adding three new item quality tiers, a much larger inventory, a fourth difficulty above Hell, a level cap of 99, and dozens of smaller quality-of-life fixes — all configurable, most on by default, none of it touching multiplayer.

[![Latest release](https://img.shields.io/github/v/release/Oracooll/Diablo-Oracool-Edition?label=latest%20release)](https://github.com/Oracooll/Diablo-Oracool-Edition/releases/latest)
[![License](https://img.shields.io/badge/license-Sustainable%20Use-blue)](LICENSE.md)

> **Note:** this is a personal single-player mod, not an official DevilutionX release. If you're looking for the base engine project this is built on, see [diasurgical/devilutionX](https://github.com/diasurgical/devilutionX).

---

## What is this

[DevilutionX](https://github.com/diasurgical/devilutionX) is a reverse-engineered, cross-platform port of the original Diablo and Hellfire that fixes bugs and adds engine-level improvements while keeping the original game byte-for-byte faithful. Oracool Edition starts from that foundation and builds a substantial single-player content and quality-of-life layer on top of it: new item tiers, a bigger inventory, extra difficulty, a higher level cap, and a long tail of UI and bugfix polish accumulated over more than seventy dated feature entries.

Everything Oracool Edition adds is single-player-only by construction — multiplayer behaves exactly like vanilla DevilutionX, with no risk of desync or unfair advantage. Almost every feature that can reasonably be made optional has its own on/off switch in `diablo.ini`, under an `[Oracool Edition]` section, so you can keep as much or as little of this mod's behavior as you want.

## Key features

### New item tiers

Three procedurally generated item quality tiers sit between Magic and vanilla Unique in power, built on the game's own affix system:

- **Rare** (yellow) — up to two prefixes and two suffixes (always at least one of each).
- **Buffed Unique** (gold, displays as `Unique {name}` to blend in with real Uniques) — two to three prefixes and suffixes each.
- **Primal** (orange) — always exactly three prefixes and three suffixes, every one of them rolled at its maximum possible value, and always full durability.

Each tier has its own configurable drop chance, follows the same "Auto Identify Drops" setting as everything else, and gets its own inventory-slot background color so it's recognizable at a glance.

### Tabbed Inventory

Your backpack grows from 1 page to 10. Tab 1 is your original backpack, unchanged; tabs 2–10 are full-size extra storage pages, numbered with roman numerals. Every interaction — placing, stacking, equipping, selling, identifying, repairing, reading a book — works in an extra tab exactly like it does in your main backpack, and everything you store there saves inside your one existing character save file.

### Torment difficulty & level cap 99

A fourth difficulty above Hell, with an adjustable multiplier (1.1x–5.0x) that scales Hell's own monster and treasure formulas further, plus an optional level-gate (15/30/40) for Nightmare/Hell/Torment. The character level cap is raised from 50 to 99, with a new experience curve for the extra levels.

### Stackable Consumables & Belt Mod

Potions, elixirs, scrolls, books, and oils stack up to 99 per slot instead of eating one slot each. Belt slots go further with Belt Mod: each slot holds its own physical stock and automatically refills itself from a matching inventory stack once emptied, instead of going empty after a single use.

### HUD additions

- **Event Log** — a collapsible, timestamped log of session events: saves, boss/unique kills, tiered/Unique/Quest item drops, deaths, shrine effects, quest-log additions, and level-ups.
- **Mini-map** — an always-on corner map (independent of the full map, which still works exactly like vanilla via TAB).
- **Game Clock** — a real-world clock, 12- or 24-hour.
- **XP Counter** — experience needed for your next level; press and hold to see the total remaining monster XP on the level instead.
- **Monster Range Highlight** — nearby monsters get a red outline before they're even on screen, at an adjustable range.

### Griswold enhancements

A "Buy unique items" shop (an independent, non-restocking stock of identified Uniques), a Premium refresh service, and "Repair all"/"Sell all" buttons that batch-process your whole inventory in one click.

### Quality of life

Local portable saves (`diablo.ini` and `Saved_Games` live beside the executable, so the whole install is copy-anywhere portable), automatic saving, auto-identify and configurable-radius auto-pickup, a raised gold stack cap, Respawn In Town (keep all your gear when you die), broken items going inactive instead of being destroyed, an inventory sort button, and a large number of smaller fixes and polish passes documented in full below.

For the complete, dated history of every feature and fix — including exactly which ones are on by default and which need to be enabled — see [`CHANGELOG.md`](_ProjectLibrary/Documentation/CHANGELOG.md) and the more technical [`Gameplay-Changes.md`](_ProjectLibrary/Documentation/Gameplay-Changes.md).

## Installation

1. You'll need the original game data. If you don't own the game, you can [buy Diablo on GoG.com](https://www.gog.com/game/diablo), or use `spawn.mpq` from the shareware version in place of `DIABDAT.MPQ` to play the shareware portion for free.
2. Download the latest release from the [Releases page](https://github.com/Oracooll/Diablo-Oracool-Edition/releases/latest) and extract it into its own folder.
3. Copy `DIABDAT.MPQ` from your CD or GoG installation into that same folder.
4. To play the Hellfire expansion, also copy `hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq`, and `hfvoice.mpq` into the folder.
5. Run `devilutionx.exe`. The title screen should show `DevilutionX 1.5.5` above `Oracool Edition vX.X.X` as two separate lines.

Every release's bundled README documents that specific version's save compatibility with earlier characters — check it before loading an existing save into a new release, since some feature-driven format changes require starting a fresh character.

## Configuration

Settings live in `diablo.ini`, in a dedicated `[Oracool Edition]` section created automatically the first time you run the game. Most features are on by default; a handful of especially foundational ones (uncapped stats, Respawn In Town, Stackable Consumables, Belt Mod, Tabbed Inventory, Torment difficulty, the level 99 cap, and a few others) are permanent parts of the mod and no longer have a toggle at all. Everything else — drop chances, HUD elements, auto-pickup behavior, and more — stays configurable.

## Building from source

Oracool Edition builds exactly the way DevilutionX itself does — the engine, build system, and platform support are untouched. See DevilutionX's own [build instructions](https://github.com/diasurgical/devilutionX/wiki) for your platform; the only difference is cloning this repository's `oracool-main` branch instead of upstream.

```bash
git clone --branch oracool-main https://github.com/Oracooll/Diablo-Oracool-Edition.git
```

## Credits

Diablo Oracool Edition is a derivative work built on [DevilutionX](https://github.com/diasurgical/devilutionX), originally the [Devilution](https://github.com/diasurgical/devilution#credits) project, by the [Diasurgical](https://github.com/diasurgical) team and its [many contributors](https://github.com/diasurgical/devilutionX/graphs/contributors) — including [Nikolay Popov](https://www.instagram.com/nikolaypopovz/) for UI and graphics work reused here. All the engine work, platform support, and countless bugfixes this mod builds on belong to that project; Oracool Edition adds a single-player content and quality-of-life layer on top of it.

## Legal

Diablo Oracool Edition, like the DevilutionX base it's built on, is released under the Sustainable Use License (see [LICENSE.md](LICENSE.md)). The source code in this repository is for non-commercial use only — you may not charge others for access to it or any derivative work.

Diablo® – Copyright © 1996 Blizzard Entertainment, Inc. All rights reserved. Diablo and Blizzard Entertainment are trademarks or registered trademarks of Blizzard Entertainment, Inc. in the U.S. and/or other countries. This project does not include or distribute any of Blizzard's original game data, and neither DevilutionX nor Diablo Oracool Edition are associated with or endorsed by Blizzard Entertainment®.

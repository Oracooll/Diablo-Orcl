# Diablo Orcl — Oracool Edition

A single-player overhaul of Diablo and Hellfire built on [DevilutionX](https://github.com/diasurgical/devilutionX). Diablo Orcl expands character builds, loot, crafting, endgame progression and the interface, with a 32-bit colour renderer.

**Project still in development. Bugs are common and come in hordes :)**

[Website](https://diabloorcl.oracooll.com/) · [Downloads](https://github.com/Oracooll/Diablo-Orcl/releases) · [Report a bug](https://github.com/Oracooll/Diablo-Orcl/issues) · [License](LICENSE.md)

## Current status

- The default development branch is `renderer-32bit`.
- Current source version: **v1.12.347** as of 2 October 2026; [ORACOOL_VERSION](ORACOOL_VERSION) is the authoritative version file.
- Latest published Windows download: **[v1.12.347](https://github.com/Oracooll/Diablo-Orcl/releases/tag/v1.12.347)**, with `DiabloOrcl-v1.12.347-win64.zip`. This matches the current source version. Read the release notes before installing or upgrading.
- **V1 is single-player only.** Multiplayer is not supported.
- Windows is the primary development platform. Upstream platform instructions do not guarantee that this fork has a supported package for every platform.
- Save compatibility can change between development builds. Back up characters before upgrading.

## Heroes and skills

Six classes: **Paladin, Rogue, Sorcerer, Monk, Barbarian and Necromancer**. The Paladin replaces the Warrior; the Barbarian and Necromancer have their own class systems. The Sorcerer is male.

The current class-tree definitions contain **303 skill entries** across seven tiers. Skill ranks grow their effects, and tooltips explain bonuses. The Barbarian uses Rage instead of mana; the Necromancer uses Essence and has summons, curses and bone skills. Paladin auras appear on the ground. Summoned companions include the Valkyrie, Korlic, Talic, Madawc, Spirit Guardian and Decoy. Passive slots and F-key cast slots support character builds.

Cold joins fire, lightning and magic as a damage element, and Strength scales physical damage as a percentage.

## Items, crafting and town

- **256 named uniques**: 250 expansion uniques and six Necromancer uniques; **94 set pieces across 15 sets**, with bonuses for equipped pieces.
- A shared affix pool constrained by item level, class-weighted Smart Loot, and monster treasure classes.
- Sockets, **33 runes**, **370 runeword definitions**, a runeword book, gems and jewels.
- Inventory charms, including growing charms; **24 Imbuement Shard kinds**; ethereal equipment, item tiers and oils; drop-only Signets and Sealed Maps.
- Levski's Cube, including reroll, Recast and Consecrate recipes; Gillian and Ogden's crafting workshops; Griswold salvage; Wirt's unidentified-item gambling shop.
- Vendor grids and tabs, a ten-page backpack and a shared stash.

Counts describe the current source definitions and can change as development continues; they do not imply that an older downloadable package includes every current feature.

## Progression and interface

Torment is the fourth difficulty above Hell. Rifts reached through the Rift Monument have kill progress and tiers; keystones open Guardian Rifts. Waypoints cover the Diablo, Hellfire and Orcl Acts. Monster variants, named encounters and endgame bosses sit within a 64-step area difficulty and loot ladder.

The interface includes a redesigned hero sheet, advanced stats, item tooltip cards, event log, XP bar and counter. Equipment changes the hero's appearance, including shields, and dual wielding is supported. Keyboard and gamepad navigation, buttons that act on release, and Escape closing the top window support the new panels.

The game auto-saves. On Windows, saves are in `Saved_Games` beside the executable and settings are in `diablo.ini` beside it. Options include auto-identify, game speed, unique drop multiplier, unlocked town entrances and the trial Spells Never Miss setting. Consult the current in-game settings for availability and defaults.

## Download and install

1. Choose a package from [this project's Releases page](https://github.com/Oracooll/Diablo-Orcl/releases). Read its release notes; the latest development source may be newer than the latest download.
2. Extract the entire package into its own writable folder. Keep its DLLs, mod archive and engine assets together.
3. Supply your own **`diabdat.mpq`** from an owned Diablo installation beside `DiabloOrcl.exe`.
4. For the full Hellfire content and Monk assets, also supply **`hellfire.mpq`, `hfmonk.mpq`, `hfmusic.mpq` and `hfvoice.mpq`** from an owned Hellfire installation. Use the complete four-file set when enabling Hellfire.
5. Run `DiabloOrcl.exe` and check the displayed version. Older packages may have different instructions; follow their bundled README.

**No original Diablo or Hellfire game-data archives are distributed by this project. Players must supply their own.** Keep `oracool.mpq` and the packaged DevilutionX assets (`devilutionx.mpq` or the bundled `assets` folder), as well as the supplied libraries. Do not mix packages from different versions.

Back up `Saved_Games` and `diablo.ini` before replacing a build. Do not assume that older Oracool Edition or development saves remain compatible.

## Building from source

Clone the current branch:

```sh
git clone --branch renderer-32bit https://github.com/Oracooll/Diablo-Orcl.git
cd Diablo-Orcl
```

The project uses CMake and requests C++20. See the [upstream build guide](https://github.com/diasurgical/devilutionX/wiki) for toolchain and dependency setup, then use this fork's [CMakeLists.txt](CMakeLists.txt) and [CMakeSettings.json](CMakeSettings.json). The checked-in Windows settings contain machine-specific toolchain paths that need adjusting on another machine.

For an already configured Windows Release build:

```powershell
cmake --build build/x64-Release --target devilutionx --parallel
tools\build_oracool_mpq.cmd build\x64-Release
powershell -File tools\BuildReleasePackage.ps1
```

[BuildReleasePackage.ps1](tools/BuildReleasePackage.ps1) checks package contents and version consistency. Building an executable alone does not assemble the required mod and engine assets. Original game archives remain user-supplied.

## Documentation and contributions

- [Current project scope](.ProjectDocumentation/01-Project-Overview/Project-Scope.md)
- [Documentation home](.ProjectDocumentation/Home.md)
- [Development reports](.ProjectDocumentation/02-Development-Reports) and [v1.12.347 report](.ProjectDocumentation/02-Development-Reports/2026-10-02-open-list-fixed-v1.12.347.md)
- [Historical changelog](.ProjectDocumentation/04-Changelog/CHANGELOG.md)
- [Contribution guide](docs/CONTRIBUTING.md)

Dated reports, old release notes and archived feature plans describe their own point in development. They are not a promise of current behaviour. The documentation vault uses Obsidian wikilinks; use the folder listings on GitHub when a wikilink does not resolve there.

Report bugs through [GitHub Issues](https://github.com/Oracooll/Diablo-Orcl/issues), with the version, reproduction steps, expected and actual behaviour, and a screenshot or error message where useful.

## Credits and license

- **Claude — Lead Programmer**
- **ChatGPT — Lead Artist**
- **Oracooll — Coordinator**
- **DevilutionX Team — Fundamental mod and engine foundation**
- **Blizzard North — Creators of the original Diablo**


Diablo Orcl builds on DevilutionX and Devilution by the Diasurgical team and their contributors. Credit also belongs to the artists and contributors recorded in the repository and upstream project.

See [LICENSE.md](LICENSE.md) for the Sustainable Use License and applicable terms. Existing dependency and asset notices remain applicable.

Diablo and Hellfire, their original assets and related trademarks belong to Blizzard Entertainment. This independent, non-commercial fan project is not endorsed by Blizzard or an official DevilutionX release.

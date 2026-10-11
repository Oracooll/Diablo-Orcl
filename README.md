# Diablo Orcl — Oracool Edition

A free, non-profit, single-player overhaul of Diablo and Hellfire built on [DevilutionX](https://github.com/diasurgical/devilutionX). Diablo Orcl expands character builds, loot, crafting, endgame progression and the interface. Version 1.13 opens the era of next-gen Diablo graphics: coloured and per-pixel lighting, shadows, bloom and a graphics-card renderer.

**Project still in development. Bugs are common and come in hordes :)**

[Website](https://diabloorcl.oracooll.com/) · [Downloads](https://github.com/Oracooll/Diablo-Orcl/releases) · [Trailers](https://diabloorcl.oracooll.com/media/) · [Interface tour](https://diabloorcl.oracooll.com/interface/) · [Report a bug](https://github.com/Oracooll/Diablo-Orcl/issues) · [License](LICENSE.md)

## Trailers

| [![The Paladin trailer](https://diabloorcl.oracooll.com/assets/trailer-paladin.jpg)](https://youtu.be/yExCXtGM7PQ) | [![The Barbarian trailer](https://diabloorcl.oracooll.com/assets/trailer-barbarian.jpg)](https://youtu.be/8cknWYUSkcU) |
| :---: | :---: |
| **[The Paladin](https://youtu.be/yExCXtGM7PQ)**: hero creation, rift combat under the v1.13 lighting and a tour of the town's interface | **[The Barbarian](https://youtu.be/8cknWYUSkcU)**: Torment tiers, the Guardian Rift, Whirlwind and the fight with Na-Krul |

## Current status

- The default development branch is `renderer-32bit`.
- Current source version: **v1.13.004** as of 11 October 2026; [ORACOOL_VERSION](ORACOOL_VERSION) is the authoritative version file.
- Latest published Windows download: **[v1.13.004](https://github.com/Oracooll/Diablo-Orcl/releases/tag/v1.13.004)**, with `DiabloOrcl-v1.13.004-win64.zip`. This matches the current source version. Read the release notes before installing or upgrading.
- **V1 is single-player only.** Multiplayer is not supported.
- Windows is the primary development platform. Upstream platform instructions do not guarantee that this fork has a supported package for every platform.
- Save compatibility can change between development builds. Back up characters before upgrading.

## Next-gen graphics (v1.13)

Every effect has its own setting under **Settings → Oracool Edition**, and all are on by default.

- **Light and shadow:** coloured lighting (fire orange, lightning blue-white, holy gold, poison green, cold cyan), smooth light falloff, ray-traced shadows from walls and pillars, torch flicker, and on the graphics card per-pixel lights and lit sprites.
- **Atmosphere:** spell glow and a wide bloom, light shafts, heat distortion over lava and fire, a colour grade for each area, a vignette, particles, and lava haze and water light.
- **A living world:** a 20-minute day and night in Tristram, floor marks after a fight, loot beams under magic and better items, smooth animation between frames, and a pixel-art zoom with adaptive sharpening.
- **The graphics-card renderer:** everything drawn after the figures runs through Direct3D 11 on any Direct3D 11 card (Intel HD 4000, GeForce 400, Radeon HD 5000 or newer, and every Snapdragon X). Without one, the game falls back to the CPU on its own, and the picture is the same apart from the card-only effects. With **Show FPS** on, the game shows what each effect costs.

| ![The skill tree in a rift](https://diabloorcl.oracooll.com/assets/shots/skill-tree.jpg) | ![The Barbarian against Na-Krul](https://diabloorcl.oracooll.com/assets/shots/nakrul.jpg) | ![Torment tiers](https://diabloorcl.oracooll.com/assets/shots/torment-tiers.jpg) |
| :---: | :---: | :---: |
| Class skill tree | Na-Krul, guardian of the rift | Torment and its eight tiers |
| ![Griswold's shop](https://diabloorcl.oracooll.com/assets/shots/vendor-tabs.jpg) | ![The hero sheet](https://diabloorcl.oracooll.com/assets/shots/hero-sheet.jpg) | ![The Runeword Book](https://diabloorcl.oracooll.com/assets/shots/runeword-book.jpg) |
| Grid shops with tabs | Hero sheet and advanced stats | The Runeword Book |

More on the website's [interface tour](https://diabloorcl.oracooll.com/interface/) and [media page](https://diabloorcl.oracooll.com/media/).

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
- [Development reports](.ProjectDocumentation/02-Development-Reports), the latest being the [graphics-card renderer report](.ProjectDocumentation/02-Development-Reports/2026-10-11-gpu-renderer-stages1-3-v1.12.434-439.md)
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

### Thanks

Diablo Orcl would not exist without these people and projects:

- **Blizzard Entertainment and Blizzard North**, for creating Diablo and giving the world the Diablo franchise.
- **Synergistic Software and Sierra On-Line**, for Diablo: Hellfire, the 1997 expansion.
- **The DevilutionX team and contributors**, for the open-source Diablo engine this mod is built on.
- **The teams behind the Belzebub and Tchernobog mods**, for showing how far Diablo 1 can be taken.
- **The Median XL team**, for setting the standard for deep Diablo mods.
- **The DamnationX team**, for inspiration.
- **Claude (Anthropic)**, for writing the code and recommending which new features this project could implement.
- **ChatGPT (OpenAI)**, for creating the visual assets.
- **Cloudflare**, for hosting the website, and **GitHub**, for hosting the source code and releases.
- **The Paint.NET and Notepad++ teams**, for the image and text editors.
- **The FFmpeg project**, for the tools the trailers are cut with.
- **SDL, CMake** and the other open-source libraries and tools the game is built with.
- And everyone in the Diablo modding community who has kept this game alive for almost thirty years.

Diablo Orcl is free and non-profit; neither the mod nor its videos are monetised.

See [LICENSE.md](LICENSE.md) for the Sustainable Use License and applicable terms. Existing dependency and asset notices remain applicable.

Diablo and Hellfire, their original assets and related trademarks belong to Blizzard Entertainment. This independent, non-commercial fan project is not endorsed by Blizzard or an official DevilutionX release.

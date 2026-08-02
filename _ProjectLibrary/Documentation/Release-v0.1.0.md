# Diablo Oracool Edition v0.1.0 Release Record

## Identity

- Oracool release: `v0.1.0`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-02
- Source branch: `oracool-main`
- Feature baseline commit: `9c52d0c`
- Build type: optimized Windows x64 Release

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.1.0` number identifies this Oracool Edition preview release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.1.0.zip`
- Size: 4,212,131 bytes
- SHA-256: `692B38E1A127A0BC59166478B6864915F2BE7BE6228E6E58D6B85CC07559B858`
- Verified files: 15

Every file in the ZIP was compared byte-for-byte by SHA-256 with its staged
Release-build source. The executable dependency list was also checked to ensure
the required adjacent libraries were included.

## Package contents

The archive contains `devilutionx.exe`, its eight required distributable
libraries, installation instructions, and project/upstream license notices.
Files are placed at the ZIP root so the archive can be extracted directly into
a vanilla DevilutionX 1.5.5 folder.

The archive intentionally excludes:

- Blizzard MPQ game data;
- `diablo.ini` and personal settings;
- `Saved_Games` and character data;
- debug symbols, test programs, build intermediates, and source files.

## Release scope

This preview captures the accepted 1.5.4-to-1.5.5 migration, Gold Stacks Buff,
and OE-007 fixed Pepin potions in Griswold's Consumables store. Detailed
behavior and test status remain authoritative in `Feature-Catalogue.md`,
`Gameplay-Changes.md`, and `Testing-Guide.md`.

## Installation

Back up the destination folder, then extract the ZIP directly into a working
vanilla DevilutionX 1.5.5 folder and allow replacement of same-named files.
Run `devilutionx.exe`; the title should identify Diablo Oracool Edition based on
version 1.5.5.

This is a development preview, so important character files should remain
backed up.

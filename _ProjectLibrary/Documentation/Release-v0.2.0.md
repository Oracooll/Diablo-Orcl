# Diablo Oracool Edition v0.2.0 Release Record

## Identity

- Oracool release: `v0.2.0`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-04
- Source branch: `oracool-main`
- Feature baseline commit: `d696616`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.2.0` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.2.0.zip`
- Size: 4,227,205 bytes
- SHA-256: `805FEAA0490B2A066703B325335A791EACDE7014B7D437FE594DDABF4466BA65`
- Verified files: 15

Every file in the ZIP was compared byte-for-byte by SHA-256 with its staged
Release-build source. The executable dependency list was also checked to
ensure the required adjacent libraries were included, matching the v0.1.0
package's 8 distributable libraries plus `devilutionx.exe`.

## Package contents

The archive contains `devilutionx.exe`, its eight required distributable
libraries, installation instructions, and project/upstream license notices.
Files are placed at the ZIP root so the archive can be extracted directly
into a vanilla DevilutionX 1.5.5 folder.

The archive intentionally excludes:

- Blizzard MPQ game data;
- `diablo.ini` and personal settings;
- `Saved_Games` and character data;
- debug symbols, test programs, build intermediates, and source files.

## Release scope

This is a **save-breaking** foundations/polish pass across every
Oracool-specific modification made to date. Headline changes:

- Item tier/affix data (Rare, Buffed Unique, Primal) is now folded directly
  into the shared `SaveItem`/`LoadItemData` format instead of a separate
  sidecar file, closing the root cause of the entire v0.1.11-v0.1.13 bug
  streak. A pre-v0.2.0 save is rejected cleanly with an "incompatible
  version" message rather than risking silent item loss or corruption.
- Gold Stacks Buff's cap raised from 65,535 to 100,000,000; `UiFlags`
  widened from 32 to 64 bits.
- Griswold's empty "Buy Basic Items" screen no longer bounces the player
  back to the store menu; Griswold's and the Witch's sell-list logic is
  unified into one shared helper, so the Witch's sell list now sorts by
  price too.
- A real item-duplication bug in the two-handed-weapon unequip path fixed.
- 11 of the 37 Oracool options (uncapped stats, Respawn In Town, Gold
  Stacks Buff, Stackable Consumables, Belt Mod, Tabbed Inventory, free Town
  Portal, and four Griswold services) are now permanent, non-optional parts
  of Oracool Edition - their INI toggles have been removed. The remaining
  26 stay fully configurable.

Detailed behavior and test status remain authoritative in
`Gameplay-Changes.md` (see OE-018) and `Testing-Guide.md`.

## Installation

Back up the destination folder - especially any existing `Saved_Games`, as
old characters cannot be loaded by this version - then extract the ZIP
directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition based on version 1.5.5.

This is a development preview, so important character files should remain
backed up.

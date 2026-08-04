# Diablo Oracool Edition v0.2.1 Release Record

## Identity

- Oracool release: `v0.2.1`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-04
- Source branch: `oracool-main`
- Feature baseline commit: `523b563`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.2.1` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.2.1.zip`
- Size: 4,227,117 bytes
- SHA-256: `1F2DBDD881B91DB904CF2B65626A63948B8BF85C9DA9ADE3F3ACE46239940E06`
- Verified files: 15

Every file in the ZIP was compared byte-for-byte by SHA-256 with its staged
Release-build source, matching the v0.2.0/v0.1.0 package structure exactly
(devilutionx.exe, 8 distributable libraries, license notices, README).

## Release scope

This is a small follow-up to v0.2.0 (OE-018), requested by the user
immediately after confirming v0.2.0 worked. It does **not** change the save
format - v0.2.0 saves load into v0.2.1 without any issue.

- OE-019: the six vanilla potion auto-pickup options (Heal, Full Heal, Mana,
  Full Mana, Rejuvenation, Full Rejuvenation) each stopped auto-picking up
  that potion type once 16 were carried - a vanilla ceiling that no longer
  made sense now that Stackable Consumables (always-on since v0.2.0) lets a
  single slot hold up to 99. Converted to plain on/off switches, matching
  how Elixir/Oil auto-pickup already worked. Old numeric settings migrate
  automatically: any previous nonzero cap becomes "on," `0` stays "off."

Detailed behavior and test status remain authoritative in
`Gameplay-Changes.md` (see OE-019) and `Testing-Guide.md`.

## Installation

Extract directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition v0.2.1 based on DevilutionX 1.5.5. Save
compatibility notes are unchanged from v0.2.0 - see `Release-v0.2.0.md`.

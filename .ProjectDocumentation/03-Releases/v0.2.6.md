# Diablo Oracool Edition v0.2.6 Release Record

## Identity

- Oracool release: `v0.2.6`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-05
- Source branch: `oracool-main`
- Feature baseline commit: `f0bce1bc5ba722c0abbaaaac52782ee56f9baf2c`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.2.6` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.2.6.zip`
- Size: 4,228,524 bytes
- SHA-256: `BE464B541AC667AABEB7DDB4FF553016692904E6C95F7ED2CFDAC62763CF86B9`
- Verified files: 15

Every file in the ZIP was compared byte-for-byte (SHA-256) against its
staged Release-build source, matching the v0.2.1/v0.2.0/v0.1.0 package
structure exactly (devilutionx.exe, 8 distributable libraries, license
notices, README).

## Release scope

This is the first published release since v0.2.1 - versions v0.2.2 through
v0.2.5 were built and merged but never individually packaged/tagged, so this
release's README and notes summarize everything shipped across all of them
plus the new work in this batch (OE-024).

**Not save-compatible with v0.2.5 or earlier** - the durability feature
below grew the per-item save record by one byte. This continues the same
save-format break v0.2.0 already made against v0.1.x.

- OE-020 (v0.2.2): curated default settings profile for new installs; fixed
  a latent item-reconstruction bug that could flip Magic items to Unique.
- OE-021 (v0.2.3): fixed potions/scrolls silently refusing to stack with
  vendor-bought or starting-gear equivalents, due to a vanilla duplicate-ID
  quirk.
- OE-022 (v0.2.4): repositioned the Reset Stats button next to the points
  value it affects, changed its icon; gave Rare items their own yellow
  background instead of sharing Magic's blue.
- OE-023 (v0.2.5): added scroll auto-pickup with its own toggle; Griswold's
  Repair list now sorts by cost, highest first.
- OE-024 (v0.2.6, this release): items at 0 durability go inactive instead
  of being destroyed, repairable back to normal; Reset Stats reworked to
  only remove manually-spent points, preserving quest/shrine bonuses; fixed
  Ctrl+Click not sending extra-tab items to the Stash; fixed The Butcher's
  Cleaver being unsellable due to a quest-ID-range quirk.

Detailed behavior and test status remain authoritative in
`Gameplay-Changes.md` (see OE-020 through OE-024) and `Testing-Guide.md`.
Automated regression: 180 tests across 7 suites, 100% passing at release
time.

## Installation

Extract directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition v0.2.6 based on DevilutionX 1.5.5. See the
README in the ZIP for full save-compatibility notes.

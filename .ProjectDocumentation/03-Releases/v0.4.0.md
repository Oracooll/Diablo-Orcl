# Diablo Oracool Edition v0.4.0 Release Record

## Identity

- Oracool release: `v0.4.0`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-07
- Source branch: `oracool-main`
- Feature baseline commit: `173b8b7`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.4.0` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.4.0.zip`
- Size: 4,154,856 bytes
- SHA-256: `7a768706954b481dfc7793dbe01254012b774234f27b56dccaa6f24d24159ad2`
- Verified files: 15

Package structure matches the v0.3.17/v0.3.0/v0.2.x/v0.1.0 packages exactly
(devilutionx.exe, 8 distributable libraries, license notices, README). The
8 libraries were reused byte-for-byte from the v0.3.17 package (unchanged
since that release); only `devilutionx.exe` and the README are new for
v0.4.0.

## Release scope

Covers every change shipped between v0.3.17 (the previous packaged GitHub
release) and v0.3.57 (the version immediately preceding this one) —
roughly 40 incremental version bumps, none of them individually packaged.
Full detail for each is in `CHANGELOG.md`; highlights:

- Furious Charge cosmetic pass: proper name/icon everywhere (main panel,
  SpeedBook, spell book, tooltips), Hellfire-MPQ-free icon choice, red
  cooldown fill.
- Monster Range Highlight (adjustable red outline for nearby monsters,
  now visible through darkness as well as walls).
- Griswold "Repair all" (matching the existing "Sell all"), after fixing
  a redraw bug that made both buttons genuinely invisible for several
  versions.
- XP Counter hold-to-see-remaining-monster-XP, thousands separators.
- Town spellcasting, Sort Stash, magic-item description popups, expanded
  shrine/quest/level-up Event Log coverage.
- Primal and Buffed Unique items now get their own inventory-slot
  background colors (orange, gold) instead of sharing magic items' blue.
- A long tail of bug fixes: elixir walk-onto stacking, Rare/Buffed
  Unique/Primal tooltip number corruption (with a self-healing fix for
  already-affected items), item-name mismatches, extra-tab
  identify/repair/recharge/oil gaps, Stash single-unit consumption, main
  menu/settings-list display bugs, Speed Book stack counts, Griswold
  Premium button alignment, quest-range Unique sellability.

**Partial save-format note**: v0.3.33 (within this release's scope) changed
the Stash's internal save format; Stash contents from v0.3.32 or earlier
are lost on first load under this version, though character/inventory/
belt/equipped items are unaffected. This already took effect for anyone
updating incrementally; it's newly visible to anyone jumping straight from
the v0.3.17 package to v0.4.0, and is called out in the bundled README.
No other save-format changes occurred in this release's scope — v0.4.0
remains save-compatible with v0.3.0 and later otherwise.

Detailed behavior and test status remain authoritative in
`CHANGELOG.md` and `Gameplay-Changes.md`. Automated regression: 200 tests
across 6 targets (items_test, inv_test, stores_test, loadsave_test,
player_test, pack_test), 100% passing at release time.

## Installation

Extract directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition v0.4.0 based on DevilutionX 1.5.5. See the
SAVE COMPATIBILITY note above and in the bundled README before loading any
existing character.

# Overnight audits, batch 1: dead assets, line endings, stale docs (v1.9.302)

**Date:** 2026-09-07 (the user: "do a bunch of audits. i am going to bed.")

## Audit: assets in the MPQ that nothing loads

Every file under `Packaging/resources/oracool_assets` was checked against every string in Source (by basename and by stem, then by constructed-name prefix for `aura_*` and `levski_*`, which are built at runtime and are fine). 44 files, 5.5 MB, had no reader:

| Group | Files | Origin |
|---|---|---|
| `texture_stone_v1..7.png` | 7 (4.6 MB) | BuildWaypointPanel.ps1's INTERMEDIATE crops - inputs to the panel it composes |
| `border2_*.png` | 13 | the same tool's border-kit pieces |
| `panel_frame_*.png` | 18 | CutPanelFrameKit.ps1; the canvas replaced the framed panels |
| `spellbezel_*.png` | 4 | CutSpellbookBezel.ps1; the bezel is no longer drawn from parts |
| `inventory_sygil.png` | 1 | an INPUT to InvCompose, baked into inventory_panel.png |
| `points_icons_lit.png` | 1 | the 99-frame numbered strip CutSkillPointsIcon.ps1 replaced |

Removed from the tree, and the four tools changed so they cannot re-ship them: intermediates go to `%TEMP%` scratch folders, the shipped outputs (waypoint panel and icons, inventory panel, tabs, sort) still land in Packaging. `oracool.mpq` 39.1 -> 33.7 MB.

## Audit: stat tokens (the delivered-package IPL rule)

Every `IPL_` token used in Source/oracool data has a case in `SaveItemPower`. Clean.

## Audit: file-local per-game state

The 2026-08-30/31 sweeps hold: every window flag, the log, the grid, the latches, the dash and the Zeal chain are reset on the way out of a game. The one leftover is Redemption's tick phase counter, a function static whose only effect is where in the second the next corpse is taken; left alone.

## Audit: functions declared in Oracool headers but unused outside their unit

None (666 declarations scanned).

## Audit: mixed line endings

Three vanilla files (touch/renderers.cpp, levels/trigs.cpp, utils/paths.cpp) carried a few LF lines in CRLF files; normalised. Everything this session touched was already consistent.

## Docs

The Skill and Spell Reference's Paladin table lost its Holy Bolt row and its frames renumbered (48 frames), matching v1.9.301.

Suite 635/636, the standing dungeon-generation failure only.

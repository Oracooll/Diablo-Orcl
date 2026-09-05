# The belt glyphs in; tab hover as gold (v1.9.291)

**Date:** 2026-09-06
**Requests:** "sweep mpq for something from gpt" and, mid-build: "shadows of inv grid tabs is too much. dont use it here. hover efect to be turning the number from white to gold."

## The sweep

New in the Oracool.MPQ root: `oracool-belt-glyphs-v1.zip`, already unpacked under `02-source-art/delivered-packs/oracool-belt-glyphs-v1`. The pack answers the first belt-button brief: six 30x30 RGBA glyphs - town portal and burger menu, idle / hover / pressed - white (243,243,243) and shadow (12,7,7) on binary transparency, verified by its own manifest and verification pass. Hover is the idle mask thickened one pixel; pressed is the idle mask shifted (+1,+1) with no shadow, and the README asks that no state be recentred by its bounds. Nothing else new since the 2026-09-05 sweep.

## Consumed

- `tools/CutBeltGlyphs.ps1` - checks every pixel is one of the two colours or transparent and writes `ui\belt_glyphs_tp.png` and `ui\belt_glyphs_menu.png` (90x30, idle/hover/pressed) into Packaging/resources/oracool_assets/ui. The MPQ was repacked.
- hud_art.cpp - `TownPortalGlyphsArt` / `BurgerMenuGlyphsArt` loaded, quantised, hot-reload reset; `TryDrawBeltGlyph` draws frame <state> 1:1 centred in the 34px cell over the plate when the frame passes `IsGlyphFrame`, else the TP / M text stands in. The painted rings and bars remain loaded but undrawn.
- Oracool.MPQ README row for the pack.

## The tabs

`DrawInventoryTabs`: the hover shadow is gone; a hovered tab's numeral turns gold, white otherwise. The plates keep grey/gold for closed/open.

## Test

`OracoolAudit.TheBeltButtonsSitCentredWithShadows` now runs against the glyphs: a shape at least 12px each way, centred in the cell within two pixels. The two-colour check was dropped - this binary is headless, `LoadPalette` returns early there and `orig_palette` is not exported, so every quantised colour lands on one index.

## The disk

The commit first failed on a full C: drive. The cause was one 4.5 GB background-task output file in this session's Claude temp folder (a build log from 2026-09-05 20:44); deleting it freed 12.5 GB.

Suite 633/634, the standing dungeon-generation failure only.

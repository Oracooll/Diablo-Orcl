# 2026-09-27 - Tools repointed at the new Resources, RfA-31 in, devilutionx.mpq extracted (v1.12.202-203)

**Date:** 2026-09-27. Debug only. The user said:
- "repoint the tools to the new resources folders";
- "rfa 31 is delivered";
- "extract all devilutionx.mpq assets and put them in ...\Resources\03.DevilutionX Assets".

## v1.12.202: the tools find their art again

The user rebuilt `Resources` as:
- `01. Blizzard Assets`;
- `02. Oracooll Assets`: `01. Used`, `02. Unused`, `delivered-packs`, `ChatGPT RfA`, `skill-glyphs`, `unique-items`...;
- `03.DevilutionX Assets`.

76 tracked files still named the 2026-09-12 folders (`00-original-game-art`, `01-in-use-assets`, `02-concept-assets`): tools, the eight icon generators, the icon spec lists and source comments.

- **How references were mapped:** every reference was resolved to where the file lives now, not by one prefix swap. The old in-use folder is split three ways:
  - `items` and `item-sets` went to `01. Used`;
  - `world`, `bottom-hud`, `auras` and the other working folders went to `02. Unused`;
  - `delivered-packs`, `ui`, `unique-items`, `skill-glyphs` and `skill-sounds` sit directly in `02. Oracooll Assets`.
- **Files moved out of their folders** into `01. Used`: Fist and Regular Attacks, the skill-points bezel, the waypoint sigil, the Levski's Roar painting, the Cube and the Rift Monument paintings.
- **Blizzard folders the user renamed:**

  | Old | Now |
  |---|---|
  | `missiles` | `Animated Items 2` |
  | `gendata` | `Loading Wallpapers 640x480` |
  | `spellanimations` | `Spells Animations` |
  | `duricons` | `Item Durability Icons 32x32px` |
  | `raw\...\town.pal` | `palettes\...\town.pal` |

- **Scripts fixed by hand**, where a root folder was joined with a sub-path that moved:
  - `BuildWaypointPanel.ps1`: textures and borders are in `02. Unused`, the sigil in `01. Used`.
  - `CutLevskiRoarSkin.ps1`: its icon-pack path was already doubled before the move.
  - `BuildSpellAnimationGifs.ps1`.
- **Other fixes:** the living RfA-18 standing instruction now names the new delivery folder. The dated dev reports and the closed RfA briefs are history and keep their old paths.
- **Checks:**
  - Every static path in the tools and source was tested for existence. Two point at files that are gone from `Resources` altogether: `04-references\class-layouts\class-silhouette-reference-sheet-v2.png` (`CutClassSilhouette.ps1`) and `ORCL-skill-asset-briefs.md`, which is only named in a comment.
  - Every unquoted use of a path that now has spaces is an `echo`.
- **Proof:** `tools\build_item_icons.cmd` runs end to end again. All eight generators ran, every spec's art was found, and the `oracool_items.cel` it wrote is byte-identical to the committed one. The regenerated spec lists and data files differ only in their paths.

## v1.12.203: RfA-31, the sheet's frame backing

Batch 61 delivered the three pieces at 132 pixels tall. I checked them here:
- the top 2 and bottom 4 rows match the old pieces pixel for pixel;
- the interior is opaque;
- the left light lip is there.

They replace `ui\stat_field_*.png`. The hero-sheet render shows the 288x132 top frame 1:1 in a fine stone texture with no stretch bands. The smaller frames are shrunk by row averaging (v1.12.201).

## devilutionx.mpq extracted

All 188 files, extracted with `tools\oracool_mpq_extract.exe` into `Resources\03.DevilutionX Assets`, keeping the archive's folders: arena 3, data 19, fonts 135, gendata 10, levels 4, nlevels 4, ui_art 13.
- The archive has no internal file list, so the names came from the build's `devilutionx_mpq_files.txt`.
- The count was checked by decrypting the archive's block table: 188 files in use.
- All 188 are byte-identical to the loose files the archive was packed from.

## Tests

v1.12.202 and v1.12.203 each build clean with 873 of 873 passing.

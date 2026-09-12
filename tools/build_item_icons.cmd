@echo off
REM Builds data\inv\oracool_items.cel - every Oracool custom item icon - from the user's art
REM sheets, and installs it into both asset channels.
REM
REM Frame order is load-bearing: it must match InvItemWidth3/InvItemHeight3 in Source/cursor.cpp
REM and the ICURS_ORACOOL_* values in Source/itemdat.h EXACTLY, in numeric order - a CEL stores
REM no widths and no names of its own, only a flat frame list, so a frame's POSITION is the only
REM thing that ties it to an ICURS_* id. Bug caught while first writing this: the Iron tier's helm
REM cell and the original standalone helm render both wanted to produce a frame called "helm" -
REM harmless-looking (spec "name" is only used for console logging and preview filenames), but
REM having both specs present would have appended TWO frames, silently shifting every ICURS_*
REM value after 235 (helm) off by one and corrupting every icon from Leather Armor onward. Only
REM ONE spec may ever produce each named frame; the list below matches the ICURS_ORACOOL_*
REM numeric sequence for exactly that reason - keep it that way.
REM
REM Specs go through a temp file and ItemIconCel.exe's "@specfile" mode instead of ^-continued
REM command-line arguments. Second bug caught while first writing this: at 80 items, a single
REM ^-continued command line stopped partway through with "Bad spec: ^" and no clearer diagnosis -
REM not a bug in the parser, a cmd.exe limit on very long continued lines. One echo per spec, one
REM invocation reading the file, sidesteps it entirely and has no per-batch ceiling to hit again.
REM
REM Three source generations feed this file:
REM   - The original six worn-slot items (shoulders/bracers/gloves/belt/legs/boots): single
REM     full-canvas green-screen renders, mode=green.
REM   - Helm: sourced from the Iron tier's own composite sheet (below), not a separate standalone
REM     render - re-cut for visual consistency with the rest of that tier instead of shipping a
REM     mismatched one-off.
REM   - Leather Armor/Shield: the two pieces held back from the original batch until an item was
REM     actually named for them - see itemdat.cpp.
REM   - The eight-tier set expansion (Iron/Steel/Crusader/Bone/Royal/Obsidian/Infernal/Diamond):
REM     each tier is ONE 1402x1122 composite sheet holding all nine pieces in a 3x3 grid (gloves,
REM     shoulders, bracers / belt, legs, boots / armor, shield, helm), cut nine times per sheet
REM     with per-cell source rects. Cell (0,0) - gloves - is offset 80px down to clear the
REM     "<Tier> Set vN" title text baked into that corner; the other eight cells are plain
REM     uniform thirds of the canvas (1402/3, 1122/3), left untightened beyond that because
REM     ContentBoxByGreenKey does the real work and the gutters between cells measured wide
REM     enough (~100px+) that a few px of slop here risks nothing.
REM
REM All specs use the same trailing three fields: backdropCut (30, the shared dark-canvas
REM default - present only because it is positional and mode sits after it, unused whenever
REM mode=green), fillPunctures (false throughout this file - checked per-icon with a border
REM flood-fill puncture counter after cutting, not assumed; see the dev report for the two real
REM bugs that check caught: a ContentBoxByGreenKey/ExtractWithGreenKey threshold mismatch, then
REM the green-key ramp itself needing recalibration per composite sheet), and mode (green
REM throughout - see ItemIconCel.cs's ExtractWithGreenKey for why a chroma key replaced the
REM original dark-canvas flood fill for every single-subject render in this pipeline).
REM
REM Usage:  tools\build_item_icons.cmd
REM Run from the repository root.

setlocal enabledelayedexpansion
REM The composite sheets this file cuts from (worn slots, the fifteen set tiers, gems, runes) live
REM in item-sets. This said ...\items until 2026-09-12, which was a leftover of the Resources
REM reorganisation the day before: `items` holds only the per-icon subfolders (charms, jewels,
REM unqbase) that the GENERATED specs point at by their own full paths, so every one of the 183
REM sheet cuts below was reading a path that did not exist. The failure was ItemIconCel dying with
REM "Parameter is not valid" out of Bitmap..ctor - a missing-file message that never names a file -
REM so verify with the spec-input existence check, not by reading this line and believing it.
set ART=..\Resources\01-in-use-assets\item-sets
set PAL=tools\town.pal
set PALSRC=..\Resources\00-original-game-art\palettes\levels\towndata\town.pal

REM RE-STAGE THE PALETTE EVERY RUN, from the original, never trusting what is already there.
REM
REM tools\town.pal is gitignored (a copy of Blizzard's palette), so it is a local file with no
REM history and nothing to notice when it goes stale - which it had. On 2026-08-15 this fork injected
REM a GREEN ramp over palette entries 152-159 and something wrote that edited palette here, which was
REM correct at the time. The injection came OUT on 2026-09-10 because it was breaking the dungeon
REM fires, and this copy was never re-staged: it still claimed 140,190,140 / 100,160,100 / ... where
REM the running game has 254,190,160 / 255,140,87 / ... - orange.
REM
REM So every icon cut between those dates was palette-matched against a palette the game does not
REM have. Any source pixel near green matched into the orange minis and drew ORANGE in play. Found
REM 2026-09-12 while fixing the set-item backing, which was the same mistake in the draw code;
REM re-staging moved 21,113 bytes of oracool_items.cel and dropped its use of 152-159 from 9,465
REM pixels to 2,085 (the ones genuinely nearest to orange).
REM
REM A copy is cheap and staleness is invisible, so there is no version of this worth "optimising"
REM into a test-and-skip.
if not exist "%PALSRC%" (
  echo ERROR: the original town palette is missing: %PALSRC%
  echo        tools\town.pal cannot be trusted without it - refusing to cut icons against a stale palette.
  exit /b 1
)
copy /y "%PALSRC%" "%PAL%" >nul || exit /b 1
echo Palette re-staged from %PALSRC%
set OUT=Packaging\resources\oracool_assets\data\inv\oracool_items.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\ItemIconCel.exe
set SPECFILE=%TEMP%\oracool_item_specs.txt

if not exist "%ART%" (
  echo ERROR: source art folder not found: %ART%
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\ItemIconCel.cs || exit /b 1

REM Spec order = ICURS_ORACOOL_* numeric order (229..308). Do not reorder without re-checking
REM every frame index against itemdat.h - see the header comment above.
if exist "%SPECFILE%" del "%SPECFILE%"
> "%SPECFILE%" (
  echo %ART%\item-icons-shoulders-v2.png,0,0,1254,1254,56,56,shoulders,30,false,green
  echo %ART%\item-icons-bracers-v2.png,0,0,1254,1254,56,56,bracers,30,false,green
  echo %ART%\item-icons-gloves-v2.png,0,0,1254,1254,56,56,gloves,30,false,green
  echo %ART%\item-icons-belts-v2.png,0,0,1254,1254,56,28,belt,30,false,green,1.25
  echo %ART%\item-icons-legs-v2.png,0,0,1254,1254,56,56,legs,30,false,green
  echo %ART%\item-icons-boots-v3.png,0,0,1254,1254,56,56,boots,30,false,green
  echo %ART%\item-set-iron-v1.png,934,748,468,374,56,56,helm,30,false,green
  echo %ART%\item-icons-body-armour-v2.png,0,0,1254,1254,56,56,leather_armor,30,false,green
  echo %ART%\item-icons-shields-v3.png,0,0,1254,1254,56,56,leather_shield,30,false,green
  echo %ART%\item-set-iron-v1.png,0,80,467,294,56,56,iron_gloves,30,false,green
  echo %ART%\item-set-iron-v1.png,467,0,467,374,56,56,iron_shoulders,30,false,green
  echo %ART%\item-set-iron-v1.png,934,0,468,374,56,56,iron_bracers,30,false,green
  echo %ART%\item-set-iron-v1.png,0,374,467,374,56,28,iron_belt,30,false,green,1.25
  echo %ART%\item-set-iron-v1.png,467,374,467,374,56,56,iron_legs,30,false,green
  echo %ART%\item-set-iron-v1.png,934,374,468,374,56,56,iron_boots,30,false,green
  echo %ART%\item-set-iron-v1.png,0,748,467,374,56,56,iron_armor,30,false,green
  echo %ART%\item-set-iron-v1.png,467,748,467,374,56,56,iron_shield,30,false,green
  echo %ART%\item-set-steel-v1.png,0,80,467,294,56,56,steel_gloves,30,false,green
  echo %ART%\item-set-steel-v1.png,467,0,467,374,56,56,steel_shoulders,30,false,green
  echo %ART%\item-set-steel-v1.png,934,0,468,374,56,56,steel_bracers,30,false,green
  echo %ART%\item-set-steel-v1.png,0,374,467,374,56,28,steel_belt,30,false,green,1.25
  echo %ART%\item-set-steel-v1.png,467,374,467,374,56,56,steel_legs,30,false,green
  echo %ART%\item-set-steel-v1.png,934,374,468,374,56,56,steel_boots,30,false,green
  echo %ART%\item-set-steel-v1.png,0,748,467,374,56,56,steel_armor,30,false,green
  echo %ART%\item-set-steel-v1.png,467,748,467,374,56,56,steel_shield,30,false,green
  echo %ART%\item-set-steel-v1.png,934,748,468,374,56,56,steel_helm,30,false,green
  echo %ART%\item-set-steel-v2.png,0,80,467,294,56,56,crusader_gloves,30,false,green
  echo %ART%\item-set-steel-v2.png,467,0,467,374,56,56,crusader_shoulders,30,false,green
  echo %ART%\item-set-steel-v2.png,934,0,468,374,56,56,crusader_bracers,30,false,green
  echo %ART%\item-set-steel-v2.png,0,374,467,374,56,28,crusader_belt,30,false,green,1.25
  echo %ART%\item-set-steel-v2.png,467,374,467,374,56,56,crusader_legs,30,false,green
  echo %ART%\item-set-steel-v2.png,934,374,468,374,56,56,crusader_boots,30,false,green
  echo %ART%\item-set-steel-v2.png,0,748,467,374,56,56,crusader_armor,30,false,green
  echo %ART%\item-set-steel-v2.png,467,748,467,374,56,56,crusader_shield,30,false,green
  echo %ART%\item-set-steel-v2.png,934,748,468,374,56,56,crusader_helm,30,false,green
  echo %ART%\item-set-bone-v1.png,0,80,467,294,56,56,bone_gloves,30,false,green
  echo %ART%\item-set-bone-v1.png,467,0,467,374,56,56,bone_shoulders,30,false,green
  echo %ART%\item-set-bone-v1.png,934,0,468,374,56,56,bone_bracers,30,false,green
  echo %ART%\item-set-bone-v1.png,0,374,467,374,56,28,bone_belt,30,false,green,1.25
  echo %ART%\item-set-bone-v1.png,467,374,467,374,56,56,bone_legs,30,false,green
  echo %ART%\item-set-bone-v1.png,934,374,468,374,56,56,bone_boots,30,false,green
  echo %ART%\item-set-bone-v1.png,0,748,467,374,56,56,bone_armor,30,false,green
  echo %ART%\item-set-bone-v1.png,467,748,467,374,56,56,bone_shield,30,false,green
  echo %ART%\item-set-bone-v1.png,934,748,468,374,56,56,bone_helm,30,false,green
  echo %ART%\item-set-gold-v1.png,0,80,467,294,56,56,royal_gloves,30,false,green
  echo %ART%\item-set-gold-v1.png,467,0,467,374,56,56,royal_shoulders,30,false,green
  echo %ART%\item-set-gold-v1.png,934,0,468,374,56,56,royal_bracers,30,false,green
  echo %ART%\item-set-gold-v1.png,0,374,467,374,56,28,royal_belt,30,false,green,1.25
  echo %ART%\item-set-gold-v1.png,467,374,467,374,56,56,royal_legs,30,false,green
  echo %ART%\item-set-gold-v1.png,934,374,468,374,56,56,royal_boots,30,false,green
  echo %ART%\item-set-gold-v1.png,0,748,467,374,56,56,royal_armor,30,false,green
  echo %ART%\item-set-gold-v1.png,467,748,467,374,56,56,royal_shield,30,false,green
  echo %ART%\item-set-gold-v1.png,934,748,468,374,56,56,royal_helm,30,false,green
  echo %ART%\item-set-obsidian-v1.png,0,80,467,294,56,56,obsidian_gloves,30,false,green
  echo %ART%\item-set-obsidian-v1.png,467,0,467,374,56,56,obsidian_shoulders,30,false,green
  echo %ART%\item-set-obsidian-v1.png,934,0,468,374,56,56,obsidian_bracers,30,false,green
  echo %ART%\item-set-obsidian-v1.png,0,374,467,374,56,28,obsidian_belt,30,false,green,1.25
  echo %ART%\item-set-obsidian-v1.png,467,374,467,374,56,56,obsidian_legs,30,false,green
  echo %ART%\item-set-obsidian-v1.png,934,374,468,374,56,56,obsidian_boots,30,false,green
  echo %ART%\item-set-obsidian-v1.png,0,748,467,374,56,56,obsidian_armor,30,false,green
  echo %ART%\item-set-obsidian-v1.png,467,748,467,374,56,56,obsidian_shield,30,false,green
  echo %ART%\item-set-obsidian-v1.png,934,748,468,374,56,56,obsidian_helm,30,false,green
  echo %ART%\item-set-obsidian-v2.png,0,80,467,294,56,56,infernal_gloves,30,false,green
  echo %ART%\item-set-obsidian-v2.png,467,0,467,374,56,56,infernal_shoulders,30,false,green
  echo %ART%\item-set-obsidian-v2.png,934,0,468,374,56,56,infernal_bracers,30,false,green
  echo %ART%\item-set-obsidian-v2.png,0,374,467,374,56,28,infernal_belt,30,false,green,1.25
  echo %ART%\item-set-obsidian-v2.png,467,374,467,374,56,56,infernal_legs,30,false,green
  echo %ART%\item-set-obsidian-v2.png,934,374,468,374,56,56,infernal_boots,30,false,green
  echo %ART%\item-set-obsidian-v2.png,0,748,467,374,56,56,infernal_armor,30,false,green
  echo %ART%\item-set-obsidian-v2.png,467,748,467,374,56,56,infernal_shield,30,false,green
  echo %ART%\item-set-obsidian-v2.png,934,748,468,374,56,56,infernal_helm,30,false,green
  echo %ART%\item-set-diamond-v1.png,0,80,467,294,56,56,diamond_gloves,30,false,green
  echo %ART%\item-set-diamond-v1.png,467,0,467,374,56,56,diamond_shoulders,30,false,green
  echo %ART%\item-set-diamond-v1.png,934,0,468,374,56,56,diamond_bracers,30,false,green
  echo %ART%\item-set-diamond-v1.png,0,374,467,374,56,28,diamond_belt,30,false,green,1.25
  echo %ART%\item-set-diamond-v1.png,467,374,467,374,56,56,diamond_legs,30,false,green
  echo %ART%\item-set-diamond-v1.png,934,374,468,374,56,56,diamond_boots,30,false,green
  echo %ART%\item-set-diamond-v1.png,0,748,467,374,56,56,diamond_armor,30,false,green
  echo %ART%\item-set-diamond-v1.png,467,748,467,374,56,56,diamond_shield,30,false,green
  echo %ART%\item-set-diamond-v1.png,934,748,468,374,56,56,diamond_helm,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,0,80,467,294,56,56,ruby_gloves,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,467,0,467,374,56,56,ruby_shoulders,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,934,0,468,374,56,56,ruby_bracers,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,0,374,467,374,56,28,ruby_belt,30,false,green,1.25
  echo %ART%\item-set-red-diamond-v1.png,467,374,467,374,56,56,ruby_legs,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,934,374,468,374,56,56,ruby_boots,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,0,748,467,374,56,56,ruby_armor,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,467,748,467,374,56,56,ruby_shield,30,false,green
  echo %ART%\item-set-red-diamond-v1.png,934,748,468,374,56,56,ruby_helm,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,0,80,467,294,56,56,onyx_gloves,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,467,0,467,374,56,56,onyx_shoulders,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,934,0,468,374,56,56,onyx_bracers,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,0,374,467,374,56,28,onyx_belt,30,false,green,1.25
  echo %ART%\item-set-black-diamond-v1.png,467,374,467,374,56,56,onyx_legs,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,934,374,468,374,56,56,onyx_boots,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,0,748,467,374,56,56,onyx_armor,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,467,748,467,374,56,56,onyx_shield,30,false,green
  echo %ART%\item-set-black-diamond-v1.png,934,748,468,374,56,56,onyx_helm,30,false,green
  echo %ART%\item-set-ice-v1.png,0,80,467,294,56,56,glacial_gloves,30,false,green
  echo %ART%\item-set-ice-v1.png,467,0,467,374,56,56,glacial_shoulders,30,false,green
  echo %ART%\item-set-ice-v1.png,934,0,468,374,56,56,glacial_bracers,30,false,green
  echo %ART%\item-set-ice-v1.png,0,374,467,374,56,28,glacial_belt,30,false,green,1.25
  echo %ART%\item-set-ice-v1.png,467,374,467,374,56,56,glacial_legs,30,false,green
  echo %ART%\item-set-ice-v1.png,934,374,468,374,56,56,glacial_boots,30,false,green
  echo %ART%\item-set-ice-v1.png,0,748,467,374,56,56,glacial_armor,30,false,green
  echo %ART%\item-set-ice-v1.png,467,748,467,374,56,56,glacial_shield,30,false,green
  echo %ART%\item-set-ice-v1.png,934,748,468,374,56,56,glacial_helm,30,false,green
  echo %ART%\item-set-cyborg-v1.png,0,80,467,294,56,56,cyborg_gloves,30,false,green
  echo %ART%\item-set-cyborg-v1.png,1100,748,302,374,56,56,cyborg_shoulders,30,false,green
  echo %ART%\item-set-cyborg-v1.png,934,0,468,374,56,56,cyborg_bracers,30,false,green
  echo %ART%\item-set-cyborg-v1.png,0,374,467,374,56,28,cyborg_belt,30,false,green,1.25
  echo %ART%\item-set-cyborg-v1.png,467,374,467,374,56,56,cyborg_legs,30,false,green
  echo %ART%\item-set-cyborg-v1.png,934,374,468,374,56,56,cyborg_boots,30,false,green
  echo %ART%\item-set-cyborg-v1.png,0,748,467,374,56,56,cyborg_armor,30,false,green
  echo %ART%\item-set-cyborg-v1.png,467,748,467,374,56,56,cyborg_shield,30,false,green
  echo %ART%\item-set-cyborg-v1.png,467,0,467,374,56,56,cyborg_helm,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,0,80,467,294,56,56,fallen_gloves,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,934,748,468,374,56,56,fallen_shoulders,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,934,0,468,374,56,56,fallen_bracers,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,0,374,467,374,56,28,fallen_belt,30,false,green,1.25
  echo %ART%\item-set-dark-angel-v1.png,467,374,467,374,56,56,fallen_legs,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,934,374,468,374,56,56,fallen_boots,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,0,748,467,374,56,56,fallen_armor,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,467,748,467,374,56,56,fallen_shield,30,false,green
  echo %ART%\item-set-dark-angel-v1.png,467,0,467,374,56,56,fallen_helm,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,0,80,467,294,56,56,seraphic_gloves,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,934,748,468,374,56,56,seraphic_shoulders,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,934,0,468,374,56,56,seraphic_bracers,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,0,374,467,374,56,28,seraphic_belt,30,false,green,1.25
  echo %ART%\item-set-angelic-gold-v1.png,467,374,467,374,56,56,seraphic_legs,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,934,374,468,374,56,56,seraphic_boots,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,0,748,467,374,56,56,seraphic_armor,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,467,748,467,374,56,56,seraphic_shield,30,false,green
  echo %ART%\item-set-angelic-gold-v1.png,467,0,467,374,56,56,seraphic_helm,30,false,green
  echo %ART%\item-set-ghostly-v1.png,0,80,467,294,56,56,spectral_gloves,30,false,green
  echo %ART%\item-set-ghostly-v1.png,934,748,468,374,56,56,spectral_shoulders,30,false,green
  echo %ART%\item-set-ghostly-v1.png,934,0,468,374,56,56,spectral_bracers,30,false,green
  echo %ART%\item-set-ghostly-v1.png,0,374,467,374,56,28,spectral_belt,30,false,green,1.25
  echo %ART%\item-set-ghostly-v1.png,467,374,467,374,56,56,spectral_legs,30,false,green
  echo %ART%\item-set-ghostly-v1.png,934,374,468,374,56,56,spectral_boots,30,false,green
  echo %ART%\item-set-ghostly-v1.png,0,748,467,374,56,56,spectral_armor,30,false,green
  echo %ART%\item-set-ghostly-v1.png,467,748,467,374,56,56,spectral_shield,30,false,green
  echo %ART%\item-set-ghostly-v1.png,467,0,467,374,56,56,spectral_helm,30,false,green
  REM Gems: the user's pixel-art sheet (Gems.png), a clean 7x5 grid measured by green-key scan.
  REM Columns are types - amethyst diamond emerald ruby sapphire topaz skull - and rows are the
  REM quality ladder: chipped, flawed, normal, flawless, perfect.
  REM   col x-spans: 67-187  266-389  467-588  665-786  863-983  1059-1179  1251-1374
  REM   row y-spans: 102-213  279-399  463-593  651-790  837-987
  REM These five are the NORMAL row (y 463, height 131) and keep ICURS 372-376, so an item already
  REM sitting in a save keeps pointing at the right picture - it just gets the better art.
  echo %ART%\item-gems-v2.png,665,463,122,131,28,28,gem_ruby,30,false,green
  echo %ART%\item-gems-v2.png,863,463,121,131,28,28,gem_sapphire,30,false,green
  echo %ART%\item-gems-v2.png,1059,463,121,131,28,28,gem_topaz,30,false,green
  REM The emerald is itself green, and measurement says no threshold can save it: the backdrop
  REM reaches a green-excess of 240 and the gem's own body reaches 234. Its column is therefore
  REM pre-processed to a black backdrop by exact-colour match (item-gems-v2-emerald-dark.png,
  REM x 467-588 of the sheet cropped to x 0) and cut with the dark-mode extractor instead.
  echo %ART%\item-gems-v2-emerald-dark.png,0,463,122,131,28,28,gem_emerald,30,false
  echo %ART%\item-gems-v2.png,1251,463,124,131,28,28,gem_skull,30,false,green
  REM Runes: the user's 33-rune D2 sheet, 11 columns x 3 rows. We use five: El, Tir, Ral, Ort (row 1), Sol (row 2).
  echo %ART%\item-runes-v1.png,38,230,106,140,28,28,rune_el,30,false,green
  echo %ART%\item-runes-v1.png,296,230,105,140,28,28,rune_tir,30,false,green
  echo %ART%\item-runes-v1.png,922,230,106,140,28,28,rune_ral,30,false,green
  echo %ART%\item-runes-v1.png,1048,230,107,140,28,28,rune_ort,30,false,green
  echo %ART%\item-runes-v1.png,36,441,107,140,28,28,rune_sol,30,false,green
  REM The rest of the gem ladder, ICURS 382-411. Same grid as the five above; these are the four
  REM other qualities of each type, plus all five of the two types that were missing entirely.
  echo %ART%\item-gems-v2.png,67,102,121,112,28,28,gem_amethyst_chipped,30,false,green
  echo %ART%\item-gems-v2.png,67,279,121,121,28,28,gem_amethyst_flawed,30,false,green
  echo %ART%\item-gems-v2.png,67,463,121,131,28,28,gem_amethyst_normal,30,false,green
  echo %ART%\item-gems-v2.png,67,651,121,140,28,28,gem_amethyst_flawless,30,false,green
  echo %ART%\item-gems-v2.png,67,837,121,151,28,28,gem_amethyst_perfect,30,false,green
  echo %ART%\item-gems-v2.png,266,102,124,112,28,28,gem_diamond_chipped,30,false,green
  echo %ART%\item-gems-v2.png,266,279,124,121,28,28,gem_diamond_flawed,30,false,green
  echo %ART%\item-gems-v2.png,266,463,124,131,28,28,gem_diamond_normal,30,false,green
  echo %ART%\item-gems-v2.png,266,651,124,140,28,28,gem_diamond_flawless,30,false,green
  echo %ART%\item-gems-v2.png,266,837,124,151,28,28,gem_diamond_perfect,30,false,green
  echo %ART%\item-gems-v2-emerald-dark.png,0,102,122,112,28,28,gem_emerald_chipped,30,false
  echo %ART%\item-gems-v2-emerald-dark.png,0,279,122,121,28,28,gem_emerald_flawed,30,false
  echo %ART%\item-gems-v2-emerald-dark.png,0,651,122,140,28,28,gem_emerald_flawless,30,false
  echo %ART%\item-gems-v2-emerald-dark.png,0,837,122,151,28,28,gem_emerald_perfect,30,false
  echo %ART%\item-gems-v2.png,665,102,122,112,28,28,gem_ruby_chipped,30,false,green
  echo %ART%\item-gems-v2.png,665,279,122,121,28,28,gem_ruby_flawed,30,false,green
  echo %ART%\item-gems-v2.png,665,651,122,140,28,28,gem_ruby_flawless,30,false,green
  echo %ART%\item-gems-v2.png,665,837,122,151,28,28,gem_ruby_perfect,30,false,green
  echo %ART%\item-gems-v2.png,863,102,121,112,28,28,gem_sapphire_chipped,30,false,green
  echo %ART%\item-gems-v2.png,863,279,121,121,28,28,gem_sapphire_flawed,30,false,green
  echo %ART%\item-gems-v2.png,863,651,121,140,28,28,gem_sapphire_flawless,30,false,green
  echo %ART%\item-gems-v2.png,863,837,121,151,28,28,gem_sapphire_perfect,30,false,green
  echo %ART%\item-gems-v2.png,1059,102,121,112,28,28,gem_topaz_chipped,30,false,green
  echo %ART%\item-gems-v2.png,1059,279,121,121,28,28,gem_topaz_flawed,30,false,green
  echo %ART%\item-gems-v2.png,1059,651,121,140,28,28,gem_topaz_flawless,30,false,green
  echo %ART%\item-gems-v2.png,1059,837,121,151,28,28,gem_topaz_perfect,30,false,green
  echo %ART%\item-gems-v2.png,1251,102,124,112,28,28,gem_skull_chipped,30,false,green
  echo %ART%\item-gems-v2.png,1251,279,124,121,28,28,gem_skull_flawed,30,false,green
  echo %ART%\item-gems-v2.png,1251,651,124,140,28,28,gem_skull_flawless,30,false,green
  echo %ART%\item-gems-v2.png,1251,837,124,151,28,28,gem_skull_perfect,30,false,green
)

REM The fifteen item sets' 94 icons, ICURS 412..505. Appended from a GENERATED spec list rather
REM than written out here: tools\GenItemSets.ps1 walks the same set-data.json files that produce
REM the ICURS_ORACOOL_SET_* ids and the InvItemWidth3/Height3 rows, and emits all four in one pass
REM and one order. Ninety-four hand-written spec lines would be ninety-four chances to put the
REM frame order out of step with the ids, and a CEL has no way to notice.
REM
REM Their specs use the "asis" mode: the art is already exactly grid x 28 with real transparency,
REM so the crop-and-refit every other spec needs would only damage it. See ItemIconCel.cs.
if not exist "Source\oracool\item_sets_icon_specs.txt" (
  echo ERROR: Source\oracool\item_sets_icon_specs.txt is missing - run tools\GenItemSets.ps1 first
  exit /b 1
)
type "Source\oracool\item_sets_icon_specs.txt" >> "%SPECFILE%"

REM The expansion uniques' 143 frames, appended AFTER the sets so their frames land at 506 onward -
REM the order cursor.cpp's static_asserts and the generated ICURS_ORACOOL_UNQ_* ids assume.
REM Same asis mode, same one-generator discipline: tools\GenUniqueItems.ps1 emits this spec together
REM with the ids and the width/height rows in one walk.
if not exist "Source\oracool\unique_items_icon_specs.txt" (
  echo ERROR: Source\oracool\unique_items_icon_specs.txt is missing - run tools\GenUniqueItems.ps1 first
  exit /b 1
)
type "Source\oracool\unique_items_icon_specs.txt" >> "%SPECFILE%"

REM Sockets v2: the other 28 runes, appended AFTER the uniques so their frames land past
REM ICURS_ORACOOL_UNQ_LAST - the order runes_curs.inc and the width/height tables assume.
REM GENERATED by tools\GenRunes.ps1 in one walk with the ids, sizes and data rows.
if not exist "Source\oracool\runes_icon_specs.txt" (
  echo ERROR: Source\oracool\runes_icon_specs.txt is missing - run tools\GenRunes.ps1 first
  exit /b 1
)
type "Source\oracool\runes_icon_specs.txt" >> "%SPECFILE%"

REM Salvage: the seven material orbs, appended AFTER the runes so their frames land past
REM ICURS_ORACOOL_RUNE_ZOD - the order salvage_curs.inc and the size tables assume.
REM GENERATED by tools\GenSalvageMaterials.ps1, which also DRAWS the placeholder art these specs
REM point at, into %TEMP%. Run it first; there is no hand-made file to fall back on, and that is
REM deliberate - a placeholder nobody can mistake for finished art.
if not exist "Source\oracool\salvage_icon_specs.txt" (
  echo ERROR: Source\oracool\salvage_icon_specs.txt is missing - run tools\GenSalvageMaterials.ps1 first
  exit /b 1
)
type "Source\oracool\salvage_icon_specs.txt" >> "%SPECFILE%"

REM Jewels: the third socket family, appended AFTER the salvage block so their frames land past
REM ICURS_ORACOOL_CHARM_SALVAGE_ETHEREAL_IMBUEITIES - the order jewels_curs.inc and the size tables
REM assume. GENERATED by tools\GenJewels.ps1, which also DRAWS them; there is no hand-made file to
REM fall back on, deliberately.
if not exist "Source\oracool\jewels_icon_specs.txt" (
  echo ERROR: Source\oracool\jewels_icon_specs.txt is missing - run tools\GenJewels.ps1 first
  exit /b 1
)
type "Source\oracool\jewels_icon_specs.txt" >> "%SPECFILE%"

REM Mystic Orbs, appended AFTER the jewels - the order mystic_orbs_curs.inc and the size tables
REM assume. GENERATED by tools\GenMysticOrbs.ps1, which also DRAWS them.
if not exist "Source\oracool\mystic_orbs_icon_specs.txt" (
  echo ERROR: Source\oracool\mystic_orbs_icon_specs.txt is missing - run tools\GenMysticOrbs.ps1 first
  exit /b 1
)
type "Source\oracool\mystic_orbs_icon_specs.txt" >> "%SPECFILE%"

REM The Signet of Learning, appended AFTER the orbs. GENERATED by tools\GenSignets.ps1.
if not exist "Source\oracool\signets_icon_specs.txt" (
  echo ERROR: Source\oracool\signets_icon_specs.txt is missing - run tools\GenSignets.ps1 first
  exit /b 1
)
type "Source\oracool\signets_icon_specs.txt" >> "%SPECFILE%"

REM The growing charms, appended AFTER the signet. GENERATED by tools\GenGrowingCharms.ps1.
if not exist "Source\oracool\growing_charms_icon_specs.txt" (
  echo ERROR: Source\oracool\growing_charms_icon_specs.txt is missing - run tools\GenGrowingCharms.ps1 first
  exit /b 1
)
type "Source\oracool\growing_charms_icon_specs.txt" >> "%SPECFILE%"

REM The named-encounter items, appended AFTER the growing charms. GENERATED by
REM tools\GenEncounterItems.ps1.
if not exist "Source\oracool\encounter_items_icon_specs.txt" (
  echo ERROR: Source\oracool\encounter_items_icon_specs.txt is missing - run tools\GenEncounterItems.ps1 first
  exit /b 1
)
type "Source\oracool\encounter_items_icon_specs.txt" >> "%SPECFILE%"

REM The six Phase 1 charms' own icons, appended AFTER the encounter items (RfA-02 batch 5,
REM 2026-09-11). Hand-written spec; the art is the delivered 28x28 icons, filed under
REM ..\Resources\01-in-use-assets\items\charms. First added by appending to the sheet in place, because
REM the Temp inputs of several generators above had been cleared - re-run those generators before
REM trusting a full rebuild of this file.
type "Source\oracool\charm_icons_icon_specs.txt" >> "%SPECFILE%"

REM The unique expansion's LATE run (2026-09-11): the 107 uniques on bases mapped through the alias
REM table in tools\GenUniqueItems.ps1, appended AFTER the charm icons. GENERATED by that script.
if not exist "Source\oracool\unique_items2_icon_specs.txt" (
  echo ERROR: Source\oracool\unique_items2_icon_specs.txt is missing - run tools\GenUniqueItems.ps1 first
  exit /b 1
)
type "Source\oracool\unique_items2_icon_specs.txt" >> "%SPECFILE%"

REM The 18 unique-expansion bases' own icons (RfA-04 batch 12, 2026-09-11), after the late run.
REM Hand-written spec; art in ..\Resources\01-in-use-assets\items\unqbase.
type "Source\oracool\unqbase_icons_icon_specs.txt" >> "%SPECFILE%"

REM VERIFY EVERY SPEC'S ART EXISTS, before cutting anything.
REM
REM The guards above each check only that the SPEC FILE exists, never that the art it names does.
REM That is a real gap: three of the generated specs (item_sets, unique_items, unique_items2) name
REM paths inside %TEMP%, because their art arrives as zips that the generators extract there. Those
REM trees are not the repository's, and they evaporate - Storage Sense clears %TEMP%, and a fresh
REM clone never had them. Without this check the cutter meets a missing file and dies with
REM "Parameter is not valid" out of Bitmap..ctor, which names no file at all (2026-09-12, and it
REM cost a whole debugging session once already when the ART root was stale).
REM
REM So: every spec line's first field, checked, with the missing ones NAMED and the generator to
REM re-run spelled out. One pass over ~620 lines costs nothing next to cutting them.
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$bad = @(); foreach ($line in Get-Content '%SPECFILE%') { if ($line -match '^\s*REM' -or $line.Trim() -eq '') { continue }; $p = ($line -split ',')[0]; if (-not (Test-Path -LiteralPath $p)) { $bad += $p } };" ^
  "if ($bad.Count -gt 0) { Write-Host ''; Write-Host ('SPEC ART MISSING: {0} of the cut specs name a file that is not there.' -f $bad.Count) -ForegroundColor Red; $bad | Select-Object -Unique | Select-Object -First 12 | ForEach-Object { Write-Host ('    ' + $_) }; if (($bad | Select-Object -Unique).Count -gt 12) { Write-Host '    ...' }; Write-Host ''; Write-Host 'A %%TEMP%% path here means a generator has not run on this machine, or the folder was cleared.' -ForegroundColor Yellow; Write-Host 'Re-run the generators that own them, then this script again:' -ForegroundColor Yellow; Write-Host '    GenItemSets.ps1  GenUniqueItems.ps1  GenSalvageMaterials.ps1  GenMysticOrbs.ps1  GenSignets.ps1  GenEncounterItems.ps1  GenJewels.ps1  GenGrowingCharms.ps1'; exit 1 }" || exit /b 1
echo Spec art verified.

"%EXE%" "%PAL%" "%OUT%" "%TEMP%\oracool_item_icons" "@%SPECFILE%" || exit /b 1

REM Second channel: the loose assets folder, so a build that has not had oracool.mpq packed yet
REM still finds the sheet. InitCursor loads it unconditionally, so a missing file is fatal.
if not exist "Packaging\resources\assets\data\inv" mkdir "Packaging\resources\assets\data\inv"
copy /y "%OUT%" "Packaging\resources\assets\data\inv\oracool_items.cel" >nul
if exist "build\x64-Debug\assets" (
  if not exist "build\x64-Debug\assets\data\inv" mkdir "build\x64-Debug\assets\data\inv"
  copy /y "%OUT%" "build\x64-Debug\assets\data\inv\oracool_items.cel" >nul
)

echo.
echo oracool_items.cel installed. Now run tools\build_oracool_mpq.cmd to repack the archive.
endlocal

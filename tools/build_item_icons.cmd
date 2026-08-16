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
set ART=..\Oracool.MPQ\02-source-art\items
set PAL=tools\town.pal
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
  REM Gems: the user's 7x5 sheet, row 3 (the oval cuts). Columns: amethyst diamond emerald ruby sapphire topaz skull.
  echo %ART%\item-gems-v1.png,843,602,281,293,28,28,gem_ruby,30,false,green
  echo %ART%\item-gems-v1.png,1124,602,281,293,28,28,gem_sapphire,30,false,green
  echo %ART%\item-gems-v1.png,1405,602,281,293,28,28,gem_topaz,30,false,green
  REM The emerald is itself green, so the chroma key eats it. Its cell is pre-processed to a black
  REM backdrop (item-gems-emerald-dark-v1.png) and cut with the dark-mode extractor instead.
  echo %ART%\item-gems-emerald-dark-v1.png,0,0,281,293,28,28,gem_emerald,30,false
  echo %ART%\item-gems-v1.png,1686,602,281,293,28,28,gem_skull,30,false,green
  REM Runes: the user's 33-rune D2 sheet, 11 columns x 3 rows. We use five: El, Tir, Ral, Ort (row 1), Sol (row 2).
  echo %ART%\item-runes-v1.png,38,230,106,140,28,28,rune_el,30,false,green
  echo %ART%\item-runes-v1.png,296,230,105,140,28,28,rune_tir,30,false,green
  echo %ART%\item-runes-v1.png,922,230,106,140,28,28,rune_ral,30,false,green
  echo %ART%\item-runes-v1.png,1048,230,107,140,28,28,rune_ort,30,false,green
  echo %ART%\item-runes-v1.png,36,441,107,140,28,28,rune_sol,30,false,green
)

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

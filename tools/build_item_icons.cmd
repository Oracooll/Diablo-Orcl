@echo off
REM Builds data\inv\oracool_items.cel - the inventory icons for the six worn item types Oracool
REM added, plus any other custom item icon (currently: one new ILOC_HELM item) - from the user's
REM art sheets, and installs it into both asset channels.
REM
REM Frame order is load-bearing: it must match InvItemWidth3/InvItemHeight3 in Source/cursor.cpp
REM and the ICURS_ORACOOL_* values in Source/itemdat.h. A CEL stores no widths of its own.
REM
REM All seven specs below are green-screen "-v2"/"-v3" sources as of this pass, each a single
REM full-canvas render (source rect is always the whole 1254x1254 image - there is nothing else
REM on the canvas to crop away), cut with mode=green (see ItemIconCel.cs's ExtractWithGreenKey).
REM User request, after the original hand-painted-sheet and dark-canvas-render sources kept
REM causing their own investigations (torn edges, then enclosed punctures) - green versus this
REM item family's browns/bronzes/iron greys needs no flood-fill connectivity analysis to tell
REM apart, unlike black versus a dark item interior. Confirmed on this batch: 0 enclosed
REM punctures on every icon except a 9-pixel fleck on boots (negligible, left alone - compare the
REM belt's confirmed-real 212px gap discovered the same way).
REM
REM Each spec's trailing three fields are backdropCut (30, the shared default - present only
REM because it is positional and mode sits after it), fillPunctures (false - not needed for any
REM of these seven, verified per-icon rather than assumed), and mode (green).
REM
REM Two ADDITIONAL green-screen renders exist in the same source folder - body-armour-v2 and
REM shields-v3 - cut just as cleanly but deliberately NOT wired in below: unlike these seven,
REM ILOC_ARMOR/ILOC_SHIELD have no Oracool item occupying them yet, and adding one is a new-item
REM decision (name, stats, whether it ships at all) nobody has made, not an art swap. Cut them by
REM hand with ItemIconCel.exe directly if you want a preview.
REM
REM Usage:  tools\build_item_icons.cmd
REM Run from the repository root.

setlocal
set ART=..\Oracool.MPQ\02-source-art\items
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\data\inv\oracool_items.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\ItemIconCel.exe

if not exist "%ART%" (
  echo ERROR: source art folder not found: %ART%
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\ItemIconCel.cs || exit /b 1

"%EXE%" "%PAL%" "%OUT%" "%TEMP%\oracool_item_icons" ^
  "%ART%\item-icons-shoulders-v2.png,0,0,1254,1254,56,56,shoulders,30,false,green" ^
  "%ART%\item-icons-bracers-v2.png,0,0,1254,1254,56,56,bracers,30,false,green" ^
  "%ART%\item-icons-gloves-v2.png,0,0,1254,1254,56,56,gloves,30,false,green" ^
  "%ART%\item-icons-belts-v2.png,0,0,1254,1254,56,28,belt,30,false,green" ^
  "%ART%\item-icons-legs-v2.png,0,0,1254,1254,56,56,legs,30,false,green" ^
  "%ART%\item-icons-boots-v3.png,0,0,1254,1254,56,56,boots,30,false,green" ^
  "%ART%\item-icons-helm-v2.png,0,0,1254,1254,56,56,helm,30,false,green" || exit /b 1

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

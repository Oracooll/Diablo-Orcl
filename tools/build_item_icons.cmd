@echo off
REM Builds data\inv\oracool_items.cel - the inventory icons for the six worn item types Oracool
REM added, plus any other custom item icon (currently: one new ILOC_HELM item) - from the user's
REM art sheets, and installs it into both asset channels.
REM
REM Frame order is load-bearing: it must match InvItemWidth3/InvItemHeight3 in Source/cursor.cpp
REM and the ICURS_ORACOOL_* values in Source/itemdat.h. A CEL stores no widths of its own.
REM
REM The source rects for the six worn-type sheets are the FIRST item of each sheet - one test item
REM per type. They stop short of each cell's boxed ground icon and caption, which sit directly
REM beneath the large art; a taller box picks those up and they end up composited into the icon.
REM The helm is its own single-render source image, so its rect is simply the full canvas.
REM
REM The helm's spec carries two trailing tuning fields the other six don't: an explicit backdrop
REM cutoff (30, same as the shared default - written out because the tenth field can't be passed
REM without it) and fillPunctures=true, which closes enclosed transparent holes left by the
REM downscale (see ItemIconCel.cs's Pass 4). Deliberately NOT enabled on the six worn types: the
REM belt's own enclosed gap was reviewed and confirmed intentional in an earlier pass ("daylight
REM through the middle of the loop"), and turning this on for all seven would silently re-decide
REM that call as a side effect of a fix nobody asked to revisit it for.
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
  "%ART%\item-icons-shoulders.png,20,70,225,152,56,56,shoulders" ^
  "%ART%\item-icons-bracers.png,20,70,225,152,56,56,bracers" ^
  "%ART%\item-icons-gloves.png,20,78,225,157,56,56,gloves" ^
  "%ART%\item-icons-belts.png,20,90,270,120,56,28,belt" ^
  "%ART%\item-icons-legs.png,20,70,225,152,56,56,legs" ^
  "%ART%\item-icons-boots-v2.png,20,70,225,152,56,56,boots" ^
  "%ART%\item-icons-helm.png,0,0,1536,1024,56,56,helm,30,true" || exit /b 1

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

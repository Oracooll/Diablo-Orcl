@echo off
REM Builds data\inv\oracool_items.cel - the inventory icons for the six worn item types Oracool
REM added - from the user's art sheets, and installs it into both asset channels.
REM
REM Frame order is load-bearing: it must match InvItemWidth3/InvItemHeight3 in Source/cursor.cpp
REM and the ICURS_ORACOOL_* values in Source/itemdat.h. A CEL stores no widths of its own.
REM
REM The source rects below are the FIRST item of each sheet - one test item per type. They stop
REM short of each cell's boxed ground icon and caption, which sit directly beneath the large art;
REM a taller box picks those up and they end up composited into the icon.
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
  "%ART%\item-icons-boots-v2.png,20,70,225,152,56,56,boots" || exit /b 1

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

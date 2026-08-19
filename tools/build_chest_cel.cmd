@echo off
REM Builds objects\orclstash.cel from the 2026-08-20 chest art - the town Stash Chest.
REM
REM Replaces the grand-reliquary pack as the live chest. That pack and its build script both stay
REM (user: "the old one is there just in case. keep it"), so this is a sibling of
REM tools\build_reliquary_cel.cmd rather than an edit of it - run whichever chest you want.
REM
REM Two differences from the reliquary build, both deliberate:
REM
REM   nomirror  - the reliquary was flipped because the user picked the opposite facing for THAT
REM               pack. This art is delivered in its own facing and stays in it ("leave it as
REM               drawn").
REM   width 76  - a target rather than the 0.5 ratio. The reliquary masters were 160 wide, so half
REM               was 80; these are 517, and half would be 258 - a chest the size of a house. 76 is
REM               the width the engine already expects.
REM
REM 76 must equal OracoolStashChestAnimWidth in Source/objdat.h. CEL stores no width, so a mismatch
REM splits every RLE scanline at the wrong point - the tool prints what it produced, check it.
REM
REM Run tools\CutChestStates.ps1 first: it writes the three RGBA masters this consumes.
REM
REM Usage:  tools\build_chest_cel.cmd
REM Run from the repository root.

setlocal
set ART=%TEMP%\oracool-chest-states
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclstash.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\ReliquaryCel.exe

for %%S in (closed opening open) do (
  if not exist "%ART%\chest_%%S.png" (
    echo ERROR: state master not found: %ART%\chest_%%S.png
    echo Run tools\CutChestStates.ps1 first.
    exit /b 1
  )
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\ReliquaryCel.cs || exit /b 1
"%EXE%" "%ART%\chest_closed.png" "%ART%\chest_opening.png" "%ART%\chest_open.png" "%PAL%" "%OUT%" - nomirror 76 || exit /b 1

REM Second channel: the loose assets folder, so a build that has not had oracool.mpq packed yet
REM still finds the sprite instead of dying on a missing file.
if not exist "Packaging\resources\assets\objects" mkdir "Packaging\resources\assets\objects"
copy /y "%OUT%" "Packaging\resources\assets\objects\orclstash.cel" >nul
if exist "build\x64-Debug\assets" (
  if not exist "build\x64-Debug\assets\objects" mkdir "build\x64-Debug\assets\objects"
  copy /y "%OUT%" "build\x64-Debug\assets\objects\orclstash.cel" >nul
)

echo.
echo orclstash.cel installed. Now run tools\build_oracool_mpq.cmd to repack the archive.

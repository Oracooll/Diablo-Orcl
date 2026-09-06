@echo off
REM Builds objects\orclstash.cel - the town Stash Chest's Grand Reliquary - at half the delivered
REM size and mirrored, from the pack's three RGBA state masters.
REM
REM The delivered pack ships a finished 160-wide CEL; this re-cut exists because that read as a
REM building rather than a chest (user, 2026-08-18: "this is too big. scale down to half the size
REM and use the mirror asset"). Halving means resampling, and resampling palette indices would
REM blend them as numbers, so this goes back to the RGBA masters and redoes downsample-then-quantise
REM in that order. See tools/ReliquaryCel.cs.
REM
REM The tool prints the frame width it produced. If that ever changes, OracoolStashChestAnimWidth in
REM Source/objdat.h must change with it: CEL stores no width, and a wrong one splits every RLE
REM scanline at the wrong point.
REM
REM Usage:  tools\build_reliquary_cel.cmd
REM Run from the repository root.

setlocal
set ART=..\Resources\02-source-art\world
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclstash.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\ReliquaryCel.exe

for %%S in (closed opening open) do (
  if not exist "%ART%\grand-reliquary-chest-v1.1.0-%%S-rgba-160x160.png" (
    echo ERROR: source art not found: %ART%\grand-reliquary-chest-v1.1.0-%%S-rgba-160x160.png
    exit /b 1
  )
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\ReliquaryCel.cs || exit /b 1
"%EXE%" "%ART%\grand-reliquary-chest-v1.1.0-closed-rgba-160x160.png" "%ART%\grand-reliquary-chest-v1.1.0-opening-rgba-160x160.png" "%ART%\grand-reliquary-chest-v1.1.0-open-rgba-160x160.png" "%PAL%" "%OUT%" "%TEMP%\reliquary_preview" || exit /b 1

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
endlocal

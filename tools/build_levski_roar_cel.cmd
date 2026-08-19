@echo off
REM Builds objects\orclroar.cel - Levski's Roar, the town monument - from the green-keyed painting
REM the user dropped in Oracool.MPQ's root on 2026-08-20, and installs it into both asset channels.
REM
REM Until now the monument borrowed OFILE_ROCKSTAN, the Anvil of Fury's rock stand, as an explicit
REM placeholder. This gives it its own art and its own object_graphic_id.
REM
REM The source is found by GLOB rather than by name: the drop-zone files arrive with generated names
REM containing a Cyrillic abbreviation, and a literal path did not survive tools\CutChestStates.ps1's
REM encoding. The timestamp is the stable part.
REM
REM 192 must equal OracoolLevskiRoarAnimWidth in Source/objdat.h - three town tiles wide. CEL stores
REM no width, so a mismatch splits every RLE scanline at the wrong point and renders the monument as
REM garbage. The tool prints the width it produced; check it.
REM
REM Usage:  tools\build_levski_roar_cel.cmd
REM Run from the repository root.

setlocal
set ART=
for %%F in ("..\Oracool.MPQ\*00_54_48*.png") do set ART=%%~fF
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclroar.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\MonumentCel.exe

if "%ART%"=="" (
  echo ERROR: no monument painting matching *00_54_48*.png in ..\Oracool.MPQ
  exit /b 1
)
echo Source: %ART%

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\MonumentCel.cs || exit /b 1
"%EXE%" "%ART%" "%PAL%" "%OUT%" 192 "%TEMP%\levski_roar_preview" || exit /b 1

REM Second channel: the loose assets folder, so a build that has not had oracool.mpq packed yet
REM still finds the sprite instead of dying on a missing file.
if not exist "Packaging\resources\assets\objects" mkdir "Packaging\resources\assets\objects"
copy /y "%OUT%" "Packaging\resources\assets\objects\orclroar.cel" >nul
if exist "build\x64-Debug\assets" (
  if not exist "build\x64-Debug\assets\objects" mkdir "build\x64-Debug\assets\objects"
  copy /y "%OUT%" "build\x64-Debug\assets\objects\orclroar.cel" >nul
)

echo.
echo orclroar.cel installed. Now run tools\build_oracool_mpq.cmd to repack the archive.
endlocal

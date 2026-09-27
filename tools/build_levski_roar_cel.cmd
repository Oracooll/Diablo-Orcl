@echo off
REM Builds objects\orclroar.cel - Levski's Roar, the town monument - from the green-keyed painting
REM the user dropped on 2026-08-20 and filed 2026-08-31 into 02. Oracooll Assets\02. Unused\world, and
REM installs it into both asset channels.
REM
REM Until now the monument borrowed OFILE_ROCKSTAN, the Anvil of Fury's rock stand, as an explicit
REM placeholder. This gives it its own art and its own object_graphic_id.
REM
REM The painting arrived in the drop zone under a generated name carrying a Cyrillic abbreviation,
REM so this script used to find it by GLOB on the timestamp - a literal path in an earlier cut
REM script did not survive that script's own encoding. The file was swept into 02. Oracooll Assets\02. Unused\world
REM on 2026-08-31 under an ASCII name, so it is referenced by name again.
REM
REM 96 must equal OracoolLevskiRoarAnimWidth in Source/objdat.h. CEL stores
REM no width, so a mismatch splits every RLE scanline at the wrong point and renders the monument as
REM garbage. The tool prints the width it produced; check it.
REM
REM Usage:  tools\build_levski_roar_cel.cmd
REM Run from the repository root.

setlocal
set ART=
set ART=..\Resources\02. Oracooll Assets\02. Unused\world\vasil-levski-monument-greenscreen.png
set PAL=tools\town.pal
REM Into oracool.mpq with everything else since the private archive was dissolved (2026-09-12).
set OUT=Packaging\resources\oracool_assets\objects\orclroar.cel
REM Desaturation before the palette match, percent (user, 2026-09-08: a stone monument, not a gold one).
set DESAT=75
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\MonumentCel.exe

if not exist "%ART%" (
  echo ERROR: monument painting not found: %ART%
  exit /b 1
)
echo Source: %ART%

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\MonumentCel.cs || exit /b 1
"%EXE%" "%ART%" "%PAL%" "%OUT%" 96 "%TEMP%\levski_roar_preview" %DESAT% || exit /b 1

echo.
echo orclroar.cel installed in the private asset folder. Now run tools\build_oracool_mpq.cmd to repack.
endlocal

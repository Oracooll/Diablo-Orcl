@echo off
REM Builds objects\orclstash.cel - the town Stash Chest as the gold-filled sarcophagus - from the
REM five-frame indexed sheet (user, 2026-09-08: "take l5sarco-gold.png and use it as stash chest.
REM opening animation and reverse for closing"). The sheet is Hellfire's l5sarco edited; it ships in
REM oracool.mpq like every other asset since the private archive was dissolved (2026-09-12).
REM
REM Frames 1..5, closed to open; the engine plays them forward on opening and backward on closing.
REM The frame width the tool prints must equal OracoolStashChestAnimWidth in Source/objdat.h (90 at 70%).
REM
REM Usage:  tools\build_stash_cel.cmd
REM Run from the repository root.

setlocal
set ART=..\Resources\01-in-use-assets\private-sweep\l5sarco-gold.png
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclstash.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\SarcoCel.exe

if not exist "%ART%" (
  echo ERROR: sarcophagus sheet not found: %ART%
  exit /b 1
)
if not exist "%PAL%" (
  echo ERROR: %PAL% not found - it is a Blizzard palette, staged locally and never committed
  exit /b 1
)

"%CSC%" /nologo /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\SarcoCel.cs || exit /b 1
REM 70: the sheet's 128x96 frame drawn at 90x67 (user, 2026-09-08: "reduce stash size by 30%").
set SCALE=70
"%EXE%" "%ART%" "%PAL%" "%OUT%" "%TEMP%\stash_preview" %SCALE% || exit /b 1

echo.
echo orclstash.cel installed in the private asset folder. Now run tools\build_oracool_mpq.cmd to repack.
endlocal

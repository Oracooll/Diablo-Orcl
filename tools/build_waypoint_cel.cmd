@echo off
REM Builds objects\orclwayp.cel - the waypoint platform's own sprite - from the user's two-state
REM painting, and installs it into both asset channels.
REM
REM Unlike every other asset this project ships, this one is a real Diablo .CEL rather than a PNG:
REM the waypoint is a world object, so it is loaded by the engine's own LoadCel/CelToClx path and
REM not by our hud_art machinery. See tools/WaypointCel.cs for the format and for why the sprite
REM has to quantise into palette indices 128-255 only.
REM
REM Usage:  tools\build_waypoint_cel.cmd
REM Run from the repository root.

setlocal
set ART=..\Resources\01-in-use-assets\world\waypoint-2-states.png
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclwayp.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\WaypointCel.exe

if not exist "%ART%" (
  echo ERROR: source art not found: %ART%
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\WaypointCel.cs || exit /b 1
"%EXE%" "%ART%" "%PAL%" "%OUT%" "%TEMP%\waypoint_preview" || exit /b 1

REM Second channel: the loose assets folder, so a build that has not had oracool.mpq packed yet
REM still finds the sprite instead of dying on a missing file.
if not exist "Packaging\resources\assets\objects" mkdir "Packaging\resources\assets\objects"
copy /y "%OUT%" "Packaging\resources\assets\objects\orclwayp.cel" >nul
if exist "build\x64-Debug\assets" (
  if not exist "build\x64-Debug\assets\objects" mkdir "build\x64-Debug\assets\objects"
  copy /y "%OUT%" "build\x64-Debug\assets\objects\orclwayp.cel" >nul
)

echo.
echo orclwayp.cel installed. Now run tools\build_oracool_mpq.cmd to repack the archive.
endlocal

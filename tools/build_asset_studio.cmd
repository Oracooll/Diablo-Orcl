@echo off
REM Builds OracoolAssetStudio.exe with the in-box .NET Framework compiler - no SDK, no bundled
REM runtime, no NuGet. Same approach as build_pcx_watcher.cmd; the result is a couple of hundred KB.
REM
REM Usage:  tools\build_asset_studio.cmd
REM Run from the repository root.

setlocal
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set OUT=tools\OracoolAssetStudio.exe

if not exist "%CSC%" (
  echo ERROR: in-box C# compiler not found at %CSC%
  exit /b 1
)

REM /target:winexe suppresses the console window.
"%CSC%" /nologo /target:winexe /out:"%OUT%" ^
  /reference:System.dll ^
  /reference:System.Drawing.dll ^
  /reference:System.Windows.Forms.dll ^
  tools\OracoolAssetStudio.cs tools\OracoolAssetStudioForm.cs || exit /b 1

REM Put the town palette beside the exe so it loads with no setup. Its global half (entries
REM 128-255) is what HUD and UI art is quantized against, and that half is identical across town
REM and every dungeon type - so this one file is correct for all of them.
set PAL=..\Oracool.MPQ\99-original-game-art\raw\levels\towndata\town.pal
if exist "%PAL%" (
  copy /y "%PAL%" tools\town.pal >nul
  echo Palette staged: tools\town.pal
) else (
  echo NOTE: town.pal not found at "%PAL%" - load a palette from the app instead.
)

echo Built %OUT%
endlocal

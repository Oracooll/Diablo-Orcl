@echo off
REM Builds Levski's Cube's animated sheet (2026-10-01) from ChatGPT's green-screen stills - see tools\LevskiCubeSheet.cs:
REM   Packaging\resources\oracool_assets\objects\levski_cube.png   (then tools\build_oracool_mpq.cmd)
REM   Source\oracool\levski_cube_frames.inc                        (then rebuild the game)
REM Usage:  tools\build_levski_cube_sheet.cmd   (from the repository root)
setlocal
set SRC=..\Resources\02. Oracooll Assets\01. Used\levski-cube\green-background
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\LevskiCubeSheet.exe
"%CSC%" /nologo /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\LevskiCubeSheet.cs || exit /b 1
"%EXE%" "%SRC%" Packaging\resources\oracool_assets\objects\levski_cube.png Source\oracool\levski_cube_frames.inc "%TEMP%\levski_cube_preview.png" || exit /b 1
endlocal

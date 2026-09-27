@echo off
REM Builds objects\orclroar.cel - Levski's Cube, the town object that opens the transmute window - from the
REM thirteen real-alpha frames of batch 43a (RfA-20, 2026-09-20): frames 0-11 the idle loop (rune glow),
REM frame 12 the open pose while the window is up. The frames are the CLEANED copies in
REM 02. Oracooll Assets\01. Used (tools\CleanCubeLabel.ps1 paints out the bright LEVSKI label under the cube; the
REM plinth's carving stays), not the delivered pack.
REM
REM 96 must equal OracoolLevskiRoarAnimWidth in Source/objdat.h. CEL stores no width, so a mismatch
REM splits every RLE scanline at the wrong point. The tool prints the width it produced; check it.
REM
REM Usage:  tools\build_levski_cube_cel.cmd
REM Run from the repository root. Re-run tools\CleanCubeLabel.ps1 first if the delivery changed.

setlocal
REM 2026-09-20, later: the user's own painting replaces batch 43a outright ("take this asset and replace
REM current levski's cube with it" - Resources\02. Oracooll Assets\01. Used\Levski's Cube.png, 1254x1254, scaled to 96x94 by
REM tools\ScaleLevskiCube.ps1 into levski-cube-user\). ONE frame, no idle loop: ProcessLevskiCubeAnimation
REM leaves a sheet shorter than thirteen frames alone. The cleaned batch-43a frames stay in levski-cube\.
set FRAMES=..\Resources\02. Oracooll Assets\01. Used\levski-cube-user
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclroar.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\FramesCel.exe

if not exist "%FRAMES%\levski_cube.png" (
  echo ERROR: the Cube frame not found under %FRAMES% - run tools\ScaleLevskiCube.ps1
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\FramesCel.cs || exit /b 1
"%EXE%" "%FRAMES%" "levski_cube.png" "%PAL%" "%OUT%" "%TEMP%\levski_cube_preview" 0 || exit /b 1
endlocal

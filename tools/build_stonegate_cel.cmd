@echo off
REM Builds objects\orclgate.cel - the Stonegate, the town monument the rift portals open in - from the
REM seventeen real-alpha frames of batch 44 (RfA-21, 2026-09-20; batch 42's frames were cut to a
REM mis-measured portal and stand three times too tall): frame 0 the closed gate, 1-8 the gate lit
REM gold, 9-16 the gate lit violet. The portal itself is a missile drawn inside the opening.
REM
REM The frame width (128) must equal OracoolStonegateAnimWidth in Source/objdat.h. CEL stores no width,
REM so a mismatch splits every RLE scanline at the wrong point. The tool prints the width it produced.
REM
REM Usage:  tools\build_stonegate_cel.cmd
REM Run from the repository root.

setlocal
set FRAMES=..\Resources\02-concept-assets\delivered-packs\batch-44-stonegate-second-cut\monument
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclgate.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\FramesCel.exe

if not exist "%FRAMES%\stonegate_closed.png" (
  echo ERROR: stonegate frames not found under %FRAMES%
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\FramesCel.cs || exit /b 1
"%EXE%" "%FRAMES%" "stonegate_closed.png;stonegate_gold_*.png;stonegate_purple_*.png" "%PAL%" "%OUT%" "%TEMP%\stonegate_preview" 0 || exit /b 1

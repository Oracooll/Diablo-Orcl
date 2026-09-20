@echo off
REM Builds objects\orclgate.cel - the Rift Monument (called the Stonegate until 2026-09-20), the town
REM monument the rift portals open in. ONE frame since 2026-09-20: the user's own painting
REM (Resources\Rift Monument.png, 1161x1355 with real alpha) scaled to 128 wide by tools\ScalePainting.ps1 and TONED
REM to worn grey stone (user, 2026-09-20: "less bright and more worn-down stone grey-ish"), then cast onto the town
REM palette's own blue-grey ramp (entries 178-188, the tone of Tristram's rocks - "they are very blue-ish"; a cast
REM between that ramp and the neutral greys speckled at quantisation): -Saturation 0.45
REM -Brightness 0.78 -CoolCast 0.04.
REM The portal is a missile drawn over it (AddRiftPortal lifts it 20px into the arch). The seventeen-frame
REM batch 45 cut (closed + eight gold + eight violet, RfA-22) is superseded and stays filed under
REM 02-concept-assets\delivered-packs.
REM
REM The frame width (128) must equal OracoolStonegateAnimWidth in Source/objdat.h. CEL stores no width,
REM so a mismatch splits every RLE scanline at the wrong point. The tool prints the width it produced.
REM
REM Usage:  tools\build_stonegate_cel.cmd
REM Run from the repository root. If the frame is missing, run first:
REM   powershell -File tools\ScalePainting.ps1 -Source "..\Resources\Rift Monument.png" -OutDir "..\Resources\01-in-use-assets\objects\rift-monument-user" -OutName rift_monument.png -Width 128 -Saturation 0.45 -Brightness 0.78 -TintRgb "121,127,160" -TintStrength 1.0

setlocal
set FRAMES=..\Resources\01-in-use-assets\objects\rift-monument-user
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclgate.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\FramesCel.exe

if not exist "%FRAMES%\rift_monument.png" (
  echo ERROR: rift_monument.png not found under %FRAMES% - run tools\ScalePainting.ps1 first, see the header
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\FramesCel.cs || exit /b 1
"%EXE%" "%FRAMES%" "rift_monument.png" "%PAL%" "%OUT%" "%TEMP%\stonegate_preview" 0 || exit /b 1
endlocal

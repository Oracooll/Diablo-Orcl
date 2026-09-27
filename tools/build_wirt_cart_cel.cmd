@echo off
REM Builds objects\orclcart.cel - Wirt's merchant cart, the scenery that stands beside him in town
REM (user, 2026-09-22, with a paint.net composite showing where it goes).
REM
REM The user's painting (Resources\Wirt Cart.png, 209x209 with real alpha) scaled to 160 wide by
REM tools\ScalePainting.ps1 and quantised onto the town palette by FramesCel.cs. NO toning: unlike
REM the Rift Monument this is a painted wooden cart meant to read as a merchant's stall, not as
REM Tristram's blue rock, so it is cast as painted.
REM
REM The frame width (160) must equal OracoolWirtCartAnimWidth in Source/objdat.h. CEL stores no
REM width, so a mismatch splits every RLE scanline at the wrong point. The tool prints the width it
REM produced - check it.
REM
REM Well inside the hover outline's 253-pixel sprite limit (clx_render.cpp MaxOutlineSpriteWidth),
REM which the Rift Monument's shadowed 372-wide frame tripped - and moot here anyway, because the
REM cart is unselectable scenery and never draws an outline.
REM
REM Usage:  tools\build_wirt_cart_cel.cmd
REM Run from the repository root. If the frame is missing, run first:
REM   powershell -File tools\ScalePainting.ps1 -Source "..\Resources\Wirt Cart.png" -OutDir "..\Resources\02. Oracooll Assets\01. Used\wirt-cart-user" -OutName wirt_cart.png -Width 160

setlocal
set FRAMES=..\Resources\02. Oracooll Assets\01. Used\wirt-cart-user
set PAL=tools\town.pal
set OUT=Packaging\resources\oracool_assets\objects\orclcart.cel
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set EXE=%TEMP%\FramesCel.exe

if not exist "%FRAMES%\wirt_cart.png" (
  echo ERROR: wirt_cart.png not found under %FRAMES% - run tools\ScalePainting.ps1 first, see the header
  exit /b 1
)

"%CSC%" /nologo /unsafe /optimize /target:exe /out:"%EXE%" /r:System.Drawing.dll tools\FramesCel.cs || exit /b 1
"%EXE%" "%FRAMES%" "wirt_cart.png" "%PAL%" "%OUT%" "%TEMP%\wirt_cart_preview" 0 || exit /b 1
endlocal

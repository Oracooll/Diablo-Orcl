@echo off
REM Recolours the bottom HUD art to match the darkened inventory panel.
REM
REM Always reads from tools\hud_source\ - a pristine snapshot taken before the first recolour - so
REM running this repeatedly is idempotent. Never point it at the installed assets as input, or
REM every run will desaturate what the previous run already desaturated.
REM
REM Usage:  tools\build_hud_recolour.cmd
REM Run from the repository root.

setlocal
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set SRC=tools\hud_source
set OUT=Packaging\resources\assets\ui

if not exist "%CSC%" (
  echo ERROR: in-box C# compiler not found at %CSC%
  exit /b 1
)
if not exist "%SRC%\middle_hud.png" (
  echo ERROR: pristine HUD snapshot missing at "%SRC%"
  exit /b 1
)

"%CSC%" /nologo /target:exe /out:"%TEMP%\HudRecolour.exe" /reference:System.Drawing.dll tools\HudRecolour.cs || exit /b 1
"%TEMP%\HudRecolour.exe" "%SRC%" "%OUT%" || exit /b 1

if exist build\x64-Debug\assets\ui (
  copy /y "%OUT%\middle_hud.png" build\x64-Debug\assets\ui\ >nul
  copy /y "%OUT%\health_orb.png" build\x64-Debug\assets\ui\ >nul
  copy /y "%OUT%\mana_orb.png"   build\x64-Debug\assets\ui\ >nul
  copy /y "%OUT%\menu_icons.png" build\x64-Debug\assets\ui\ >nul
  echo Mirrored into build\x64-Debug\assets\ui
)

echo Done.
endlocal

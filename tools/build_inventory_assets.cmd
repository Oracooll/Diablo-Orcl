@echo off
REM Rebuilds the Oracool V1 inventory panel and tab strip from the artist's component files.
REM
REM InvCompose.cs carries a copy of the panel geometry (cell pitch, grid origin, equipment
REM slot table). It MUST stay in step with Source/oracool/inventory_layout.h - if you move a
REM slot there, move it here too and re-run this, or the art and the hit-testing will disagree.
REM
REM Usage:  tools\build_inventory_assets.cmd
REM Run from the repository root.

setlocal
set CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe
set SRC=..\Oracool.MPQ\01-in-use\inventory-panel
set OUT=Packaging\resources\assets\ui

if not exist "%CSC%" (
  echo ERROR: in-box C# compiler not found at %CSC%
  exit /b 1
)
if not exist "%SRC%\panel-background-320x660.png" (
  echo ERROR: component art not found at "%SRC%"
  exit /b 1
)

"%CSC%" /nologo /target:exe /out:"%TEMP%\AssetCut.exe"   /reference:System.Drawing.dll tools\AssetCut.cs   || exit /b 1
"%CSC%" /nologo /target:exe /out:"%TEMP%\InvCompose.exe" /reference:System.Drawing.dll tools\InvCompose.cs || exit /b 1
"%CSC%" /nologo /target:exe /out:"%TEMP%\InvPreview.exe" /reference:System.Drawing.dll tools\InvPreview.cs || exit /b 1

REM Cuts the tab numerals, the SORT button and the class sygil. Sizes inside AssetCut.cs must
REM match TabCellSize / SortButtonSize / SygilSize in inventory_layout.h.
"%TEMP%\AssetCut.exe" "%SRC%" "%OUT%" || exit /b 1

REM Bakes the background, silhouette, slot frames and sygil into one flat panel.
"%TEMP%\InvCompose.exe" "%SRC%" "%OUT%\inventory_panel.png" "%OUT%\inventory_sygil.png" || exit /b 1

REM Whole-panel preview for eyeballing the layout; last arg is the selected tab.
"%TEMP%\InvPreview.exe" "%OUT%" "%TEMP%\inventory_preview.png" 0 || exit /b 1

REM Mirror into the build tree so a Debug run picks them up without a full asset copy.
if exist build\x64-Debug\assets\ui (
  copy /y "%OUT%\inventory_panel.png" build\x64-Debug\assets\ui\ >nul
  copy /y "%OUT%\inventory_tabs.png"  build\x64-Debug\assets\ui\ >nul
  copy /y "%OUT%\inventory_sort.png"  build\x64-Debug\assets\ui\ >nul
  echo Mirrored into build\x64-Debug\assets\ui
)
echo Preview written to %TEMP%\inventory_preview.png

echo Done.
endlocal

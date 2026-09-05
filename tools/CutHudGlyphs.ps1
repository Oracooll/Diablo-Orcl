# CutHudGlyphs.ps1 - builds the burger-menu and inventory-tab glyph strips from oracool-hud-glyphs-v1.
#
# Source: Oracool.MPQ\02-source-art\delivered-packs\oracool-hud-glyphs-v1 (2026-09-06): eight 37x38
# menu glyphs (character, quests, runewords, game_menu, inventory, spellbook, crafting, event_log -
# the order of hud_menu.cpp's MenuEntries) and ten 28x28 tab numerals, white (243,243,243) and
# shadow (12,7,7) on transparency.
#
# Output, in Packaging\resources\oracool_assets\ui:
#   ui\menu_glyphs.png  304x38 - eight 38x38 cells; the 37-wide glyph sits at x=0 of its cell, the
#                       38th column transparent, because DrawStripIcon takes square cells (cell =
#                       strip height) and the vanilla plate is 37x38.
#   ui\tab_glyphs.png   280x28 - ten 28x28 cells, numeral 1 first.
# Re-run tools\build_oracool_mpq.cmd afterwards: a normal build does NOT repack oracool.mpq.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutHudGlyphs.ps1
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\02-source-art\delivered-packs\oracool-hud-glyphs-v1\glyphs'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'

function IsGlyphPixel([System.Drawing.Color]$c) {
  if ($c.A -eq 0) { return $true }
  if ($c.A -ne 255) { return $false }
  return (($c.R -eq 243 -and $c.G -eq 243 -and $c.B -eq 243) -or ($c.R -eq 12 -and $c.G -eq 7 -and $c.B -eq 7))
}

function BuildStrip([string[]]$files, [int]$cell, [int]$srcW, [int]$srcH, [string]$outName) {
  $strip = New-Object System.Drawing.Bitmap ($cell * $files.Count), $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($strip)
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.Dispose()
  for ($i = 0; $i -lt $files.Count; $i++) {
    $file = $files[$i]
    if (-not (Test-Path $file)) { throw "missing $file" }
    $src = New-Object System.Drawing.Bitmap $file
    if ($src.Width -ne $srcW -or $src.Height -ne $srcH) { throw "$file is $($src.Width)x$($src.Height), not ${srcW}x${srcH}" }
    for ($y = 0; $y -lt $srcH; $y++) { for ($x = 0; $x -lt $srcW; $x++) {
      $c = $src.GetPixel($x, $y)
      if (-not (IsGlyphPixel $c)) { throw "$file has a non-glyph pixel at $x,$y : $c" }
      if ($c.A -eq 255) { $strip.SetPixel($i * $cell + $x, $y, $c) }
    } }
    $src.Dispose()
  }
  $path = Join-Path $outDir $outName
  $strip.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $strip.Dispose()
  Write-Host "wrote $path ($($cell * $files.Count)x$cell)"
}

$menu = @('character', 'quests', 'runewords', 'game_menu', 'inventory', 'spellbook', 'crafting', 'event_log') | ForEach-Object { Join-Path $pack "menu\$_.png" }
BuildStrip $menu 38 37 38 'menu_glyphs.png'
$tabs = 1..10 | ForEach-Object { Join-Path $pack "tabs\$_.png" }
BuildStrip $tabs 28 28 28 'tab_glyphs.png'

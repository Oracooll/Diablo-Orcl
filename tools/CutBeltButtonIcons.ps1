# CutBeltButtonIcons.ps1 - cuts ui\burger_menu_button.png and ui\town_portal_icon.png from GPT's
# 112px source states, at the belt's size, with the default state lifted.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutBeltButtonIcons.ps1
#
# Source: Resources\01-in-use-assets\delivered-packs\diablo-bottom-hud-v1\icons\{burger-menu,town-portal}\
# source-112\*-{default,hover,click}-112x112.png. Output: one strip per button, three cells side by
# side in that order, indexed state * cell by hud_art.cpp's DrawBurgerMenuButton / DrawTownPortalIcon.
#
# 31px, not 28 (user, 2026-09-05: "scale them 10% together with their hover and click versions"):
# the belt cells are 28x30, so the icon overhangs the rim by a pixel or two, which is what "10%
# bigger" means on a cell that size. Cut from the 112px sources rather than upscaled from the
# delivered 28px runtime strips, so the extra pixels are real.
#
# The DEFAULT state is lifted (user: "make burger default icon and portal default icon brighter.
# they are too dim now") - a gamma of 0.7 on its RGB, the same kind of lift the side-panel canvas
# takes, so the midtones come up and the highlights stay put. Hover takes the SAME lift, so it
# still reads brighter than default as delivered - lifting default alone inverted the cue. Click is as delivered.

param([double] $DefaultGamma = 0.7, [double] $HoverGamma = 0.7, [int] $Cell = 28)   # 28 since the compact belt (29px pitch); 31 for the carved HUD's wider cells

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Resources\01-in-use-assets\delivered-packs\diablo-bottom-hud-v1\icons'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'

$lut = New-Object byte[] 256; $hlut = New-Object byte[] 256
for ($i = 0; $i -lt 256; $i++) { $lut[$i] = [byte][math]::Round(255.0 * [math]::Pow($i / 255.0, $DefaultGamma)); $hlut[$i] = [byte][math]::Round(255.0 * [math]::Pow($i / 255.0, $HoverGamma)) }

function Strip($folder, $stem, $outName) {
  $strip = New-Object System.Drawing.Bitmap ($Cell * 3), $Cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($strip)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
  $states = @('default', 'hover', 'click')
  for ($s = 0; $s -lt 3; $s++) {
    $src = New-Object System.Drawing.Bitmap (Join-Path $pack "$folder\source-112\$stem-$($states[$s])-112x112.png")
    if ($src.Width -ne 112 -or $src.Height -ne 112) { throw "$stem $($states[$s]) is $($src.Width)x$($src.Height), expected 112x112" }
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle -ArgumentList ($s * $Cell), 0, $Cell, $Cell), (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, 112, 112), 'Pixel')
    $src.Dispose()
  }
  $g.Dispose()
  # the default cell's lift, and binary alpha throughout (the game keys on >= 128)
  for ($y = 0; $y -lt $Cell; $y++) { for ($x = 0; $x -lt $Cell * 3; $x++) {
    $c = $strip.GetPixel($x, $y)
    if ($c.A -lt 128) { $strip.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
    if ($x -lt $Cell) { $strip.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $lut[$c.R], $lut[$c.G], $lut[$c.B])) }
    elseif ($x -lt 2 * $Cell) { $strip.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $hlut[$c.R], $hlut[$c.G], $hlut[$c.B])) }
    else { $strip.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $c.R, $c.G, $c.B)) }
  } }
  $strip.Save((Join-Path $outDir $outName), [System.Drawing.Imaging.ImageFormat]::Png)
  $strip.Dispose()
  Write-Host ("{0,-24} {1}x{2}  default gamma {3}" -f $outName, ($Cell * 3), $Cell, $DefaultGamma)
}

Strip 'burger-menu' 'burger-menu' 'burger_menu_button.png'
Strip 'town-portal' 'town-portal' 'town_portal_icon.png'

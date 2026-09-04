# CutLevskiRoarSkin.ps1 - cuts the painted Levski's Roar skin into shipped assets and a geometry header.
#
# Reads  Oracool.MPQ/02-source-art/delivered-packs/oracool-levski-roar-skin/
# Writes Packaging/resources/oracool_assets/ui/levski_bg.png and twenty levski_<button>_<state>.png
#        Source/oracool/levski_roar_skin.h  (window size, grid, ten rects - GENERATED, do not edit)
#
# WHY A HEADER. The window's hit rects and the art's plates have to agree to the pixel, and the art
# is resampled from a 1122x1402 painting. Measuring the painting here and emitting the numbers is
# the only way the two cannot drift: change the source rects below, re-run, rebuild.
#
# THE SCALE. The painting's grid cells are ~185px; the game's item cell is 28px and item sprites are
# cut to it. Items in this grid are drawn at 3x (84px cells), so the whole window is drawn at 84/185.
#
# THE STATE SHEETS. Each is two plates on a gradient: LEFT lit (hover), RIGHT plain (pressed). The
# plate is found as the bounding box of "not gradient" pixels - dark, or saturated (gold, gems, the
# red Transmute) - and the gradient inside that box is keyed out. Three sheets have the left plate
# cut off at x=0; those use the right plate for both states, and the game marks hover otherwise.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\02-source-art\delivered-packs\oracool-levski-roar-skin'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\levski_roar_skin.h'

# ---- measured off levski-roar-background.png (2026-09-04) ------------------------------------
$srcW = 1122; $srcH = 1402
$cellPitch = 185                       # centre-to-centre of the painted 3x4 grid
$gridOrigin = @{ X = 113; Y = 430 }    # top-left of the first cell's interior
$gameCell = 84                         # 3 x the 28px inventory cell
$scale = $gameCell / $cellPitch

# name, source rect (x y w h), sheet file
$buttons = @(
  @{ Name = 'close';      Rect = @(975, 115,  72,  68); Sheet = 'button-close-states.png' },
  @{ Name = 'transmute';  Rect = @(118, 1172, 512, 118); Sheet = 'button-transmute-states.png' },
  @{ Name = 'recipes';    Rect = @(678, 1178, 352, 110); Sheet = 'button-recipe-book-states.png' },
  @{ Name = 'basic';      Rect = @(745, 469,  282,  80); Sheet = 'button-basic-states.png' },
  @{ Name = 'magic';      Rect = @(745, 561,  282,  80); Sheet = 'button-magic-states.png' },
  @{ Name = 'rare';       Rect = @(745, 653,  282,  80); Sheet = 'button-rare-states.png' },
  @{ Name = 'unique';     Rect = @(745, 745,  282,  80); Sheet = 'button-unique-states.png' },
  @{ Name = 'set';        Rect = @(745, 837,  282,  80); Sheet = 'button-set-states.png' },
  @{ Name = 'primal';     Rect = @(745, 929,  282,  80); Sheet = 'button-primal-states.png' },
  @{ Name = 'ethereal';   Rect = @(745, 1021, 282,  80); Sheet = 'button-ethereal-states.png' }
)

function Scaled($v) { return [int][math]::Round($v * $scale) }

function Resample([System.Drawing.Image]$src, [System.Drawing.Rectangle]$crop, [int]$w, [int]$h) {
  $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), $crop, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose()
  return $bmp
}

function IsPlatePixel([System.Drawing.Color]$c) {
  $l = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
  $sat = [math]::Max($c.R, [math]::Max($c.G, $c.B)) - [math]::Min($c.R, [math]::Min($c.G, $c.B))
  return ($l -lt 95) -or ($sat -gt 70)
}

# The plate's GOLD - its border, ornaments and lettering. The gradient behind the plates runs from
# warm beige to near-black and matches a luma test on its dark half, so the box is found from the
# gold alone, which nothing in the gradient is; the dark interior sits inside that border anyway.
function IsGoldPixel([System.Drawing.Color]$c) {
  return ($c.R -gt 110) -and (($c.R - $c.B) -gt 55) -and ($c.G -ge $c.B)
}

# The plate inside one half of a sheet: bounding box of its gold, with a small margin.
function PlateBox([System.Drawing.Bitmap]$b, [int]$x0, [int]$x1) {
  $minX = $x1; $maxX = $x0; $minY = $b.Height; $maxY = 0
  for ($y = 0; $y -lt $b.Height; $y += 2) {
    for ($x = $x0; $x -lt $x1; $x += 2) {
      if (IsGoldPixel $b.GetPixel($x, $y)) {
        if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
        if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
      }
    }
  }
  $m = 4
  $minX = [math]::Max($x0, $minX - $m); $maxX = [math]::Min($x1 - 1, $maxX + $m)
  $minY = [math]::Max(0, $minY - $m);   $maxY = [math]::Min($b.Height - 1, $maxY + $m)
  return New-Object System.Drawing.Rectangle $minX, $minY, ($maxX - $minX + 1), ($maxY - $minY + 1)
}

# Key the gradient out of a cut plate: light, unsaturated pixels become transparent.
function KeyOutGradient([System.Drawing.Bitmap]$b) {
  for ($y = 0; $y -lt $b.Height; $y++) {
    for ($x = 0; $x -lt $b.Width; $x++) {
      $c = $b.GetPixel($x, $y)
      if (-not (IsPlatePixel $c)) { $b.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)) }
    }
  }
}

$winW = Scaled $srcW; $winH = Scaled $srcH
Write-Host ("window {0}x{1}  scale {2:N4}" -f $winW, $winH, $scale)

# ---- the background --------------------------------------------------------------------------
$bg = [System.Drawing.Image]::FromFile((Join-Path $pack 'levski-roar-background.png'))
$bgOut = Resample $bg (New-Object System.Drawing.Rectangle 0, 0, $srcW, $srcH) $winW $winH
$bgOut.Save((Join-Path $outDir 'levski_bg.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bgOut.Dispose(); $bg.Dispose()
Write-Host "levski_bg.png"

# ---- the buttons -----------------------------------------------------------------------------
$lines = @()
$cropped = @()
foreach ($btn in $buttons) {
  $r = $btn.Rect
  $gx = Scaled $r[0]; $gy = Scaled $r[1]; $gw = Scaled $r[2]; $gh = Scaled $r[3]
  $lines += ("`t{{ {{ {0}, {1} }}, {{ {2}, {3} }} }}, // {4}" -f $gx, $gy, $gw, $gh, $btn.Name)

  $sheet = New-Object System.Drawing.Bitmap (Join-Path $pack $btn.Sheet)
  $half = [int]($sheet.Width / 2)
  $left = PlateBox $sheet 0 $half
  $right = PlateBox $sheet $half $sheet.Width
  $leftCut = $left.X -le 3
  if ($leftCut) { $cropped += $btn.Name }
  $states = @{ hover = $(if ($leftCut) { $right } else { $left }); pressed = $right }
  foreach ($state in $states.Keys) {
    $plate = Resample $sheet $states[$state] $gw $gh
    KeyOutGradient $plate
    $plate.Save((Join-Path $outDir ("levski_{0}_{1}.png" -f $btn.Name, $state)), [System.Drawing.Imaging.ImageFormat]::Png)
    $plate.Dispose()
  }
  $sheet.Dispose()
  Write-Host ("{0,-10} game rect {1},{2} {3}x{4}  left plate {5}  right plate {6}{7}" -f $btn.Name, $gx, $gy, $gw, $gh, $left, $right, $(if ($leftCut) { '  [left CUT OFF - right used for hover]' } else { '' }))
}

# ---- the header ------------------------------------------------------------------------------
$croppedList = if ($cropped.Count -gt 0) { ($cropped -join ', ') } else { 'none' }
$h = @"
/**
 * @file oracool/levski_roar_skin.h
 *
 * GENERATED by tools/CutLevskiRoarSkin.ps1 - do not edit. Change the measurements at the top of
 * that script and re-run it; the art it writes and these numbers come from the same pass.
 *
 * The painted Levski's Roar window (Oracool.MPQ/02-source-art/delivered-packs/
 * oracool-levski-roar-skin), resampled to $($winW)x$($winH) - the scale at which its ~$($cellPitch)px
 * painted grid cells become 3x the game's 28px item cell. Every rect below is in WINDOW pixels.
 *
 * State sheets whose lit plate was cut off in delivery, and so wear the plain plate for hover too:
 * $croppedList.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::levski_skin {

constexpr Size WindowSize { $winW, $winH };
constexpr int CellSize = $gameCell;
constexpr Point GridOrigin { $(Scaled $gridOrigin.X), $(Scaled $gridOrigin.Y) };

/** The ten plates, in ButtonIndex order. */
enum ButtonIndex : int {
	Close = 0,
	Transmute = 1,
	Recipes = 2,
	SalvageFirst = 3, // Basic; the seven salvage tiers follow in SalvageTier order
};
constexpr int ButtonCount = $($buttons.Count);

constexpr Rectangle ButtonRects[ButtonCount] = {
$($lines -join "`n")
};

/** ui\ asset stem per button - levski_<stem>_hover.png and levski_<stem>_pressed.png. */
constexpr const char *ButtonStems[ButtonCount] = {
$(($buttons | ForEach-Object { "`t`"$($_.Name)`"," }) -join "`n")
};

/** True where the hover file is the PLAIN plate (the lit one was cut off in delivery), so the game
 * has to mark hover some other way. */
constexpr bool HoverIsPlain[ButtonCount] = {
$(($buttons | ForEach-Object { if ($cropped -contains $_.Name) { "`ttrue, // $($_.Name)" } else { "`tfalse, // $($_.Name)" } }) -join "`n")
};

} // namespace devilution::oracool::levski_skin
"@
[System.IO.File]::WriteAllText($header, $h.Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote $header"

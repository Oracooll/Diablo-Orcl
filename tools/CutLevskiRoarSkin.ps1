# CutLevskiRoarSkin.ps1 - cuts the painted Levski's Roar skin into shipped assets and a geometry header.
#
# Reads  Oracool.MPQ/02-source-art/delivered-packs/oracool-levski-roar-skin/levski-roar-v2-447x559.png
# Writes Packaging/resources/oracool_assets/ui/levski_bg.png
#        Source/oracool/levski_roar_skin.h  (window size, grid, ten rects - GENERATED, do not edit)
#
# WHY A HEADER. The window's hit rects and the art's plates have to agree to the pixel, and the art
# is resampled from the painting. Measuring the painting here and emitting the numbers is the only
# way the two cannot drift: change the source rects below, re-run, rebuild.
#
# THE SECOND PAINTING (2026-09-04, "there is a newer version of levskis roar in oracool.mpq"). 447x559,
# an 8x10 grid, and painted very nearly at game size: its cells are 29.4 x 28.1 px. The two axes are
# scaled SEPARATELY so both land on the game's 28px cell - a 5% squeeze across, none down - which
# no eye picks up on stonework and which is what lets the game's 28px items sit in the painted cells.
# It came without state sheets, so no levski_<button>_<state>.png is written; the game marks hover and
# press with an outline and a shade when a state file is missing (levski_roar.cpp).
#
# The first painting (1122x1402, 3x4 grid, ten state sheets) is still in the pack folder; this
# script's history has the cut for it (v1.9.203-204).

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\02-source-art\delivered-packs\oracool-levski-roar-skin'
$source = Join-Path $pack 'levski-roar-v2-447x559.png'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\levski_roar_skin.h'

# ---- measured off levski-roar-v2-447x559.png (2026-09-04, luma profiles + a 2x overlay check) -----
$srcW = 447; $srcH = 559
$gridColumns = 8; $gridRows = 10
$pitchX = 29.4; $pitchY = 28.1           # centre-to-centre of the painted grid, per axis
$gridOrigin = @{ X = 39.5; Y = 167 }     # top-left of the first cell's interior
$gameCell = 28                           # the inventory cell (user, 2026-09-04: "regular game size")
$scaleX = $gameCell / $pitchX
$scaleY = $gameCell / $pitchY

# name, source rect (x y w h) - the plate's outer gold edge
$buttons = @(
  @{ Name = 'close';      Rect = @(386, 46,  27, 28) },
  @{ Name = 'transmute';  Rect = @(50,  474, 196, 38) },
  @{ Name = 'recipes';    Rect = @(270, 473, 140, 38) },
  @{ Name = 'basic';      Rect = @(301, 188, 106, 31) },
  @{ Name = 'magic';      Rect = @(301, 224, 106, 31) },
  @{ Name = 'rare';       Rect = @(301, 260, 106, 31) },
  @{ Name = 'unique';     Rect = @(301, 296, 106, 31) },
  @{ Name = 'set';        Rect = @(301, 332, 106, 31) },
  @{ Name = 'primal';     Rect = @(301, 368, 106, 31) },
  @{ Name = 'ethereal';   Rect = @(301, 404, 106, 31) }
)

function ScaledX($v) { return [int][math]::Round($v * $scaleX) }
function ScaledY($v) { return [int][math]::Round($v * $scaleY) }

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

$winW = ScaledX $srcW; $winH = ScaledY $srcH
Write-Host ("window {0}x{1}  scale {2:N4} x {3:N4}" -f $winW, $winH, $scaleX, $scaleY)

# ---- the background --------------------------------------------------------------------------
$bg = [System.Drawing.Image]::FromFile($source)
if ($bg.Width -ne $srcW -or $bg.Height -ne $srcH) { throw "expected ${srcW}x${srcH}, got $($bg.Width)x$($bg.Height)" }
$bgOut = Resample $bg (New-Object System.Drawing.Rectangle 0, 0, $srcW, $srcH) $winW $winH
$bgOut.Save((Join-Path $outDir 'levski_bg.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bgOut.Dispose(); $bg.Dispose()
Write-Host "levski_bg.png"

# Stale state files from the first painting must not lie over the new plates.
Get-ChildItem $outDir -Filter 'levski_*_hover.png' | Remove-Item
Get-ChildItem $outDir -Filter 'levski_*_pressed.png' | Remove-Item

# ---- the buttons -----------------------------------------------------------------------------
$lines = @()
foreach ($btn in $buttons) {
  $r = $btn.Rect
  $gx = ScaledX $r[0]; $gy = ScaledY $r[1]; $gw = ScaledX $r[2]; $gh = ScaledY $r[3]
  $lines += ("`t{{ {{ {0}, {1} }}, {{ {2}, {3} }} }}, // {4}" -f $gx, $gy, $gw, $gh, $btn.Name)
  Write-Host ("{0,-10} game rect {1},{2} {3}x{4}" -f $btn.Name, $gx, $gy, $gw, $gh)
}

# ---- the header ------------------------------------------------------------------------------
$h = @"
/**
 * @file oracool/levski_roar_skin.h
 *
 * GENERATED by tools/CutLevskiRoarSkin.ps1 - do not edit. Change the measurements at the top of
 * that script and re-run it; the art it writes and these numbers come from the same pass.
 *
 * The painted Levski's Roar window (Oracool.MPQ/02-source-art/delivered-packs/
 * oracool-levski-roar-skin/levski-roar-v2-447x559.png), resampled to $($winW)x$($winH) - the scale at
 * which its painted grid cells ($($pitchX) x $($pitchY) px) become the game's 28px item cell. Every rect
 * below is in WINDOW pixels.
 *
 * This painting came without hover/pressed sheets: no levski_<stem>_<state>.png exists, and the
 * game marks state with an outline and a shade instead (HoverIsPlain is true throughout).
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::levski_skin {

constexpr Size WindowSize { $winW, $winH };
constexpr int CellSize = $gameCell;
constexpr int GridColumns = $gridColumns;
constexpr int GridRows = $gridRows;
constexpr Point GridOrigin { $(ScaledX $gridOrigin.X), $(ScaledY $gridOrigin.Y) };

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

/** ui\ asset stem per button - levski_<stem>_hover.png and levski_<stem>_pressed.png, if they exist. */
constexpr const char *ButtonStems[ButtonCount] = {
$(($buttons | ForEach-Object { "`t`"$($_.Name)`"," }) -join "`n")
};

/** True where the hover file, if any, is the PLAIN plate, so the game has to mark hover itself. */
constexpr bool HoverIsPlain[ButtonCount] = {
$(($buttons | ForEach-Object { "`ttrue, // $($_.Name)" }) -join "`n")
};

} // namespace devilution::oracool::levski_skin
"@
[System.IO.File]::WriteAllText($header, $h.Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote $header"

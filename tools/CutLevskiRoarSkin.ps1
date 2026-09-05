# CutLevskiRoarSkin.ps1 - cuts the painted Levski's Roar skin into shipped assets and a geometry header.
#
# Reads  Oracool.MPQ/levskis roar.png                       (the user's own painting, 261x241, 3x4 grid)
#        Oracool.MPQ/02-source-art/delivered-packs/oracool-levski-icon-controls-v3/icons-32/*.png
#                                                          (GPT's nine 32x32 icon controls, three states each)
# Writes Packaging/resources/oracool_assets/ui/levski_bg.png
#        Packaging/resources/oracool_assets/ui/levski_<stem>_{default,hover,pressed}.png  (27 files)
#        Source/oracool/levski_roar_skin.h  (window size, grid, ten rects - GENERATED, do not edit)
#
# WHY A HEADER. The window's hit rects and the art have to agree to the pixel, and the art is
# resampled from the painting. Measuring the painting here and emitting the numbers is the only way
# the two cannot drift: change the source numbers below, re-run, rebuild.
#
# THE THIRD LAYOUT (2026-09-05, "take levskis roar.png from oracool.mpq and take the icon sets chatgpt
# prepared and combine them into a working levski's UI [...] 3 rows of 3 icons (9 total) to the right of
# the grid"). The painting is the user's own: a 3x4 grid on the left, painted at a 29px pitch (26px
# interiors, 3px rules), and an empty stone field on the right. It is scaled UNIFORMLY by 28/29 so the
# painted cells become the game's 28px item cell - 3.4%, invisible on stonework. The nine controls are
# GPT's icon-controls-v3 pack: 32x32 plates including their bezel, DEFAULT / HOVER / CLICK, laid out
# 3x3 at a 35px pitch in the empty field, the block centred on the grid's height. The painting has no
# plates of its own for them, so the DEFAULT frame is drawn by the game at rest (levski_roar.cpp) and
# the other two over it - which is also why the state files are named per button rather than sliced
# out of the painting.
#
# The close button is the game's own red X (window_close.cpp), at the frame's top-right like every
# other window; the painting carries no close plate and GPT's grey X was not used.
#
# The two earlier paintings (1122x1402 with ten state sheets; 447x559 with an 8x10 grid) are in the
# oracool-levski-roar-skin pack; this script's history has their cuts (v1.9.203-205).

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$mpq = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ'
$source = Join-Path $mpq 'levskis roar.png'
$iconPack = Join-Path $mpq '02-source-art\delivered-packs\oracool-levski-icon-controls-v3\icons-32'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\levski_roar_skin.h'
foreach ($f in @($source, $iconPack)) { if (-not (Test-Path $f)) { throw "missing: $f" } }

# ---- measured off levskis roar.png (2026-09-05, luma/gold column and row profiles) ------------------
$srcW = 261; $srcH = 241
$gridColumns = 3; $gridRows = 4
$pitch = 29                              # centre-to-centre of the painted grid, both axes
$gridInterior = @{ X = 36; Y = 93 }      # top-left of the first cell's 26px interior
$gameCell = 28                           # the inventory cell (user, 2026-09-04: "regular game size")
$scale = $gameCell / $pitch
# The 28px game cell sits over the 26px interior plus one rule pixel each side, so the origin is one
# painted pixel up and left of the interior.
$gridOrigin = @{ X = $gridInterior.X - 1; Y = $gridInterior.Y - 1 }
# The empty field right of the grid, in painting pixels: x 134..241, y 79..219 (the inner opening).
$fieldX0 = 134; $fieldX1 = 241
$frameTop = 30                           # the frame's top edge at the sides (rows 0..29 are transparent there)

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

$winW = Scaled $srcW; $winH = Scaled $srcH
Write-Host ("window {0}x{1}  scale {2:N4}" -f $winW, $winH, $scale)

# ---- the background --------------------------------------------------------------------------
$bg = [System.Drawing.Image]::FromFile($source)
if ($bg.Width -ne $srcW -or $bg.Height -ne $srcH) { throw "expected ${srcW}x${srcH}, got $($bg.Width)x$($bg.Height)" }
$bgOut = Resample $bg (New-Object System.Drawing.Rectangle 0, 0, $srcW, $srcH) $winW $winH
$bgOut.Save((Join-Path $outDir 'levski_bg.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bgOut.Dispose(); $bg.Dispose()
Write-Host "levski_bg.png"

# ---- the nine controls: a 3x3 block in the field, centred on the grid's height ----------------------
$icon = 32; $iconPitch = 35
$blockW = $iconPitch * 2 + $icon
$gridTop = Scaled $gridOrigin.Y; $gridH = $gridRows * $gameCell
$fieldLeft = Scaled $fieldX0; $fieldRight = Scaled $fieldX1
$blockX = $fieldLeft + [int](($fieldRight - $fieldLeft - $blockW) / 2)
$blockY = $gridTop + [int](($gridH - $blockW) / 2)
if ($blockX + $blockW -gt $fieldRight) { throw "the 3x3 block does not fit the field" }

# Stem, pack id, tooltip - in ButtonIndex order after Close: Transmute, Recipes, then the seven
# salvage tiers in SalvageTier order (White, Magic, Rare, Unique, Primal, Set, Ethereal), which is
# also the pack's salvage-1..7 order. The BLOCK is read row-major in the pack's order: the seven
# tiers, then Transmute, then Recipes.
$controls = @(
  @{ Name = 'transmute'; Id = 'transmute'; Cell = 7 },
  @{ Name = 'recipes';   Id = 'recipes';   Cell = 8 },
  @{ Name = 'white';     Id = 'salvage-1'; Cell = 0 },
  @{ Name = 'magic';     Id = 'salvage-2'; Cell = 1 },
  @{ Name = 'rare';      Id = 'salvage-3'; Cell = 2 },
  @{ Name = 'unique';    Id = 'salvage-4'; Cell = 3 },
  @{ Name = 'primal';    Id = 'salvage-5'; Cell = 4 },
  @{ Name = 'set';       Id = 'salvage-6'; Cell = 5 },
  @{ Name = 'ethereal';  Id = 'salvage-7'; Cell = 6 }
)

# Stale state files from earlier layouts must not lie over the new plates.
Get-ChildItem $outDir -Filter 'levski_*_default.png' | Remove-Item
Get-ChildItem $outDir -Filter 'levski_*_hover.png' | Remove-Item
Get-ChildItem $outDir -Filter 'levski_*_pressed.png' | Remove-Item

$states = @{ default = 'default'; hover = 'hover'; click = 'pressed' }
$buttons = @()
# The close button first: the game's red X, inset by the frame width at the painting's top-right.
$closeSize = 18; $inset = 3
$buttons += @{ Name = 'close'; Rect = @(($winW - $inset - $closeSize), ((Scaled $frameTop) + $inset), $closeSize, $closeSize); Plain = $false }
foreach ($c in $controls) {
  $col = $c.Cell % 3; $row = [math]::Floor($c.Cell / 3)
  $buttons += @{ Name = $c.Name; Rect = @(($blockX + $col * $iconPitch), ($blockY + $row * $iconPitch), $icon, $icon); Plain = $false }
  foreach ($s in $states.Keys) {
    $src = Join-Path $iconPack ("{0}-{1}.png" -f $c.Id, $s)
    if (-not (Test-Path $src)) { throw "missing icon: $src" }
    $img = [System.Drawing.Image]::FromFile($src)
    if ($img.Width -ne $icon -or $img.Height -ne $icon) { throw "$src is $($img.Width)x$($img.Height), not ${icon}" }
    $img.Dispose()
    Copy-Item $src (Join-Path $outDir ("levski_{0}_{1}.png" -f $c.Name, $states[$s]))
  }
}

$lines = @()
foreach ($btn in $buttons) {
  $r = $btn.Rect
  $lines += ("`t{{ {{ {0}, {1} }}, {{ {2}, {3} }} }}, // {4}" -f $r[0], $r[1], $r[2], $r[3], $btn.Name)
  Write-Host ("{0,-10} rect {1},{2} {3}x{4}" -f $btn.Name, $r[0], $r[1], $r[2], $r[3])
}

# ---- the header ------------------------------------------------------------------------------
$h = @"
/**
 * @file oracool/levski_roar_skin.h
 *
 * GENERATED by tools/CutLevskiRoarSkin.ps1 - do not edit. Change the measurements at the top of
 * that script and re-run it; the art it writes and these numbers come from the same pass.
 *
 * The painted Levski's Roar window (Oracool.MPQ/levskis roar.png, the user's own 261x241 painting),
 * resampled to $($winW)x$($winH) - the scale at which its painted grid cells ($pitch px pitch) become the
 * game's 28px item cell. The nine controls are GPT's icon-controls-v3 pack, 32x32 plates in a 3x3
 * block right of the grid; they are drawn by the game in all three states from
 * ui\levski_<stem>_{default,hover,pressed}.png, since the painting carries no plates for them. Every
 * rect below is in WINDOW pixels.
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
constexpr Point GridOrigin { $(Scaled $gridOrigin.X), $(Scaled $gridOrigin.Y) };

/** The ten controls, in ButtonIndex order. */
enum ButtonIndex : int {
	Close = 0,
	Transmute = 1,
	Recipes = 2,
	SalvageFirst = 3, // White; the seven salvage tiers follow in SalvageTier order
};
constexpr int ButtonCount = $($buttons.Count);

constexpr Rectangle ButtonRects[ButtonCount] = {
$($lines -join "`n")
};

/** ui\ asset stem per button - levski_<stem>_default.png, _hover.png and _pressed.png. The close button has none. */
constexpr const char *ButtonStems[ButtonCount] = {
$(($buttons | ForEach-Object { "`t`"$($_.Name)`"," }) -join "`n")
};

/** True where the hover file, if any, is the PLAIN plate, so the game has to mark hover itself. */
constexpr bool HoverIsPlain[ButtonCount] = {
$(($buttons | ForEach-Object { "`t$($_.Plain.ToString().ToLower()), // $($_.Name)" }) -join "`n")
};

} // namespace devilution::oracool::levski_skin
"@
[System.IO.File]::WriteAllText($header, $h.Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote $header"

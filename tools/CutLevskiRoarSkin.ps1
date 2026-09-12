# CutLevskiRoarSkin.ps1 - cuts the painted Levski's Roar skin into shipped assets and a geometry header.
#
# Reads  Resources/levskis roar.png                       (the user's own painting, 320x352, controls painted in)
#        Resources/01-in-use-assets/delivered-packs/oracool-levski-icon-controls-v3/icons-3x/*.png
#                                                          (GPT's nine icon controls at 96x96, three states each)
# Writes Packaging/resources/oracool_assets/ui/levski_bg.png                 (the painting, 1:1)
#        Packaging/resources/oracool_assets/ui/levski_<stem>_{hover,pressed}.png  (18 files, at the painted sizes)
#        Source/oracool/levski_roar_skin.h  (window size, grid, ten rects - GENERATED, do not edit)
#
# WHY A HEADER. The window's hit rects and the art have to agree to the pixel. Measuring the painting
# here and emitting the numbers is the only way the two cannot drift: change the numbers below,
# re-run, rebuild.
#
# THE FOURTH LAYOUT (2026-09-05, "i made another levski UI window [...] grab it and apply it. also
# apply the necessary icons for hover and click functions"). The painting carries EVERYTHING at rest:
# the frame, the title, the SALVAGE stone plate, the 3x4 grid, the seven salvage gems and the recipe
# book on 36x36 plates in two columns, and the transmute button on a 60x60 plate under the grid. So
# it ships 1:1 - no resampling, which keeps the painted icons crisp and lets the state overlays land
# exactly on them - and the grid's 29px painted pitch is a header constant of its own (GridPitch)
# while the game's 28px item cell stays the cell. No default state file is written: the painting IS
# the default; the game lays HOVER or PRESSED over it, cut from the pack's 96px art down to the
# painted plate sizes.
#
# The close button is the game's red X (window_close.cpp) at the frame's top-right, like every other
# window; the painting has no close plate.
#
# Earlier layouts: the 261x241 painting with the 3x3 block (v1.9.253-267), the 447x559 8x10 and the
# 1122x1402 with state sheets (oracool-levski-roar-skin, v1.9.203-205). This script's history has
# their cuts.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$mpq = Join-Path (Split-Path -Parent $root) 'Resources\01-in-use-assets\working-files'
$source = Join-Path $mpq 'levskis roar.png'
$iconPack = Join-Path $mpq '01-in-use-assets\delivered-packs\oracool-levski-icon-controls-v3\icons-3x'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\levski_roar_skin.h'
foreach ($f in @($source, $iconPack)) { if (-not (Test-Path $f)) { throw "missing: $f" } }

# ---- measured off levskis roar.png (2026-09-05, luma/gold profiles and plate-rim bboxes) ---------
$srcW = 320; $srcH = 352
$gridColumns = 3; $gridRows = 4
$gridPitch = 29                          # centre-to-centre of the painted grid, both axes (26px interiors, 3px rules)
$gridInterior = @{ X = 46; Y = 112 }     # top-left of the first cell's 26px interior
$gameCell = 28                           # the inventory cell
# The 28px game cell sits over the 26px interior plus one rule pixel each side.
$gridOrigin = @{ X = $gridInterior.X - 1; Y = $gridInterior.Y - 1 }
$frameTop = 0                            # the frame runs to the painting's edge

# name, pack id, painted plate rect (x y w h) - the plate's outer rim. ButtonIndex order after Close:
# Transmute, Recipes, then the seven salvage tiers in SalvageTier order (White, Magic, Rare, Unique,
# Primal, Set, Ethereal) - the pack's salvage-1..7 and the painting's reading order (two columns).
$controls = @(
  @{ Name = 'transmute'; Id = 'transmute'; Rect = @(57, 251, 60, 60) },
  @{ Name = 'recipes';   Id = 'recipes';   Rect = @(229, 258, 36, 36) },
  @{ Name = 'white';     Id = 'salvage-1'; Rect = @(170, 138, 36, 36) },
  @{ Name = 'magic';     Id = 'salvage-2'; Rect = @(229, 138, 36, 36) },
  @{ Name = 'rare';      Id = 'salvage-3'; Rect = @(170, 178, 36, 36) },
  @{ Name = 'unique';    Id = 'salvage-4'; Rect = @(229, 178, 36, 36) },
  @{ Name = 'primal';    Id = 'salvage-5'; Rect = @(170, 218, 36, 36) },
  @{ Name = 'set';       Id = 'salvage-6'; Rect = @(229, 218, 36, 36) },
  @{ Name = 'ethereal';  Id = 'salvage-7'; Rect = @(170, 258, 36, 36) }
)

# ---- the background, 1:1 ----------------------------------------------------------------------
$bg = [System.Drawing.Image]::FromFile($source)
if ($bg.Width -ne $srcW -or $bg.Height -ne $srcH) { throw "expected ${srcW}x${srcH}, got $($bg.Width)x$($bg.Height)" }
$bgOut = New-Object System.Drawing.Bitmap $srcW, $srcH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bgOut)
$g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.DrawImage($bg, 0, 0, $srcW, $srcH)
$g.Dispose()
$bgOut.Save((Join-Path $outDir 'levski_bg.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bgOut.Dispose(); $bg.Dispose()
Write-Host ("levski_bg.png {0}x{1} (1:1)" -f $srcW, $srcH)

# ---- the state overlays: the pack's 96px art scaled to the painted plate ----------------------------
function Resample([System.Drawing.Image]$src, [int]$w, [int]$h) {
  $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $gr = [System.Drawing.Graphics]::FromImage($bmp)
  $gr.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $gr.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $gr.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $gr.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $gr.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
  $gr.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), (New-Object System.Drawing.Rectangle 0, 0, $src.Width, $src.Height), [System.Drawing.GraphicsUnit]::Pixel)
  $gr.Dispose()
  return $bmp
}

Get-ChildItem $outDir -Filter 'levski_*_default.png' | Remove-Item
Get-ChildItem $outDir -Filter 'levski_*_hover.png' | Remove-Item
Get-ChildItem $outDir -Filter 'levski_*_pressed.png' | Remove-Item

$states = @{ hover = 'hover'; click = 'pressed' }
$buttons = @()
$closeSize = 18; $inset = 3
$buttons += @{ Name = 'close'; Rect = @(($srcW - $inset - $closeSize), ($frameTop + $inset), $closeSize, $closeSize); Plain = $false }
foreach ($c in $controls) {
  $r = $c.Rect
  $buttons += @{ Name = $c.Name; Rect = $r; Plain = $false }
  foreach ($s in $states.Keys) {
    $src = Join-Path $iconPack ("{0}-{1}.png" -f $c.Id, $s)
    if (-not (Test-Path $src)) { throw "missing icon: $src" }
    $img = [System.Drawing.Image]::FromFile($src)
    $out = Resample $img $r[2] $r[3]
    $out.Save((Join-Path $outDir ("levski_{0}_{1}.png" -f $c.Name, $states[$s])), [System.Drawing.Imaging.ImageFormat]::Png)
    $out.Dispose(); $img.Dispose()
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
 * The painted Levski's Roar window (Resources/levskis roar.png, the user's own 320x352 painting),
 * shipped 1:1 with every control painted in at rest. The game lays ui\levski_<stem>_hover.png or
 * _pressed.png over a control while the cursor is on it or the press flash runs; there is no
 * default file, the painting is the default. The grid is painted at GridPitch; the game's item cell
 * is CellSize. Every rect below is in WINDOW pixels.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::levski_skin {

constexpr Size WindowSize { $srcW, $srcH };
constexpr int CellSize = $gameCell;
/** The painted grid's centre-to-centre pitch; the item cell sits inside it. */
constexpr int GridPitch = $gridPitch;
constexpr int GridColumns = $gridColumns;
constexpr int GridRows = $gridRows;
constexpr Point GridOrigin { $($gridOrigin.X), $($gridOrigin.Y) };
/** No code-drawn title: SALVAGE is painted on its stone plate. Zero-sized, and the window skips it. */
constexpr Rectangle SalvageTitleRect { { 0, 0 }, { 0, 0 } };

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

/** ui\ asset stem per button - levski_<stem>_hover.png and _pressed.png. The close button has none. */
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

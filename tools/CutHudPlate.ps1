# Oracool asset pipeline: cuts the bottom HUD - ui\middle_hud.png, ui\health_orb.png, ui\mana_orb.png -
# from ONE painted master, and writes the geometry header the layout is built from.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutHudPlate.ps1
#
# ## The fifth HUD (2026-09-05, user: "sweep oracool.mpq. take and use 03-transparent-slot-visual-draft")
#
# The GPT pack in Oracool.MPQ\02-source-art\delivered-packs\diablo-bottom-hud-v1. The file the user
# named is a 24-bit VISUAL DRAFT with a checkerboard painted in - no alpha channel at all - so the cut
# reads its sibling 04-raised-stone-wells-true-alpha-belt.png: the same design, same pixels, with the
# transparency real - and since the same evening transparent-orbs\03-transparent-belt-and-orbs.png, that
# again with the sphere interiors at alpha 0 and their colour kept in RGB: the LIQUID, which the game
# draws itself (user: "use the transparent orbs version and draw the liquid in code").
#
# One master, three files. The health cradle, the plate and the mana cradle are cut from a single
# resampled band at screen scale, at vertical lines, so the three rects the layout butts together
# reassemble to the painting exactly: the cradles' gold arches cross the cut lines and meet again on
# screen because every piece shares the one bottom edge and the one scale.
#
# THE SCALE is set by the belt: the six holes in the painting are ~87px, and a potion sprite is 28px,
# so 0.32 makes the holes 28 wide and the item fills its cell. Everything else follows: the band comes
# out 618x121, the wells' openings ~54px, the spheres radius 37.
#
# The belt cells are the HOLES - the painting's transparent openings - found by alpha here, so they
# cannot drift from the art. The wells' openings and the spheres are measured by hand (scratchpad
# hud5overlay.ps1, rects drawn over a 3x zoom) and written below.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\02-source-art\delivered-packs\diablo-bottom-hud-v1\designs'
$source = Join-Path $pack (Join-Path "transparent-orbs" "03-transparent-belt-and-orbs.png")
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\hud_plate_skin.h'
if (-not (Test-Path $source)) { throw "missing master: $source" }

# ---- measured off the 1942x809 master (2026-09-05) --------------------------------------------
$bandX = 4; $bandY = 215; $bandW = 1931; $bandH = 380   # the alpha>=128 bounding box
# THE SCALE. 0.32 would make the belt holes exactly a 28px potion sprite - and 122px-tall cradles,
# whose spheres' crowns then sit 11px above the line the side panels' content stops at (the stash's
# seventeen saved rows, both Abilities pages; ornate_border.h's SidePanelContentBottom = 624). At
# 0.288 the crowns clear that line and only the arches' tips cross it - scrollrt.cpp clips those
# while a side panel is open - and the plate comes out 352 wide, where the old 356 was. The holes
# are 25px: a potion sprite's own art is narrower than its 28px cell, so it still sits inside.
$scale = 0.288
# Vertical cut lines in MASTER space: where the cradles hand over to the plate.
$cutLeft = 352; $cutRight = 1576
# BAND-LOCAL master pixels (subtract the band origin from a master coordinate), scaled below:
$lmbWell = @(384, 181, 169, 166)      # x y w h - the dark stone inside the left well's rim
$rmbWell = @(1388, 184, 172, 166)
$healthCentre = @(219, 162); $manaCentre = @(1722, 162); $sphereRadius = 116

$img = New-Object System.Drawing.Bitmap $source
if ($img.Width -ne 1942 -or $img.Height -ne 809) { throw "master is $($img.Width)x$($img.Height), expected 1942x809" }
# assert the band
$minX=$img.Width;$maxX=-1;$minY=$img.Height;$maxY=-1
for ($y=0;$y -lt $img.Height;$y+=1){ for($x=0;$x -lt $img.Width;$x+=1){ if($img.GetPixel($x,$y).A -ge 128){ if($x -lt $minX){$minX=$x}; if($x -gt $maxX){$maxX=$x}; if($y -lt $minY){$minY=$y}; if($y -gt $maxY){$maxY=$y} } } }
if ($minX -ne $bandX -or $minY -ne $bandY -or ($maxX-$minX+1) -ne $bandW -or ($maxY-$minY+1) -ne $bandH) {
  throw "alpha band is $($maxX-$minX+1)x$($maxY-$minY+1) at ($minX,$minY), expected ${bandW}x${bandH} at ($bandX,$bandY) - re-measure before cutting"
}

$W = [int][math]::Round($bandW * $scale); $Hh = [int][math]::Round($bandH * $scale)
$s = New-Object System.Drawing.Bitmap $W, $Hh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($s)
$g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
$g.DrawImage($img, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $W, $Hh), (New-Object System.Drawing.Rectangle -ArgumentList $bandX, $bandY, $bandW, $bandH), 'Pixel')
$g.Dispose()
# Resampling feathers the edges; anything under alpha 8 is noise, and the game keys on >=128 anyway.
for ($y=0;$y -lt $Hh;$y++){ for($x=0;$x -lt $W;$x++){ $c=$s.GetPixel($x,$y); if ($c.A -lt 8) { $s.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0)) } } }
Write-Host ("composite {0}x{1}  scale {2}" -f $W, $Hh, $scale)

# ---- the belt holes, by alpha ---------------------------------------------------------------
$midY = [int]((461 + 556) / 2 - $bandY) * $scale
$runs=@(); $in=$false
for($x=0;$x -lt $W;$x++){ $o = $s.GetPixel($x,[int]$midY).A -ge 128; if(-not $o -and -not $in){$start=$x;$in=$true}; if($o -and $in){$runs += @{ X=$start; W=$x-$start };$in=$false} }
$xlPlate = [int][math]::Round($cutLeft * $scale); $xrPlate = [int][math]::Round($cutRight * $scale)
# Only the plate's span: with the transparent-orb master the empty spheres are runs of alpha 0 too.
$holes = @($runs | Where-Object { $_.W -gt 10 -and $_.W -lt 60 -and $_.X -ge $xlPlate -and ($_.X + $_.W) -le $xrPlate })
if ($holes.Count -ne 6) { throw "found $($holes.Count) belt holes, expected 6" }
$cx = [int]($holes[0].X + $holes[0].W / 2)
$vr=@(); $in=$false
for($y=0;$y -lt $Hh;$y++){ $o = $s.GetPixel($cx,$y).A -ge 128; if(-not $o -and -not $in){$start=$y;$in=$true}; if($o -and $in){$vr += @{ Y=$start; H=$y-$start };$in=$false} }
$hole = @($vr | Where-Object { $_.H -gt 10 -and $_.H -lt 60 })[0]
$cellW = ($holes | ForEach-Object { $_.W } | Measure-Object -Minimum).Minimum
Write-Host ("belt holes y {0} h {1}: {2}" -f $hole.Y, $hole.H, (($holes | ForEach-Object { "x$($_.X) w$($_.W)" }) -join ' '))

# ---- the three pieces -------------------------------------------------------------------------
$xl = [int][math]::Round($cutLeft * $scale); $xr = [int][math]::Round($cutRight * $scale)
function Scl($v) { return [int][math]::Round($v * $scale) }
function Piece($x0, $x1, $name) {
  $p = $s.Clone((New-Object System.Drawing.Rectangle -ArgumentList $x0, 0, ($x1 - $x0), $Hh), $s.PixelFormat)
  $p.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png); $p.Dispose()
  Write-Host ("{0,-16} x {1}..{2}  {3}x{4}" -f $name, $x0, $x1, ($x1 - $x0), $Hh)
}
Piece 0 $xl 'health_orb.png'
Piece $xl $xr 'middle_hud.png'
Piece $xr $W 'mana_orb.png'
# ---- the liquid (2026-09-05, user: "use the transparent orbs version and draw the liquid in code") -
# In this master the sphere interiors are alpha 0 with their colour kept in RGB. A resampler works
# premultiplied and would throw that colour away, so the liquid is lifted BEFORE resampling: a copy
# of the master with alpha forced to 255 inside each sphere's circle and 0 everywhere else, resampled
# on its own and cut to the orb piece. The game draws it under the frame from the fill line down.
$liq = New-Object System.Drawing.Bitmap $img.Width, $img.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$rr = ($sphereRadius + 3) * ($sphereRadius + 3)
for ($y=0;$y -lt $img.Height;$y++){ for($x=0;$x -lt $img.Width;$x++){
  $c = $img.GetPixel($x,$y); $bx = $x - $bandX; $by = $y - $bandY
  $dh = ($bx-$healthCentre[0])*($bx-$healthCentre[0]) + ($by-$healthCentre[1])*($by-$healthCentre[1])
  $dm = ($bx-$manaCentre[0])*($bx-$manaCentre[0]) + ($by-$manaCentre[1])*($by-$manaCentre[1])
  if (($dh -le $rr -or $dm -le $rr) -and $c.A -lt 128) { $liq.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$c.R,$c.G,$c.B)) }
  else { $liq.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0)) } } }
$ls = New-Object System.Drawing.Bitmap $W, $Hh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($ls)
$g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
$g.DrawImage($liq, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $W, $Hh), (New-Object System.Drawing.Rectangle -ArgumentList $bandX, $bandY, $bandW, $bandH), 'Pixel')
$g.Dispose(); $liq.Dispose()
for ($y=0;$y -lt $Hh;$y++){ for($x=0;$x -lt $W;$x++){ $c=$ls.GetPixel($x,$y); if ($c.A -lt 128) { $ls.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0)) } else { $ls.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$c.R,$c.G,$c.B)) } } }
function LiquidPiece($x0, $x1, $name) {
  $p = $ls.Clone((New-Object System.Drawing.Rectangle -ArgumentList $x0, 0, ($x1 - $x0), $Hh), $ls.PixelFormat)
  $p.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png); $p.Dispose()
  Write-Host ("{0,-22} x {1}..{2}" -f $name, $x0, $x1)
}
LiquidPiece 0 $xl 'health_orb_liquid.png'
LiquidPiece $xr $W 'mana_orb_liquid.png'
$ls.Dispose()

$s.Dispose(); $img.Dispose()

# ---- the header -------------------------------------------------------------------------------
$plateW = $xr - $xl
$cells = ($holes | ForEach-Object { $_.X - $xl }) -join ', '
$h = @"
/**
 * @file oracool/hud_plate_skin.h
 *
 * GENERATED by tools/CutHudPlate.ps1 - do not edit. Change the measurements at the top of that
 * script and re-run it; the three PNGs it writes and these numbers come from the same pass.
 *
 * The fifth bottom HUD (Oracool.MPQ/02-source-art/delivered-packs/diablo-bottom-hud-v1, cut from
 * 04-raised-stone-wells-true-alpha-belt.png at scale $scale). Every number is in SCREEN pixels; the
 * plate's rects are PLATE-local, the orbs' are local to their own piece. The three pieces share one
 * bottom edge and butt together left to right: health cradle, plate, mana cradle.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::hud_skin {

constexpr Size PlateSize { $plateW, $Hh };

/** The wells' openings - the flat stone inside the rim - plate-local. */
constexpr Rectangle LmbWell { { $((Scl $lmbWell[0]) - $xl), $(Scl $lmbWell[1]) }, { $(Scl $lmbWell[2]), $(Scl $lmbWell[3]) } };
constexpr Rectangle RmbWell { { $((Scl $rmbWell[0]) - $xl), $(Scl $rmbWell[1]) }, { $(Scl $rmbWell[2]), $(Scl $rmbWell[3]) } };

/** The six belt cells are the painting's transparent holes, found by alpha. */
constexpr int BeltCellX[6] = { $cells };
constexpr int BeltCellY = $($hole.Y);
constexpr Size BeltCellSize { $cellW, $($hole.H) };

constexpr Size HealthOrbSize { $xl, $Hh };
constexpr Size ManaOrbSize { $($W - $xr), $Hh };
constexpr Point HealthSphereCenter { $(Scl $healthCentre[0]), $(Scl $healthCentre[1]) };
constexpr Point ManaSphereCenter { $((Scl $manaCentre[0]) - $xr), $(Scl $manaCentre[1]) };
constexpr int SphereRadius = $(Scl $sphereRadius);

} // namespace devilution::oracool::hud_skin
"@
[System.IO.File]::WriteAllText($header, $h.Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote $header"
